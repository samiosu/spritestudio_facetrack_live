#include "spriteface/secure_packet.hpp"
#include "spriteface/face.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace spriteface {
namespace {
constexpr std::string_view magic = "SF2";
constexpr std::size_t nonce_size = 12;
constexpr std::size_t tag_size = 16;

std::array<unsigned char, 16> key_bytes(std::string_view token) {
    const auto normalized = normalize_session_token(token);
    std::array<unsigned char, 16> key{};
    for (std::size_t i = 0; i < key.size(); ++i)
        key[i] = static_cast<unsigned char>(std::stoi(normalized.substr(2 * i, 2), nullptr, 16));
    return key;
}

void random_bytes(unsigned char* bytes, std::size_t size) {
#ifdef _WIN32
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, bytes, static_cast<ULONG>(size),
                                        BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        throw std::runtime_error("secure random generation failed");
#else
    if (RAND_bytes(bytes, static_cast<int>(size)) != 1)
        throw std::runtime_error("secure random generation failed");
#endif
}

#ifdef _WIN32
class AesKey {
public:
    explicit AesKey(const std::array<unsigned char, 16>& raw) {
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_AES_ALGORITHM, nullptr, 0)))
            throw std::runtime_error("AES provider unavailable");
        if (!BCRYPT_SUCCESS(BCryptSetProperty(algorithm_, BCRYPT_CHAINING_MODE,
                reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                sizeof(BCRYPT_CHAIN_MODE_GCM), 0)))
            throw std::runtime_error("AES-GCM unavailable");
        ULONG size = 0, ignored = 0;
        if (!BCRYPT_SUCCESS(BCryptGetProperty(algorithm_, BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&size), sizeof(size), &ignored, 0)))
            throw std::runtime_error("AES key allocation failed");
        object_.resize(size);
        if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(algorithm_, &key_, object_.data(), size,
                const_cast<PUCHAR>(raw.data()), static_cast<ULONG>(raw.size()), 0)))
            throw std::runtime_error("AES key setup failed");
    }
    ~AesKey() { if (key_) BCryptDestroyKey(key_); if (algorithm_) BCryptCloseAlgorithmProvider(algorithm_, 0); }
    AesKey(const AesKey&) = delete;
    AesKey& operator=(const AesKey&) = delete;
    BCRYPT_KEY_HANDLE get() const { return key_; }
private:
    BCRYPT_ALG_HANDLE algorithm_ = nullptr;
    BCRYPT_KEY_HANDLE key_ = nullptr;
    std::vector<unsigned char> object_;
};
#endif

std::string crypt(std::string_view input, const std::array<unsigned char, 16>& key,
                  const unsigned char* nonce, unsigned char* tag, bool encrypt) {
    std::string output(input.size(), '\0');
#ifdef _WIN32
    AesKey aes(key);
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth;
    BCRYPT_INIT_AUTH_MODE_INFO(auth);
    auth.pbNonce = const_cast<PUCHAR>(nonce);
    auth.cbNonce = static_cast<ULONG>(nonce_size);
    auth.pbAuthData = reinterpret_cast<PUCHAR>(const_cast<char*>(magic.data()));
    auth.cbAuthData = static_cast<ULONG>(magic.size());
    auth.pbTag = tag;
    auth.cbTag = static_cast<ULONG>(tag_size);
    ULONG written = 0;
    const auto status = encrypt
        ? BCryptEncrypt(aes.get(), reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),
                        static_cast<ULONG>(input.size()), &auth, nullptr, 0,
                        reinterpret_cast<PUCHAR>(output.data()), static_cast<ULONG>(output.size()), &written, 0)
        : BCryptDecrypt(aes.get(), reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),
                        static_cast<ULONG>(input.size()), &auth, nullptr, 0,
                        reinterpret_cast<PUCHAR>(output.data()), static_cast<ULONG>(output.size()), &written, 0);
    if (!BCRYPT_SUCCESS(status) || written != input.size())
        throw std::invalid_argument("packet authentication failed");
