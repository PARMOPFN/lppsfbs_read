/*
 * teststation.hpp
 *
 *  Created on: Jun 18, 2022
 *      Author: kafie
 */

#include "FBS.hpp"
#include <memory>
#include <sstream>
#include <algorithm>
#include <vector>

namespace fbs_receiver {

FbsReceiver::FbsReceiver(std::string _name) :
        name(_name) {
    //Initialize
    rem_data_len = 0;
    rem_data_start = 0;

    _main_socket = std::make_shared<net::NetDevice>(_name + "_main");
    _data_socket[fbs_channels::CHANNEL_1] = std::make_shared<net::NetDevice>(_name + "_data1");
    _data_socket[fbs_channels::CHANNEL_2] = std::make_shared<net::NetDevice>(_name + "_data2");

}

std::string FbsReceiver::sendIdnQuery() throw(std::exception) {
    std::string query = "*IDN?" + net::NEWLINE;
    const uint32_t bytesReceived = _main_socket->sendQuery(reinterpret_cast<const uint8_t*>(query.data()), query.size(), true);

    if ((bytesReceived < IDN_ACK_SIZE) || (!(bytesReceived))) {
        throw std::runtime_error((name + ", sendQuery failed : invalid Acknowledge packet"));
    }
    const std::vector<uint8_t>& vec = _main_socket->getBuffer();
    return std::string(vec.begin(), vec.end());
}

void FbsReceiver::sendAcq(bool activate, fbs_channels channel) {
    std::string query = "ACQ " + std::string(activate ? "1," : "0,") + std::to_string(static_cast<uint8_t>(channel) + 1) + net::NEWLINE;
   // std::cout << this->name << " Set FBS Acq status:<" << query << ">" << std::endl;

    _main_socket->sendQueryNoResponse(reinterpret_cast<const uint8_t*>(query.data()), query.size());
}


uint8_t FbsReceiver::queryAcqAsync() {

    if (!_main_socket->isConnected() || _main_socket->isStubbed())
             return NET_ERROR;

    std::string query = ":ACQ?" + net::NEWLINE;
    std::cout << this->name << " Query FBS ACQ status:" << std::endl;
    size_t bytes_send = 0;
    try {
         _main_socket->sendQueryNoResponse(reinterpret_cast<const uint8_t*>(query.data()), query.size());
    }   catch(std::exception& e) {
         return  NET_ERROR; //problem with connection
    }
    if (bytes_send == 0) return NET_ERROR;
     return 0;
}
uint8_t FbsReceiver::readAcqAsync(std::pair<bool, bool>& acq) {
    size_t bytes_read =  _main_socket->receiveNB(0);
    auto data = _main_socket->getNBBuffer();

    std::cout << "FBS " << name << " answer: <" << std::string(data->begin(), data->end()) << "> size:" << data->size() << std::endl;

    if ((bytes_read != 4)) { //0,0\n
        std::cout << name << " Acq query answer fail" << std::endl;
        acq = std::make_pair(false, false);
        return NET_ERROR;
    }
    acq = std::make_pair(((*data)[0] == '1'), ((*data)[2] == '1'));
    return 0;
}

void FbsReceiver::connect(const std::string& hostname, int main_port) throw(std::exception) {
    _main_socket->setStubbed(false);
    _main_socket->connect(hostname, main_port, 5);// 5 sec timeout
    //set nonblocking to ask periodically rec about health
    _main_socket->setBlocking(false);
}

void FbsReceiver::connect_channel(const std::string& hostname, fbs_channels channel, int data_port) throw(std::exception) {
    _data_socket[channel]->setStubbed(false);
    _data_socket[channel]->connect(hostname, data_port, false);//false = non blocking
}

void FbsReceiver::purgeSocket(fbs_channels channel) {
    if (_data_socket[channel]->isStubbed()) return;
    _data_socket[channel]->receiveNB(0);
    _data_socket[channel]->clearNBBuffer();
    rem_data_start = 0;
    rem_data_len = 0;
}


std::size_t FbsReceiver::receiveFbsFrames(std::vector<const uint8_t*>& frames, fbs_channels channel, uint8_t& errors) {

    frames.clear();
    if (_data_socket[channel]->isStubbed()) return 0;

    /*
     write_start  - place where recv will start writing new data
     write_end - place where recv stop writing new data
     write_start - write_end = amount of received data
     */

    /*
     When in past recv we received N bytes and only X<N bytes are frames, remaining data starts from X, and has len = N-X
     first: copy reamining data from X to N  begining of the buffer.
     This will no impact on the frames received before because they are copied in the previous cycle. Hope :>
     */
    // we have to know how many data we received last cycle
    // and where the remaining data starts
    auto data = _data_socket[channel]->getNBBuffer();

    std::copy_n((*data).begin() + rem_data_start, rem_data_len, (*data).begin());
    size_t write_start = rem_data_len;

    size_t write_end =  _data_socket[channel]->receiveNB(write_start);
    size_t nframes = 0;


    if (write_end - write_start == 0) {
        write_start = 0;
        return 0;
    }

    // Analyze received buffer from start to write_end;
    for (size_t i = 0; i < write_end;) {
        // if below, there's no enough space to keep valid frame.
        if ((write_end - i) < REC_FRAME_LEN) {
            rem_data_len = write_end - i;
            rem_data_start = i;
            //std::cout << "remaining data start: " << rem_data_start << std::endl;
            //std::cout << "remaining data len: " << rem_data_len << std::endl;

            errors = 1;
            return nframes;
        }
        /*
         std::cout << std::dec << "index:" << i << " [";
         for (size_t n = 0; n < REC_FRAME_LEN; n++) {
         std::cout << std::setfill('0') << std::setw(2) << std::hex << (0xff & (unsigned int)(*data)[i + n]) << " ";
         }
         std::cout << std::dec << "]" << std::endl;
         */

        // shift from start find header
        if (((*data)[i] == 0x01) && ((*data)[i + 1] == 'F') && ((*data)[i + 2] == 'B') && ((*data)[i + 3] == 'U')) {
            nframes++;
            frames.push_back(&(*data)[i + 4]);
            i += REC_FRAME_LEN;
        }
        // if no header, move one byte
        else i++;
    }// for
    //std::cout << "No remaining data" << std::endl;
    return nframes;
    errors = 0;
}

}// & fbs_receiver

