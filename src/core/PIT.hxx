#pragma once
#include "../headers.hxx"
#include "../definitions.hxx"
#include "../packet/InterestPacket.hxx"
#include "../packet/DataPacket.hxx"
#include "FIB.hxx"


typedef void (*SendInterestPacketToFunc)(std::string ip, InterestPacket packet);
typedef void (*SendDataPacketToFunc)(uv_stream_t* sock, DataPacket packet);

/**
 * represent a line in the PIT table
 */
struct PITLine {
    std::string prefix;
    // The request face is actually the ip address that we should return request to
    std::vector<uv_stream_t*> request_face;

    std::string ToString();
};


/**
 * The pending interst table
 */
struct PIT {
    std::list<PITLine*> table;

    void OnNewRequest(InterestPacket* packet, FIB* fib, uv_stream_t* sock, SendInterestPacketToFunc send_interest_func, SendDataPacketToFunc send_data_func);
    void OnDataReturned(DataPacket* packet, SendDataPacketToFunc func);
};

/**
 * A line in the content frequency count map.
 */
struct FreqCountLine {
    std::string content_name;
    long total = 1;
    long cached = 0;
};