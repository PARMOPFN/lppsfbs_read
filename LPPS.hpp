/*
 * teststation.hpp
 *
 *  Created on: Jun 18, 2022
 *      Author: kafie
 */

#ifndef SRC_PISA_NETDEVICES_Lpps_RECEIVER_HPP_
#define SRC_PISA_NETDEVICES_Lpps_RECEIVER_HPP_

#include <string>
#include <vector>
#include <array>

#include "NetDevice.hpp"
#include "pisa/utils/utils.hpp"
#include <memory>

namespace lpps_receiver {

constexpr size_t IDN_ACK_SIZE = 29; //
constexpr uint8_t NET_ERROR = 0x01;

/*
 * REMEMBER LITTLE ENDIAN!!
       1                  5            12                   20          24                   32
*  0   |   1 - 5          |      7      |   8 bytes          |    4     |        8           |
* ver  |     magic nb     |     PAD     |    NTP PPS TS      |  ERROR   | NTP DATA timestamp |
* 0x01 | 'L' 'P' 'P' 'S'  | x x x x x x | x x x x x x x x    |  x x x x | x x x x x x x x    |
Errors
//no set  == no error
b0 - bit timeout
b1 - no data
b2 - no clk
b3 - no pps
b4 - invalid PPS
b5
b6
b7
*/


enum class lpps_channels : std::size_t {
        CHANNEL_1 = 0u,
        CHANNEL_2,
};

#pragma pack(push, 1)
struct lpps_frame {
        uint64_t header;
        uint32_t pad;
        uint32_t lpps_data; // lpps data
        uint32_t frame_delay_pru_cycle; //in PRU CYCLES - 5ns
        uint32_t errors;
        uint64_t data_timestamp_ntp;// ntp_timestamp of first data rising edge
        uint64_t pps_timestamp_ntp; //  ntp_timestamp of rising edge of PPS pulse
};
#pragma pack(pop)

constexpr ssize_t LPPS_FRAME_LEN = sizeof(lpps_frame);

class LppsReceiver  {
    public:

        LppsReceiver(std::string name = "");
        ~LppsReceiver() = default;
        /*
         * @brieff establish connection with Lpps receiver
         * @param  hostname hostname (string)
         * @param main_port port number for management
         * @param data_port1,2 ports for data , channel1, channel2, read only
         */
       void connect(const std::string& hostname, int main_port) throw(std::exception);
       void connect_channel(const std::string& hostname, lpps_channels channel, int data_port ) throw(std::exception);
       std::string sendIdnQuery() throw (std::exception);
       void sendAcq(bool activate, lpps_channels channel);
        std::size_t receiveLppsFrames(std::vector<const lpps_frame*>& pframes, lpps_channels channel, uint8_t& errors);
        void purgeSocket(lpps_channels channel);

        /*
         * Send query for ACQ status, don't wait for answer, return errors in case of problems (with connection, usually)
         */
        uint8_t queryAcqAsync();
        uint8_t readAcqAsync(std::pair<bool, bool>& acq);
        bool async_task;



    private:
        std::shared_ptr<net::NetDevice> _main_socket;
        utils::enum_array<lpps_channels, std::shared_ptr<net::NetDevice>,2> _data_socket;
        std::string name;
        //remaining data from previous packet - len, position in packet
        size_t rem_data_len;
        size_t rem_data_start;

};//class


}



#endif /* SRC_PISA_NETDEVICES_Lpps_RECEIVER_HPP_ */