#else
    EVP_CIPHER_CTX* context = EVP_CIPHER_CTX_new();
    if (!context) throw std::runtime_error("AES-GCM context allocation failed");
    int written = 0, total = 0;
    const auto* plain = reinterpret_cast<const unsigned char*>(input.data());
    auto* cipher = reinterpret_cast<unsigned char*>(output.data());
    const bool initialized = encrypt
        ? EVP_EncryptInit_ex(context, EVP_aes_128_gcm(), nullptr, key.data(), nonce) == 1
        : EVP_DecryptInit_ex(context, EVP_aes_128_gcm(), nullptr, key.data(), nonce) == 1;
    bool success = initialized;
    if (success) success = encrypt
        ? EVP_EncryptUpdate(context, nullptr, &written,
                            reinterpret_cast<const unsigned char*>(magic.data()), static_cast<int>(magic.size())) == 1
        : EVP_DecryptUpdate(context, nullptr, &written,
                            reinterpret_cast<const unsigned char*>(magic.data()), static_cast<int>(magic.size())) == 1;
    if (success) success = encrypt
        ? EVP_EncryptUpdate(context, cipher, &written, plain, static_cast<int>(input.size())) == 1
        : EVP_DecryptUpdate(context, cipher, &written, plain, static_cast<int>(input.size())) == 1;
    total = written;
    if (!encrypt && success)
        success = EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_TAG,
                                      static_cast<int>(tag_size), tag) == 1;
    if (success) success = encrypt
        ? EVP_EncryptFinal_ex(context, cipher + total, &written) == 1
        : EVP_DecryptFinal_ex(context, cipher + total, &written) == 1;
    total += written;
    if (encrypt && success)
        success = EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_GET_TAG,
                                      static_cast<int>(tag_size), tag) == 1;
    EVP_CIPHER_CTX_free(context);
    if (!success || total != static_cast<int>(input.size()))
        throw std::invalid_argument("packet authentication failed");
#endif
    return output;
}
} // namespace

std::string secure_random_hex() {
    std::array<unsigned char, 16> bytes{};
    random_bytes(bytes.data(), bytes.size());
    constexpr char alphabet[] = "0123456789abcdef";
    std::string result;
    result.reserve(32);
    for (unsigned char byte : bytes) {
        result += alphabet[byte >> 4];
        result += alphabet[byte & 15];
    }
    return result;
}

std::string seal_packet(std::string_view plaintext, std::string_view token) {
    std::array<unsigned char, nonce_size> nonce{};
    std::array<unsigned char, tag_size> tag{};
    random_bytes(nonce.data(), nonce.size());
    auto ciphertext = crypt(plaintext, key_bytes(token), nonce.data(), tag.data(), true);
    std::string result(magic);
    result.append(reinterpret_cast<const char*>(nonce.data()), nonce.size());
    result += ciphertext;
    result.append(reinterpret_cast<const char*>(tag.data()), tag.size());
    return result;
}

std::string open_packet(std::string_view packet, std::string_view token) {
    if (packet.size() < magic.size() + nonce_size + tag_size ||
        packet.substr(0, magic.size()) != magic)
        throw std::invalid_argument("unsupported or unencrypted packet");
    const auto* nonce = reinterpret_cast<const unsigned char*>(packet.data() + magic.size());
    std::array<unsigned char, tag_size> tag{};
    for (std::size_t i = 0; i < tag.size(); ++i)
        tag[i] = static_cast<unsigned char>(packet[packet.size() - tag.size() + i]);
    const auto ciphertext = packet.substr(magic.size() + nonce_size,
                                           packet.size() - magic.size() - nonce_size - tag_size);
    return crypt(ciphertext, key_bytes(token), nonce, tag.data(), false);
}
} // namespace spriteface
