#include "../headers.hxx"
#include "../definitions.hxx"
#include "../util.hxx"

// test the functionalities of openssl on SHA256 digest

int main(int argc,const char* argv[]) {
    if(argc == 1) {
        std::cout << "Usage: sha256test FILE\n";
        return 1;
    }

    unsigned long filesize = std::filesystem::file_size(std::filesystem::path(argv[1]));
    uv_buf_t buf = read_file_to_binary(argv[1], 0, filesize);

    unsigned char result[32];
    SHA256((const unsigned char*)buf.base, buf.len, result);

    spdlog::info("sha256 sum of {} is {}", argv[1], spdlog::to_hex(&result[0], &result[32]));

    return 0;
}