#pragma once
#include "headers.hxx"

extern bool string_begins(std::string s, std::string substr);

extern std::string string_skip_front(std::string s, int length);

extern void init_loggers();

extern void ndn_connect_cb_send_close(uv_connect_t* con, int status);

extern void ndn_write_cb_close(uv_write_t* req, int status);

extern void ndn_close_cb(uv_handle_t* handle);

extern void ndn_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t *buf);


extern long get_timestamp();

extern std::string read_file_to_string(const char* path);

extern uv_buf_t read_file_to_binary(const char* path, int offset, int length);

extern std::string get_sender_ip_and_port(const uv_tcp_t* sock, unsigned short* out_port);

extern RSA* load_private_key(const char* path);

extern RSA* load_public_key(const char* path);

extern uv_buf_t create_signature(unsigned char* message, int message_len, RSA* rsa);

extern bool verify_signature(unsigned char* message, int message_len, unsigned char* signatre, int signatre_len, RSA* rsa);

struct NDNException : public std::exception
{
   std::string s;

   NDNException(std::string s) {
      this->s = s;
   }

   ~NDNException() throw () {}

   const char* what() const throw() { 
      return s.c_str();
   }

};