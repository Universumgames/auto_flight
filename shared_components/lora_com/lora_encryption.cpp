#include <esp_random.h>

#include "LoRa_Communication.hpp"
#include "mbedtls/aes.h"

#include "mbedtls/gcm.h"
#include <string.h>

#define KEY_SIZE 16
#define NONCE_SIZE 12
#define TAG_SIZE 16
#define MAX_PAYLOAD 64

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
int encrypt(const uint8_t *key,
             const uint8_t *nonce,
             const uint8_t *plaintext, const size_t len,
             uint8_t *ciphertext,
             uint8_t *tag)
{
    if (len > MAX_PAYLOAD) return -1;   // prevent overflow
    if (!plaintext || !ciphertext || !tag) return -2;

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, KEY_SIZE * 8);

    mbedtls_gcm_crypt_and_tag(&gcm,
        MBEDTLS_GCM_ENCRYPT,
        len,
        nonce, NONCE_SIZE,
        NULL, 0,                 // no additional data
        plaintext,
        ciphertext,
        TAG_SIZE,
        tag);

    mbedtls_gcm_free(&gcm);
    return 0;
}

int decrypt(const uint8_t *key,
            const uint8_t *nonce,
            const uint8_t *ciphertext, const size_t len,
            const uint8_t *tag,
            uint8_t *output)
{
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 128);

    int ret = mbedtls_gcm_auth_decrypt(&gcm,
        len,
        nonce, NONCE_SIZE,
        NULL, 0,
        tag, TAG_SIZE,
        ciphertext,
        output);

    mbedtls_gcm_free(&gcm);

    return ret; // 0 = success, !=0 = tampered
}

uint8_t* LoRa_Communication::encryptData(const uint8_t* originalData, const int originalSize, int* encryptedSize) {
    uint8_t encryptedData[originalSize];
    uint8_t tag[TAG_SIZE];
    uint8_t nonce[NONCE_SIZE];

    esp_fill_random(nonce, NONCE_SIZE); // Generate random nonce

    // encrypt data
    encrypt(key, nonce, originalData, originalSize, encryptedData, tag);

    // prepare encrypted packet
    size_t encryptedDataSize = originalSize + TAG_SIZE + NONCE_SIZE; // ciphertext + tag + nonce
    uint8_t* packetData = new uint8_t[encryptedDataSize];
    memccpy(packetData, nonce, NONCE_SIZE, sizeof(uint8_t));
    memccpy(packetData + NONCE_SIZE, encryptedData, originalSize, sizeof(uint8_t));
    memccpy(packetData + NONCE_SIZE + originalSize, tag, TAG_SIZE, sizeof(uint8_t));
    *encryptedSize = encryptedDataSize;

    return packetData;
}

uint8_t* LoRa_Communication::decryptData(const uint8_t* encryptedData, const int encryptedSize, int* decryptedSize) {
    if (encryptedSize < NONCE_SIZE + TAG_SIZE) return nullptr; // invalid packet

    uint8_t* nonce = new uint8_t[NONCE_SIZE];
    uint8_t* tag = new uint8_t[TAG_SIZE];
    int ciphertextSize = encryptedSize - NONCE_SIZE - TAG_SIZE;
    uint8_t* ciphertext = new uint8_t[ciphertextSize];

    memccpy(nonce, encryptedData, NONCE_SIZE, sizeof(uint8_t));
    memccpy(ciphertext, encryptedData + NONCE_SIZE, ciphertextSize, sizeof(uint8_t));
    memccpy(tag, encryptedData + NONCE_SIZE + ciphertextSize, TAG_SIZE, sizeof(uint8_t));

    uint8_t* decryptedData = new uint8_t[ciphertextSize];
    int ret = decrypt(key, nonce, ciphertext, ciphertextSize, tag, decryptedData);
    if (ret != 0) {
        delete[] decryptedData;
        return nullptr; // decryption failed (tampered)
    }
    *decryptedSize = ciphertextSize;
    return decryptedData;
}

