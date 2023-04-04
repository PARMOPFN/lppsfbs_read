#include "NetDevice.hpp"

#include <string>
#include <stdexcept>
#include <string>
#include <future>
#include <unistd.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <cstring>

//#define _NET_DEVICE_DEBUG

#ifdef _NET_DEVICE_DEBUG
#define _debug(X) std::cerr << "Debug: "<<__FILE__ << ": "<<  __LINE__ <<" > "<< X << " < " << "\n"
#else
#define _debug(X)
#endif

namespace net {

NetDevice::NetDevice(const std::string& name) :
        _name(name),
        _host(""),
        _port(0),
        _sockfd(0),
        _buffer(INIT_BUF_LENGTH),
        stubbed(true) {
}

const std::string NetDevice::getName() {
    return (_name);
}
const std::string NetDevice::getHostName() {
    return (_host);
}
NetDevice::~NetDevice() {
    disconnect();
}

void NetDevice::connect(const std::string& host, int port, int timeout, bool _blocking) throw(std::exception) {
    if (stubbed) {
        std::cerr << "The " << _name << " is in STUBBED mode, can't connect to " << host << ":" << port << std::endl;
        return;
    }
    if (_sockfd) {
        std::cerr<<_name <<" connect failed : equipment is already connected. Disconnect first"<<std::endl;
       // throw std::runtime_error((_name + " connect failed : equipment is already connected. Disconnect first"));
    }

    // store host address
    _host = host;
    _port = port;
    /*
     std::unique_lock<std::mutex> rx_lock(_rx_mtx,std::defer_lock);
     if (rx_lock.try_lock()) {}
     else
     _debug("netdevice::connect try_rx_lock fail!");



     std::unique_lock<std::mutex> tx_lock(_tx_mtx,std::defer_lock);
     if (tx_lock.try_lock()) {}
     else
     _debug("netdevice::connect try_tx_lock fail!");
     */

    // create socket
    if (!(_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
        stubbed = true;
        throw std::runtime_error((_name + " connect failed : cannot create client socket, error: " + std::to_string(errno)));
    }

    const struct sockaddr_in address =
    {
    AF_INET,
      htons(_port),
      { inet_addr(_host.c_str()) } };

    // connect to host
    if (::connect(_sockfd, reinterpret_cast<const struct sockaddr*>(&address), sizeof(address)) != 0) {
        stubbed = true;
        throw std::runtime_error((_name + " connect failed : cannot connect to " + _host + ":" + std::to_string(port) + ", error: " + std::to_string(errno)));
    }

    if (_blocking == false) {
        //Set non block
        fcntl(_sockfd, F_SETFL, O_NONBLOCK);
        //TODO:Check the keepalive config is neccesary for nb socket
    }

    int optval = 1;
    //enable keepalive
    setsockopt(_sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
    struct KeepConfig cfg =
    { 1, 1, 1 };

    setsockopt(_sockfd, IPPROTO_TCP, TCP_KEEPCNT, &cfg.keepcnt, sizeof cfg.keepcnt);
    setsockopt(_sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &cfg.keepidle, sizeof cfg.keepidle);
    setsockopt(_sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &cfg.keepintvl, sizeof cfg.keepintvl);

    //set timeout

    if (timeout) {
        struct timeval tv;
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        setsockopt(_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    }

    // test if connection is alive
    if (!isConnected()) {
        stubbed = true;
        throw std::runtime_error((_name + " connect failed : connection is not alive: " + std::to_string(errno)));
    }

    setBlocking(_blocking);
}


void NetDevice::setBlocking(bool _blocking) {
    if (_blocking == false)
        fcntl(_sockfd, F_SETFL, O_NONBLOCK);

    else  {
        auto val = fcntl(_sockfd, F_GETFL, 0);
        fcntl(_sockfd,F_SETFL,val &=~O_NONBLOCK);
    }
    blocking = _blocking;

}

void NetDevice::reconnect() {
    if (stubbed) return;

    _debug("Device: "<<_name<<" reconnecting...");
    disconnect();
    connect(_host, _port);
}

void NetDevice::disconnect() {
    if (stubbed) return;

    if (_sockfd) {
        _debug("Trying to close socket: "<<_name);

        std::unique_lock<std::mutex> rx_lock(_rx_mtx, std::defer_lock);
        if (rx_lock.try_lock()) {
        }
        else _debug("netdevice::disconnect try_rx_lock fail!");

        std::unique_lock<std::mutex> tx_lock(_tx_mtx, std::defer_lock);
        if (tx_lock.try_lock()) {
        }
        else _debug("netdevice::disconnect try_tx_lock fail!");

        ::close(_sockfd);
        _sockfd = 0;
    }
}

bool NetDevice::isConnected() {
    if (stubbed) return false;
    int errorCode = -1;
    _debug("isConnected? ");
    socklen_t errorCodeSize = sizeof(errorCode);
    if (getsockopt(_sockfd, SOL_SOCKET, SO_ERROR, &errorCode, &errorCodeSize) != 0) return (false);
    return (true);
}

void NetDevice::print_debug(const std::vector<uint8_t> buf) {
    for (auto& el : buf) {
        std::cout << std::setfill('0') << std::setw(2) << std::hex << (0xff & (unsigned int)el) << " ";
    }
    std::cout << std::endl;
}

int NetDevice::sendQuery(const uint8_t* cmd, const uint32_t size, const bool waitReceive) throw(std::exception) {

    //lock inside
    try {
        transmit(cmd, size);
    }
    catch (const std::exception &e) {
        throw;
    }

    // read response
    int bytesReceived = 0;

    if (waitReceive == false) {
        // detach a thread to read answer, lock inside receive
        std::async(std::launch::async, &NetDevice::receive, this);
    }
    else {
        bytesReceived = receive();
    }_debug("from device: "<<_name<<" received "<<std::dec<<bytesReceived<<" bytes");
    //   print_debug(_buffer);

    return (bytesReceived);
}

void NetDevice::sendQueryNoResponse(const uint8_t* cmd, const uint32_t size) throw(std::exception) {
    //std::cout<<"Transmit: LPPS: STUBBED?: "<<isStubbed()<<", connected?: "<<isConnected()<<std::endl;

    transmit(cmd, size);
}

ssize_t NetDevice::receive() {
    if (stubbed) {
        std::cerr << " Warning, device: " << _name << " is already in stub mode, command can't be proceed" << std::endl;
        return 0;
    }

    if (!isConnected()) {
        stubbed = true;
        throw std::runtime_error(std::string(_name + ", read Query failed : not connected"));
    }
    std::unique_lock<std::mutex> rx_lock(_rx_mtx, std::defer_lock);
    if (rx_lock.try_lock()) {
    }
    else _debug("netdevice::receive try_rx_lock fail!");

    ssize_t bytesReceived = 0;
    // receive response
    if ((bytesReceived = recv(_sockfd, _buffer.data(), _buffer.size(), 0)) < 0) {
        if (errno != EAGAIN) {
            _debug("netdevice::EAGAIN");
            std::cerr << "bufsize:" << _buffer.size() << std::endl;
            throw std::runtime_error((_name + ", read Query failed : cannot recv data, error: " + std::to_string(errno)));
        }
    }

    return (bytesReceived);
}

size_t NetDevice::receiveNB(size_t write_index) {

    if (stubbed) {
        std::cerr << " Warning, device: " << _name << " is already in stub mode, command can't be proceed" << std::endl;
        return 0;
    }

    if (!isConnected()) {
        stubbed = true;
        throw std::runtime_error(std::string(_name + ", read failed : not connected"));
    }

    ssize_t bytes_read = 0;

         if ((bytes_read = recv(_sockfd, _nbbuffer.begin() + write_index, _nbbuffer.size() - write_index, MSG_DONTWAIT)) > 0) {
             write_index += bytes_read;
            // std::cout<<"br:"<<bytes_read<<std::endl;
            }
        else {
            // no data, posibly socket error
            if ((errno != EAGAIN) && ( errno != EWOULDBLOCK))// error
            throw std::runtime_error((_name + ", recv failed : cannot read data, error: " + std::to_string(errno)));
        }

         /*
          * Assumption - if no new packed, the stored fragment is not needed
          */
        // if (!bytes_read) write_index = 0;
    return (write_index);
}

const std::vector<uint8_t>& NetDevice::getBuffer() {
    return _buffer;
}

 NetBuffer* NetDevice::getNBBuffer() {
    return &_nbbuffer;
}

void NetDevice::clearNBBuffer() {
    std::fill(std::begin(_nbbuffer), std::end(_nbbuffer), 0);
}

ssize_t NetDevice::transmit(const uint8_t* cmd, const uint32_t size) {
    if (stubbed) {
        std::cerr << " Warning, device: " << _name << " is already in stub mode, command can't be proceed" << std::endl;
        return 0;
    }

    if (!isConnected()) {
        stubbed = true;
        throw std::runtime_error(std::string(_name + ", sendQuery failed : not connected"));
    }

    // send query
    _debug("Netdevice::"<<_name<<" SendQuery ");//; print_debug(std::vector<uint8_t>(cmd, cmd + size));

    std::unique_lock<std::mutex> tx_lock(_tx_mtx, std::defer_lock);
    if (tx_lock.try_lock()) {
    }
    else _debug("netdevice::connect try_tx_lock fail!");

    size_t bytesSend = 0;
    if ((bytesSend = static_cast<std::size_t>(send(_sockfd, cmd, size, 0))) != size) {
        throw std::runtime_error((_name + ", sendQuery failed : cannot send query, error: " + std::to_string(errno)));
    }

    _debug("Device: "<<_name<<" send finish");
    return (bytesSend);
}

}// namespace net
