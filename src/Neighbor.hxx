#pragma once
#include "headers.hxx"
#include "definitions.hxx"


// Stores data about a neighbor node in the network
struct Neighbor {
    // The ip address of this neighbor
    std::string ip;
    // The node name of this neighbor
    std::string name;
    
    // The current number of interest packet that has been received from this neighbor.
    long interest_packet_cur_recv_count = 0;
    // The current number of interest packet that has been sent to this neighbor.
    long interest_packet_cur_send_count = 0;
    
    // The number of data packet that is supposed to get from this neighbor.
    long data_packet_pending_recv_count = 0;
    // The number of interest packet that is supposed to send to this neighbor.
    long data_packet_pending_send_count = 0;
    // The current number of data packet that has been received from this neighbor.
    long data_packet_cur_recv_count = 0;
    // The current number of data packet that has been sent to this neighbor.
    long data_packet_cur_send_count = 0;
};