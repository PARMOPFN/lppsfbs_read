/*
 * teststation.hpp
 *
 *  Created on: Jun 18, 2022
 *      Author: kafie
 */

#ifndef SRC_PISA_NETDEVICES_FBS_RECEIVER_HPP_
#define SRC_PISA_NETDEVICES_FBS_RECEIVER_HPP_

#include <string>
#include <vector>
#include <array>

#include "NetDevice.hpp"
#include "pisa/utils/utils.hpp"
#include <memory>

namespace fbs_receiver {

constexpr size_t IDN_ACK_SIZE = 29; //Astri Polska,123456,789,10.11
constexpr ssize_t REC_FRAME_LEN = 40u; //320 bits, 4bytes + FBS_FRAME_LEN
constexpr ssize_t FBS_FRAME_LEN = 36u; // Bytes, 224 bits frame + 64 bits NTP
constexpr uint8_t NET_ERROR = 0x01;

//constexpr int FBS_MAIN_PORT = 5025;
//constexpr int FBS_CHANNEL1_PORT = 5031;
//constexpr int FBS_CHANNEL2_PORT = 5032;


/*
 * REMEMBER LITTLE ENDIAN!!
* ver  |  magic nb   |   TC1   |   PAD   |   PAD   |   TC2   |   PAD   |   PAD   |   TC3   |   NTP timestamp
* 0x01 | 'F' 'B' 'U' | x x x x | x x x x | x x x x | x x x x | x x x x | x x x x | x x x x | x x x x x x x x
*/

enum class fbs_channels : std::size_t {
        CHANNEL_1 = 0u,
        CHANNEL_2,
};

class FbsReceiver  {
    public:
        FbsReceiver(std::string name = "");
        ~FbsReceiver() = default;
        /*
         * @brieff establish connection with FBS receiver
         * @param  hostname hostname (string)
         * @param main_port port number for management
         * @param data_port1,2 ports for data , channel1, channel2, read only
         */
       void connect(const std::string& hostname, int main_port) throw(std::exception);
       void connect_channel(const std::string& hostname, fbs_channels channel, int data_port ) throw(std::exception);
       std::string sendIdnQuery() throw (std::exception);
       void sendAcq(bool activate, fbs_channels channel);
       /*
        * Send query for ACQ status, don't wait for answer, return errors in case of problems (with connection, usually)
        */
       uint8_t queryAcqAsync();
       uint8_t readAcqAsync(std::pair<bool, bool>& acq);


       /*
       @brief - splitting buffer from recv into frames
       @param frames  - vector witch pointers of finded frames
       @param socket - "pseudo" socket
       @param errors -  0 - no errors, 1 - fragmented data waining for future analyse
       */
        std::size_t receiveFbsFrames(std::vector<const uint8_t*>& frames, fbs_channels channel, uint8_t& errors);
        void purgeSocket(fbs_channels channel);

    private:
        std::shared_ptr<net::NetDevice> _main_socket;
        utils::enum_array<fbs_channels, std::shared_ptr<net::NetDevice>,2> _data_socket;
        std::string name;
        //remaining data from previous packet - len, position in packet
        size_t rem_data_len;
        size_t rem_data_start;


};//class


}



#endif /* SRC_PISA_NETDEVICES_FBS_RECEIVER_HPP_ */
