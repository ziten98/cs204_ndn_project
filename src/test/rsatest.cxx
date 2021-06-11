#include "../headers.hxx"
#include "../definitions.hxx"
#include "../util.hxx"

// test the functionalities of openssl on rsa sign and verify

int main(int argc,const char* argv[]) {
    if(argc < 4) {
        std::cout << "Usage: rsate PRIVATE_KEY_PATH PUBLIC_KEY_PATH MESSAGE\n";
        return 1;
    }

    RSA* rsa_private = load_private_key(argv[1]);
    RSA* rsa_public = load_public_key(argv[2]);

    std::string message = argv[3];
    spdlog::info("original text: {}", message);

    spdlog::info("signing");
    uv_buf_t buf = create_signature((unsigned char*)message.data(), message.length(), rsa_private);

    spdlog::info("signature is {}", spdlog::to_hex(buf.base, buf.base + buf.len));

    spdlog::info("verifying");
    bool verify_result = verify_signature((unsigned char*)message.data(), message.length(), (unsigned char*)buf.base, buf.len, rsa_public);
    if(verify_result) {
        spdlog::info("verify succeed");
    }else{
        spdlog::info("verify failed");
    }

    delete[] buf.base;

    return 0;
}