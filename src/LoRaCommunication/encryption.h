#include <stdlib.h>
#include <string.h>

#include "../../lib/aes/aes.h"
#include "../../lib/picohash/picohash.h"

#define AES_KEY_SIZE AES_KEYLEN

#ifndef HASH_SIZE
#define HASH_SIZE PICOHASH_SHA256_DIGEST_LENGTH
#endif

struct HopSecretKey {
    uint8_t key[AES_KEY_SIZE];
    uint8_t nonce[16];
};

bool verifyDigest(uint8_t* data, uint8_t* digest);
void hashData(uint8_t* input, uint8_t* output);
void encryptOnion(uint8_t hopCount, HopSecretKey keys[], uint8_t* payload, size_t payloadLen);
void decryptOnion(uint8_t hopCount, HopSecretKey keys[], uint8_t* nonce, uint8_t* payload, size_t payloadLen);
void decryptSingleHop(uint8_t* key, uint8_t* nonce, uint8_t* payload, size_t len);