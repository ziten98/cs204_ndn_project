#include "headers.hxx"
#include "definitions.hxx"
#include "util.hxx"
#include <sys/time.h>

/**
 * This file includes common utility functions used in other source files.
 */

extern uv_loop_t *loop;

/**
 * Test if a string includes another string at the beginning.
 * @param s The string to test.
 * @param substr The substring to test.
 */
bool string_begins(std::string s, std::string substr) {
    if(s.length() < substr.length()) {
        return false;
    }

    for(std::size_t i=0; i<substr.length(); i++) {
        if(s[i] != substr[i]) {
            return false;
        }
    }

    return true;
}


/**
 * Skip the first length number of characters in a string, return the result new string.
 * @param s The string.
 * @param length The number of characters to erase.
 */
std::string string_skip_front(std::string s, int length) {
    std::string result;
    const char* start = s.c_str() + length;
    result = start;

    return result;
}


/**
 * Initialize the global logger to use across this project.
 */
void init_loggers() {
    #ifdef NDN_LOG_TO_FILE
        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(std::make_shared<spdlog::sinks::ansicolor_stdout_sink_st>());
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_st>("logs.log", true));
        auto combined_logger = std::make_shared<spdlog::logger>("2loggers", begin(sinks), end(sinks));

        spdlog::set_default_logger(combined_logger);
    #endif

    #ifdef NDN_DEBUG
        spdlog::set_level(spdlog::level::debug);
    #else
        spdlog::set_level(spdlog::level::info);
    #endif
}

/**
 * The callback function whenever a connect operation is finished.
 * Then buf is send and connection is closed.
 * The buf is assumed to be in the con->data, which is of uv_buf_t* type.
 */
void ndn_connect_cb_send_close(uv_connect_t* con, int status) {
    if(status < 0) {
        spdlog::error("connect error: {}", uv_strerror(status));

        uv_buf_t *buf = (uv_buf_t *)con->data;
        delete[] buf->base;
        delete buf;
        
        if (!uv_is_closing((uv_handle_t*)con->handle)) {
            uv_close((uv_handle_t*)con->handle, ndn_close_cb);
        }
        return;
    }

    uv_buf_t *buf = (uv_buf_t *)con->data;

    uv_write_t *req = new uv_write_t;
    uv_write(req, con->handle, buf, 1, ndn_write_cb_close);

    delete[] buf->base;
    delete buf;
    delete con;
}


/**
 * The callback function whenever a write operation is finished.
 * The tcp connection will be closed.
 * It will delete the uv_write_t req.
 */
void ndn_write_cb_close(uv_write_t* req, int status) {

    if(status < 0) {
        spdlog::critical("write error: {}", uv_strerror(status));
        exit(1);
    }

    // for(unsigned int i=0; i < req->nbufs; i++) {
    //     spdlog::info("nbufs: {}", req->nbufs);
    //     for(int b = 0; b < req->nbufs; b++) {
    //         spdlog::info("buf {} len: {}  base: {}", b, req->bufs[i].len, req->bufs[i].base);
    //     }

    //     delete[] req->bufs[i].base;
    // }

    if (!uv_is_closing((uv_handle_t*)req->handle)) {
        uv_close((uv_handle_t*)req->handle, ndn_close_cb);
    }
    delete req;
}



/**
 * The callback function whenever a handle is closed.
 * The memory of the handle is freed.
 */
void ndn_close_cb(uv_handle_t* handle) {
    delete handle;
}



/**
 * The callback function whenever a buffer allocation is needed.
 * User is responsible for freeing it in the uv_udp_recv_cb or the uv_read_cb callback.
 */
void ndn_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t *buf) {
    #ifdef NDN_DEBUG
        spdlog::debug("allocate memory of size {}", suggested_size);
    #endif

    buf->base = new char[suggested_size];
    if(!buf->base) {
        spdlog::critical("memory allocation failed");
        exit(1);
    }
    buf->len = suggested_size;
}



/**
 * Get the current timestamp in milliseconds since January 1, 1970
 */
long get_timestamp() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}


/**
 * Read the file at given path into string.
 * @param path The path of the file.
 */
