#pragma once
// we put headers of third party library that will be used in every source file here

#include <bits/stdc++.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <uv.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/aes.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <sw/redis++/redis++.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>