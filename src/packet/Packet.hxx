#pragma once
#include "../headers.hxx"
#include "../definitions.hxx"

// packet type definition
enum class PacketTypeEnum{
    Invalid,
    NodeQuery, // query status information on the node
    NodeQueryResponse, // the response data for query
    Interest,
    Data,
    Announcement,
};


struct Packet {
    PacketTypeEnum PacketType;
    long Timestamp;
    std::string Tag;
    // the ip address from which the packet is received
    std::string from_ip;
    unsigned short from_port;

    virtual void ParseBuf(uv_buf_t buf) = 0;
    virtual uv_buf_t SerializeToBuf() = 0;
    Packet();
};