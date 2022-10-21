#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <algorithm>
#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    const auto iter = _arp_table.find(next_hop_ip);
    if (iter != _arp_table.end()) {
        EthernetFrame frame;
        frame.header() = {/* dst  */ iter->second.eth_addr,
                          /* src  */ _ethernet_address,
                          /* type */ EthernetHeader::TYPE_IPv4};
        frame.payload() = dgram.serialize();
        _frames_out.push(std::move(frame));
    } else {
        auto dgrams_iter = _dgrams_info_waiting_for_arp_reply.find(next_hop_ip);
        if (dgrams_iter == _dgrams_info_waiting_for_arp_reply.end()) {
            send_arp_request(next_hop_ip);
            _dgrams_info_waiting_for_arp_reply[next_hop_ip].dgrams.push_back(dgram);
        } else {
            dgrams_iter->second.dgrams.push_back(dgram);
        }
    }
}

void NetworkInterface::send_arp_request(const uint32_t next_hop_ip) {
    ARPMessage message;
    message.opcode = ARPMessage::OPCODE_REQUEST;
    message.sender_ethernet_address = _ethernet_address;
    message.sender_ip_address = _ip_address.ipv4_numeric();
    message.target_ip_address = next_hop_ip;
    EthernetFrame frame;
    frame.header() = {/* dst  */ ETHERNET_BROADCAST,
                      /* src  */ _ethernet_address,
                      /* type */ EthernetHeader::TYPE_ARP};
    frame.payload() = message.serialize();
    _frames_out.push(std::move(frame));
}

//! \param[in] frame the incoming Ethernet frame, rfc::rfc826
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) {
        return nullopt;
    }
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram result;
        if (result.parse(frame.payload()) != ParseResult::NoError) {
            return nullopt;
        }
        return result;
    }
    ARPMessage message;
    if (message.parse(frame.payload()) != ParseResult::NoError) {
        return nullopt;
    }
    auto iter = _arp_table.find(message.sender_ip_address);
    bool merge_flag = false;
    if (iter != _arp_table.end()) {
        iter->second = ARPEntry(message.sender_ethernet_address);
        merge_flag = true;
    }
    if (message.target_ip_address == _ip_address.ipv4_numeric()) {
        if (merge_flag == false) {
            _arp_table[message.sender_ip_address] = ARPEntry(message.sender_ethernet_address);
            auto dgrams_iter = _dgrams_info_waiting_for_arp_reply.find(message.sender_ip_address);
            if (dgrams_iter != _dgrams_info_waiting_for_arp_reply.end()) {
                for (const auto &dgram : dgrams_iter->second.dgrams) {
                    EthernetFrame temp_frame;
                    temp_frame.header() = {/* dst  */ message.sender_ethernet_address,
                                           /* src  */ _ethernet_address,
                                           /* type */ EthernetHeader::TYPE_IPv4};
                    temp_frame.payload() = dgram.serialize();
                    _frames_out.push(std::move(temp_frame));
                }
                _dgrams_info_waiting_for_arp_reply.erase(dgrams_iter);
            }
        }
        if (message.opcode == ARPMessage::OPCODE_REQUEST) {
            message.opcode = ARPMessage::OPCODE_REPLY;
            std::swap(message.sender_ip_address, message.target_ip_address);
            message.target_ethernet_address = message.sender_ethernet_address;
            message.sender_ethernet_address = _ethernet_address;
            EthernetFrame temp_frame;
            temp_frame.header() = {/* dst  */ message.target_ethernet_address,
                                   /* src  */ _ethernet_address,
                                   /* type */ EthernetHeader::TYPE_ARP};
            temp_frame.payload() = message.serialize();
            _frames_out.push(std::move(temp_frame));
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    auto iter = _arp_table.begin();
    while (iter != _arp_table.end()) {
        if (iter->second.time_ms_to_delete <= ms_since_last_tick) {
            _arp_table.erase(iter++);
        } else {
            iter->second.time_ms_to_delete -= ms_since_last_tick;
            iter++;
        }
    }
    auto iter2 = _dgrams_info_waiting_for_arp_reply.begin();
    while (iter2 != _dgrams_info_waiting_for_arp_reply.end()) {
        if (iter2->second.time_ms_to_retry <= ms_since_last_tick) {
            send_arp_request(iter2->first);
            iter2->second.time_ms_to_retry = NetworkInterface::ARP_RESPONSE_DEFAULT_TTL_MS;
        } else {
            iter2->second.time_ms_to_retry -= ms_since_last_tick;
            iter2++;
        }
    }
}
