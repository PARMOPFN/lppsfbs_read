#ifndef __NETDEVICES_HPP
#define __NETDEVICES_HPP

#include <string>
#include <vector>
#include <mutex>
#include <iostream>
#include <iomanip>



namespace net {


constexpr std::size_t INIT_BUF_LENGTH = 512u;
//800 bytes = 20 FBS frames (40bytes each)
constexpr ssize_t MAX_PACKET_LENGTH = 8000;





static const std::string NEWLINE = "\r\n";
using NetBuffer = std::array<uint8_t,MAX_PACKET_LENGTH>;


//based on https://stackoverflow.com/users/126769/nos
struct KeepConfig {
    /** The time (in seconds) the connection needs to remain
     * idle before TCP starts sending keepalive probes (TCP_KEEPIDLE socket option)
     */
    int keepidle;
    /** The maximum number of keepalive probes TCP should
     * send before dropping the connection. (TCP_KEEPCNT socket option)
     */
    int keepcnt;

    /** The time (in seconds) between individual keepalive probes.
     *  (TCP_KEEPINTVL socket option)
     */
    int keepintvl;
};

class NetDevice {
public:
    NetDevice(const std::string& name);

    virtual ~NetDevice();

    // establish connection with network device
    void connect(const std::string& hostname, int port, int timeout = 0, bool blocking = true) throw(std::exception);
    void setBlocking(bool _blocking);
    // re-establish connection
    void reconnect();

    // disconnect from network device
    void disconnect();

    // return connection status
    bool isConnected();

    // Helper functions to retur private values
    const std::string getName();
    const std::string getHostName();

    inline bool isStubbed() { return (stubbed);}
    inline void setStubbed(bool stubbed_) { stubbed = stubbed_;}

    // send command to network device then store recieived data in _buffer, return read bytes lenght
    int sendQuery(const uint8_t* cmd, const uint32_t size, const bool waitReceive) throw (std::exception);

    // send command to network device
    void sendQueryNoResponse(const uint8_t* cmd, const uint32_t size) throw (std::exception);

    ssize_t receive();
    /*
     *
     * the second fn is for non blocking, to keep compatibility with past developed code
     * parameter writeIndex - sockrcv will keep the last load packet from this place
     *
     */


    size_t receiveNB(size_t writeIndex =0);

    //to keep the _buffer private
    const std::vector<uint8_t>& getBuffer();

    //second function to avoid possible or impossible impact with previous code
    //returns pointer. Remember, the lifetime is limited to the NetDevice object
    NetBuffer *getNBBuffer();
    void clearNBBuffer();


protected:
    //send query frame
    ssize_t transmit(const uint8_t* cmd, const uint32_t size);

    // debug print
    void print_debug(const std::vector<uint8_t> buf);

    template <std::size_t _Nm>
    void print_debug(const std::array<uint8_t, _Nm> buf) {
        for (auto& el : buf) {
               std::cout << std::setfill('0') << std::setw(2) << std::hex << (0xff & (unsigned int)el) << " ";
           }
           std::cout << std::endl;
    }

protected:
    // device name
    std::string _name;
    // device hostname
    std::string _host;
    // device port
    int _port;

    // socket handler
    int _sockfd;

    // mutex to protect recv
    std::mutex _rx_mtx;
    std::mutex _tx_mtx;
    // read buffer
    std::vector<uint8_t> _buffer;
    //raed buffer for nb
    std::array<uint8_t, MAX_PACKET_LENGTH> _nbbuffer;

    // stubbed
    bool stubbed;
    bool blocking;

};

} // namespace net

#endif //__NETDEVICES_HPP