std::string read_file_to_string(const char* path) {
    std::ifstream stream(path);
    if(!stream.is_open()) {
        std::string what = fmt::format("can not open file {}", path);
        throw NDNException(what);
    }

    std::string result((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    stream.close();
    return result;
}


/**
 * Read the file at given path into binary blobs(in the form of uv_buf_t).
 * @param path The path of the file.
 */
uv_buf_t read_file_to_binary(const char* path, int offset, int length) {
    std::ifstream stream(path, std::ios::binary);
    if(!stream.is_open()) {
        std::string what = fmt::format("can not open file {}", path);
        throw NDNException(what);
    }

    stream.seekg(offset, std::ios::beg);

    uv_buf_t result;
    result.len = length;
    result.base = new char[result.len];

    stream.read(result.base, length);
    stream.close();
    
    return result;
}


/**
 * Get ip address and port of the sender part of the tcp socket.
 * @param out_port If it is not NULL, the port number will be written to that address.
 */
std::string get_sender_ip_and_port(const uv_tcp_t* sock, unsigned short* out_port) {
    sockaddr_in sock_addr;
    int namelen = sizeof(sock_addr);
    uv_tcp_getpeername(sock, (sockaddr*)&sock_addr, &namelen);
    unsigned short port = ntohs(sock_addr.sin_port);
    if(out_port) {
        *out_port = port;
    }

    char sender_ip[21] = {0};
    uv_ip4_name(&sock_addr, sender_ip, 20);
    
    std::string result = sender_ip;
    return result;
}


/**
 * Load private key from file to memory.
 */
RSA* load_private_key(const char* path) {
    BIO* bp = BIO_new(BIO_s_file());
    if(bp == NULL) {
        spdlog::critical("BIO_new failed");
        exit(1);
    }

    BIO_read_filename(bp, path);

    RSA* rsa = PEM_read_bio_RSAPrivateKey(bp, NULL, NULL, NULL);
    BIO_free(bp);

    if(rsa == NULL) {
        spdlog::critical("PEM_read_bio_RSAPrivateKey failed");
        exit(1);
    }

    return rsa;
    
    std::string message;
}


/**
 * Load public key from file to memory.
 */
RSA* load_public_key(const char* path) {
    BIO* bp = BIO_new(BIO_s_file());
    if(bp == NULL) {
        spdlog::critical("BIO_new failed");
        exit(1);
    }

    BIO_read_filename(bp, path);

    RSA* rsa = PEM_read_bio_RSA_PUBKEY(bp, NULL, NULL, NULL);
    BIO_free(bp);

    if(rsa == NULL) {
        spdlog::critical("PEM_read_bio_RSAPrivateKey failed");
        exit(1);
    }

    return rsa;
    
    std::string message;
}

/**
 * Create a signature using the rsa private key.
 */
uv_buf_t create_signature(unsigned char* message, int message_len, RSA* rsa) {
    try{
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_PKEY* key = EVP_PKEY_new();
        EVP_PKEY_assign_RSA(key, rsa);

        uv_buf_t result;

        if (EVP_DigestSignInit(ctx,NULL, EVP_sha256(), NULL,key)<=0) {
            throw NDNException("EVP_DigestSignInit failed");
        }
        if (EVP_DigestSignUpdate(ctx, message, message_len) <= 0) {
            throw NDNException("EVP_DigestSignUpdate failed");
        }
        if (EVP_DigestSignFinal(ctx, NULL, &result.len) <=0) {
            throw NDNException("EVP_DigestSignFinal failed");
        }

        result.base = new char[result.len];
        if (EVP_DigestSignFinal(ctx, (unsigned char*)result.base, &result.len) <= 0) {
            throw NDNException("EVP_DigestSignFinal failed");
        }
        
        EVP_MD_CTX_free(ctx);

        return result;
    }catch(NDNException& e) {
        spdlog::critical(e.what());
        exit(1);
    }
}


/**
 * Verify the signature using the rsa public key.
 */
bool verify_signature(unsigned char* message, int message_len, unsigned char* signatre, int signatre_len, RSA* rsa) {
    EVP_PKEY* pubKey  = EVP_PKEY_new();
    EVP_PKEY_assign_RSA(pubKey, rsa);
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();

    if (EVP_DigestVerifyInit(ctx,NULL, EVP_sha256(),NULL,pubKey)<=0) {
        return false;
    }
    if (EVP_DigestVerifyUpdate(ctx, message, message_len) <= 0) {
        return false;
    }
    int AuthStatus = EVP_DigestVerifyFinal(ctx, signatre, signatre_len);
    if (AuthStatus==1) {
        EVP_MD_CTX_free(ctx);
        return true;
    } else{
        EVP_MD_CTX_free(ctx);
        return false;
    }
}