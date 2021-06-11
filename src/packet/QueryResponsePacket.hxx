#pragma once
#include "../headers.hxx"
#include "../definitions.hxx"
#include "Packet.hxx"

struct QueryResponsePacket : Packet {
    QueryResponsePacket();
    virtual void ParseBuf(uv_buf_t buf) override;
    virtual uv_buf_t SerializeToBuf() override;
};