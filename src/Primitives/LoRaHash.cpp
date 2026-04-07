#include "LoRaHash.h"
#include <string.h>

//OPEN ADDRESSING HASH TABLE IMPLEMENTATION

//globals
#define MAX_NODES 16
#define BYTE_COUNT 44
// Packet layout:
//   nodeID: 4 byte
//   hash: 32 bytes
//   position: 4 byte
//   lastUpdated: 4 byte
//  = 44 bytes/node
//
// 44 times 16 is 704 bytes (maximum)


//finds the slot of a certain node in the table given nodeID
//returns index if found, -1 if not found
static int findSlot(LoRaNode* table, size_t size, uint32_t nodeID) {
    size_t start = nodeID % size; //mapping of ID in 0..MAX_NODES-1: makes finding faster
    for (size_t i = 0; i < size; i++) {
        size_t idx = (start + i) % size; //modulo
        if (table[idx].nodeID == nodeID) return idx; //found
        if (table[idx].nodeID == -1) break; // -1 = empty slot; cant be further
    }
    return -1;
}

//public methods

//initialize hash table
void LoraHashTable::init() {
    table = (LoRaNode*)malloc(MAX_NODES * sizeof(LoRaNode)); //reserve memory
    // -1 is start value, indicates empty slot
    memset(table, -1, MAX_NODES * sizeof(LoRaNode)); //fill memory
}

//add a node to the hash table
uint8_t LoraHashTable::addNode(uint32_t nodeID, uint32_t position, uint32_t timestamp, uint8_t* publicKey) {
    size_t start = nodeID % size;
    for (size_t i = 0; i < size; i++) {
        size_t idx = (start + i) % size;

        if (table[idx].nodeID == -1) { // empty slot found
            table[idx].nodeID = nodeID;
            table[idx].position = position;
            table[idx].lastUpdated = timestamp;
            memcpy(table[idx].publicKey, publicKey, 32);
            memset(table[idx].sessionKey, 0, 32); //puts all zeroes as placeholder for SHA-256 hash, filled in upon sending message
            return idx;
        }

        if (table[idx].nodeID == nodeID) return idx; // already exists, do nothing
    }
    //table full, could not add node / remove oldest?
    return -1;
}

void LoraHashTable::updateNode(uint32_t nodeID, uint32_t position, uint32_t timestamp, uint8_t* publicKey) {
    int idx = findSlot(table, size, nodeID);
    if (idx == -1) {
        // Node not seen before, add it first
        idx = addNode(nodeID, position, timestamp, publicKey);
        if (idx == -1) return; // table full, give up
    }
    table[idx].lastUpdated = timestamp;
    table[idx].position = position;
    memcpy(table[idx].publicKey, publicKey, 32);

}

void LoraHashTable::removeNode(uint32_t nodeID) {
    int idx = findSlot(table, size, nodeID);
    if (idx == -1) return; //not found

    //free the slot
    memset(&table[idx], -1, sizeof(LoRaNode));

    // Rehash any nodes that were displaced by this one (open-addressing requires this)
    for (size_t i = 1; i < size; i++) {
        size_t next = ((size_t)idx + i) % size;
        if (table[next].nodeID == -1) break;   // empty — stop

        LoRaNode displaced = table[next];
        memset(&table[next], -1, sizeof(LoRaNode)); // temporarily remove
        size_t newIdx = addNode(displaced.nodeID, displaced.position, displaced.lastUpdated, displaced.publicKey);                     // re-insert at correct slot

        // Restore its fields at new index
        if (newIdx != -1) {
            table[newIdx].position    = displaced.position;
            table[newIdx].lastUpdated = displaced.lastUpdated;
            memcpy(table[newIdx].sessionKey, displaced.sessionKey, 32);
        }
    }
}
    

// Serialise the entire table into a buffer for broadcasting
uint32_t LoraHashTable::serialiseSelf(uint8_t* buffer, size_t length) {

    size_t counter = 0;
    uint8_t* ptr = buffer;

    // Iterate through the table and serialise each node's data
    for (size_t i = 0; i < size; i++) {
        if (table[i].nodeID == -1) continue; // skip empty slots

        memcpy(ptr, &table[i].nodeID, 4);       ptr += 4;
        memcpy(ptr, table[i].publicKey, 32);   ptr += 32; //already a pointer
        memcpy(ptr, &table[i].position, 4);     ptr += 4;
        memcpy(ptr, &table[i].lastUpdated, 4);  ptr += 4;
        counter++;
    }

    size_t payloadLength = counter * BYTE_COUNT;

    if (payloadLength > length) return 0; // Not enough space in buffer

    return payloadLength; //return length of actual data in buffer
}


void LoraHashTable::updateTable(uint8_t* buffer, size_t length) {
    
    if (length % BYTE_COUNT != 0) return; // packet is corrupted / malformed

    size_t nodeCount = length / BYTE_COUNT;
    uint8_t* ptr = buffer;

    for (size_t i = 0; i < nodeCount; i++) {
        uint32_t nodeID = *ptr; ptr += 4;
        uint8_t* publicKey = ptr; ptr += 32;
        uint32_t position = *ptr; ptr += 4;
        uint32_t lastUpdated = *ptr; ptr += 4;

        // add if new, update timestamp/position if existing
        updateNode(nodeID, position, lastUpdated, publicKey);
        deriveNodeSessionKey(nodeID);
    }

    return;
}

//determine the session key based on the node's public key
void LoraHashTable::deriveNodeSessionKey(uint32_t nodeID) {
    int idx = findSlot(table, size, nodeID);
    if (idx == -1) return; //not found
    //TODO: derive sessionKey using Hellman's key exchange (library or from scratch?)
    uint8_t* sessionKey = table[idx].publicKey; //placeholder
    memcpy(table[idx].sessionKey, sessionKey, 32);

}

//Retrieve the SHA-256 session key for a given node ID
uint8_t* LoraHashTable::getNodeSessionKey(uint32_t nodeID) {
    int idx = findSlot(table, size, nodeID);
    if (idx == -1) return nullptr;
    return table[idx].sessionKey;
}
