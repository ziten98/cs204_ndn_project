#pragma once
#include "../headers.hxx"
#include "../definitions.hxx"
#include "Packet.hxx"

struct AnnouncementPacket : Packet {
    std::string Name;
    int Hops = 0;
    int Delay = 0;

    AnnouncementPacket();
    AnnouncementPacket(uv_buf_t buf);
    virtual void ParseBuf(uv_buf_t buf) override;
    virtual uv_buf_t SerializeToBuf() override;
};