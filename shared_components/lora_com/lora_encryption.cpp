#include <esp_random.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <array>
#include <vector>
#include <memory>

#include "LoRa_Communication.hpp"
#include "aes/esp_aes_gcm.h"

static constexpr std::size_t KEY_SIZE = 16;
static constexpr std::size_t NONCE_SIZE = 12;
static constexpr std::size_t TAG_SIZE = 16;
static constexpr std::size_t MAX_PAYLOAD = 64;

// GCM mode constants (as expected by esp functions)
static constexpr int GCM_ENCRYPT = 1;
static constexpr int GCM_DECRYPT = 0;

/**
 *
 * @param key Encryption key
 * @param nonce Nonce
 * @param plaintext Data to encrypt
 * @param len lenght of data
 * @param ciphertext output cypher data
 * @param tag tag
 * @return 0 on success, -1 if plaintext is too long, -2 if null pointer provided
 */
int encrypt(const uint8_t* key,
            const uint8_t* nonce,
            const uint8_t* plaintext, const std::size_t len,
            uint8_t* ciphertext,
            uint8_t* tag)
{
    if (!plaintext || !ciphertext || !tag || !key) return -2;
    if (len > MAX_PAYLOAD) return -1; // prevent overflow

    esp_gcm_context gcm;
    esp_aes_gcm_init(&gcm);
    esp_aes_gcm_setkey(&gcm, 0, key, static_cast<int>(KEY_SIZE * 8));

    int ret = esp_aes_gcm_crypt_and_tag(&gcm,
                                        GCM_ENCRYPT,
                                        static_cast<int>(len),
                                        nonce, static_cast<int>(NONCE_SIZE),
                                        nullptr, 0, // no additional data
                                        plaintext,
                                        ciphertext,
                                        static_cast<int>(TAG_SIZE),
                                        tag);

    esp_aes_gcm_free(&gcm);
    return ret;
}

int decrypt(const uint8_t* key,
            const uint8_t* nonce,
            const uint8_t* ciphertext, const std::size_t len,
            const uint8_t* tag,
            uint8_t* output)
{
    if (!key || !nonce || !ciphertext || !tag || !output) return -2;

    esp_gcm_context gcm;
    esp_aes_gcm_init(&gcm);
    esp_aes_gcm_setkey(&gcm, 0, key, static_cast<int>(KEY_SIZE * 8));

    int ret = esp_aes_gcm_auth_decrypt(&gcm,
                                       static_cast<int>(len),
                                       nonce, static_cast<int>(NONCE_SIZE),
                                       nullptr, 0,
                                       tag, static_cast<int>(TAG_SIZE),
                                       ciphertext,
                                       output);

    esp_aes_gcm_free(&gcm);
    return ret; // 0 = success, !=0 = tampered
}

std::vector<uint8_t> LoRa_CommunicationClass::encryptData(const uint8_t* originalData, const int originalSize) {
    if (!key || !originalData || originalSize <= 0) {
        return {};
    }

    if (static_cast<std::size_t>(originalSize) > MAX_PAYLOAD) {
        return {}; // too large
    }

    std::vector<uint8_t> ciphertext(static_cast<std::size_t>(originalSize));
    std::array<uint8_t, TAG_SIZE> tag{};
    std::array<uint8_t, NONCE_SIZE> nonce{};

    esp_fill_random(nonce.data(), static_cast<size_t>(NONCE_SIZE)); // Generate random nonce

    int ret = encrypt(key, nonce.data(), originalData, static_cast<std::size_t>(originalSize), ciphertext.data(), tag.data());
    if (ret != 0) {
        return {};
    }

    // build packet: nonce | ciphertext | tag
    const std::size_t packetSize = static_cast<std::size_t>(originalSize) + NONCE_SIZE + TAG_SIZE;
    std::vector<uint8_t> packet(packetSize);
    std::memcpy(packet.data(), nonce.data(), NONCE_SIZE);
    std::memcpy(packet.data() + NONCE_SIZE, ciphertext.data(), static_cast<std::size_t>(originalSize));
    std::memcpy(packet.data() + NONCE_SIZE + originalSize, tag.data(), TAG_SIZE);

    return packet;
}

std::vector<uint8_t> LoRa_CommunicationClass::decryptData(const uint8_t* encryptedData, const int encryptedSize) {
    if (!key || !encryptedData) {
        return {};
    }

    if (encryptedSize < static_cast<int>(NONCE_SIZE + TAG_SIZE + 1)) {
        return {}; // invalid or empty payload
    }

    const int ciphertextSize = encryptedSize - static_cast<int>(NONCE_SIZE) - static_cast<int>(TAG_SIZE);
    if (ciphertextSize <= 0) return {};

    std::array<uint8_t, NONCE_SIZE> nonce{};
    std::array<uint8_t, TAG_SIZE> tag{};
    std::vector<uint8_t> ciphertext(static_cast<std::size_t>(ciphertextSize));

    std::memcpy(nonce.data(), encryptedData, NONCE_SIZE);
    std::memcpy(ciphertext.data(), encryptedData + NONCE_SIZE, static_cast<std::size_t>(ciphertextSize));
    std::memcpy(tag.data(), encryptedData + NONCE_SIZE + ciphertextSize, TAG_SIZE);

    std::vector<uint8_t> output(static_cast<std::size_t>(ciphertextSize));
    int ret = decrypt(key, nonce.data(), ciphertext.data(), static_cast<std::size_t>(ciphertextSize), tag.data(), output.data());
    if (ret != 0) {
        return {}; // decryption failed (tampered)
    }

    return output;
}

