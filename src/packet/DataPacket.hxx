#pragma once
#include "../headers.hxx"
#include "../definitions.hxx"
#include "Packet.hxx"

struct DataPacket : Packet {
    std::string Name;
    std::vector<char> Content;
    unsigned char SHA256Sum[32];
    std::vector<unsigned char> Signature;
    std::string Tag;
    long FreshnessPeriodMs = NDN_DATA_DEFAULT_FRESHNESS_MS;

    DataPacket();
    DataPacket(uv_buf_t buf);
    virtual void ParseBuf(uv_buf_t buf) override;
    virtual uv_buf_t SerializeToBuf() override;
};