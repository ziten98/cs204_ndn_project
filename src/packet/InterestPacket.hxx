#pragma once
#include "../headers.hxx"
#include "../definitions.hxx"
#include "Packet.hxx"

struct InterestPacket : Packet {
    std::string Name;
    long InterestLifetimeMilli = 0;

    InterestPacket();
    InterestPacket(uv_buf_t buf);
    virtual void ParseBuf(uv_buf_t buf) override;
    virtual uv_buf_t SerializeToBuf() override;
};