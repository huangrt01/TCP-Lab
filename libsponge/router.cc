#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";
    _route_table.push_back({route_prefix, prefix_length, next_hop, interface_num});
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    if (dgram.header().ttl >= 1) {
        dgram.header().ttl--;
    }
    if (dgram.header().ttl == 0) {
        return;
    }
    uint8_t max_prefix_length = 0;
    RouteEntry *cur_route = nullptr;
    for (auto &route : _route_table) {
        uint32_t mask = 0;
        if (route.prefix_length > 0) {
            mask = ~((1 << (32 - route.prefix_length)) - 1);
        }
        if ((route.prefix_length == 0) || (route.route_prefix == (dgram.header().dst & mask))) {
            if (route.prefix_length >= max_prefix_length) {
                max_prefix_length = route.prefix_length;
                cur_route = &route;
            }
        }
    }
    if (cur_route) {
        auto next_hop = cur_route->next_hop.has_value() ? cur_route->next_hop.value()
                                                        : Address::from_ipv4_numeric(dgram.header().dst);
        // cerr << "DEBUG: select interface " << cur_route->interface_num << ", next hop: " << next_hop.ip() << "\n";
        interface(cur_route->interface_num).send_datagram(dgram, next_hop);
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
