#pragma once
#include "../headers.hxx"
#include "../definitions.hxx"

extern void CS_init();

extern void CS_store(std::string name, uv_buf_t buf, long timeout_ms);

extern uv_buf_t CS_get(std::string name);