#include "encryption.h"
#include <cstdio>

//function to verify a digest against the data
bool verifyDigest(uint8_t data[48], uint8_t digest[HASH_SIZE]) {
    uint8_t hash[HASH_SIZE];
    hashData(data, hash);
    return memcmp(hash, digest, HASH_SIZE) == 0;
}

//function to compute the hash of an input using SHA-256
void hashData(uint8_t* input, uint8_t* output) {
    picohash_ctx_t ctx;
    picohash_init_sha256(&ctx);
    picohash_update(&ctx, input, HASH_SIZE);
    picohash_final(&ctx, output);
}

//function to encrypt a payload with the session keys of all hops in the circuit, starting from the last hop
void encryptOnion(uint8_t hopCount, HopSecretKey keys[4], uint8_t* payload, size_t payloadLen) {
    for (int i = hopCount - 1; i >= 0; --i) {
        struct AES_ctx ctx;
        uint8_t iv[16];
        memcpy(iv, keys[i].nonce, 16);
        AES_init_ctx_iv(&ctx, keys[i].key, iv);
        AES_CTR_xcrypt_buffer(&ctx, payload, payloadLen); //subsequent layers of encryption for each hop
    }
}

//USED?
//function to decrypt a payload with the session keys of all hops in the circuit, starting from the first hop
void decryptOnion(uint8_t hopCount, HopSecretKey keys[4], uint8_t* nonce, uint8_t* payload, size_t payloadLen) {
    for (int i = 0; i < hopCount; ++i) {
        AES_ctx ctx;
        AES_init_ctx_iv(&ctx, keys[i].key, nonce);
        AES_CTR_xcrypt_buffer(&ctx, payload, payloadLen);
    }
}

//function to decrypt a payload with the session key of a single hop.
// used for decrypting the payload of a RELAY cell at each hop
void decryptSingleHop(uint8_t* key, uint8_t* nonce, uint8_t* payload, size_t len){
    AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, nonce);
    AES_CTR_xcrypt_buffer(&ctx, payload, len);
}