#include "LoRaHash.h"

//OPEN ADDRESSING HASH TABLE IMPLEMENTATION

//finds the slot of a certain node in the table given nodeID
//returns index if found, 0 if not found
int findSlot(LoRaNode* table, size_t size, uint16_t nodeID) {
    size_t start = nodeID % size; //mapping of ID in 0..MAX_NODES-1: makes finding faster
    for (size_t i = 0; i < size; i++) {
        size_t idx = (start + i) % size; //modulo
        if (table[idx].nodeID == nodeID) return idx; //found
        if (table[idx].nodeID == 0) break; // 0 = empty slot; cant be further
    }
    return -1; //not found
}

//public methods

//initialize hash table
void LoraHashTable::init() {
    size = MAX_NODES;
    table = (LoRaNode*)malloc(MAX_NODES * sizeof(LoRaNode)); //reserve memory
    // 0 is start value, indicates empty slot
    memset(table, 0, MAX_NODES * sizeof(LoRaNode)); //fill memory
}

//add a node to the hash table
int LoraHashTable::addNode(uint16_t nodeID, NodeRole role, uint32_t position, uint32_t timestamp) {
    size_t start = nodeID % size;
    for (size_t i = 0; i < size; i++) {
        size_t idx = (start + i) % size;

        if (table[idx].nodeID == 0) { // empty slot found
            table[idx].nodeID = nodeID;
            table[idx].role = role;
            table[idx].position = position;
            table[idx].lastUpdated = timestamp;
            return idx;
        }

        if (table[idx].nodeID == nodeID) return idx; // already exists, do nothing
    }
    //table full, could not add node / remove oldest?
    return -1;
}

void LoraHashTable::updateNode(uint16_t nodeID, NodeRole role, uint32_t position, uint32_t timestamp) {
    int idx = findSlot(table, size, nodeID);
    if (idx == -1) {
        // Node not seen before, add it first
        idx = addNode(nodeID, role, position, timestamp);
        if (idx == -1) return; // table full, give up
    }
    table[idx].lastUpdated = timestamp;
    table[idx].position = position;
    table[idx].role = role;

}

void LoraHashTable::removeNode(uint16_t nodeID) {
    int idx = findSlot(table, size, nodeID);
    if (idx == -1) return; //not found

    //free the slot
    memset(&table[idx], 0, sizeof(LoRaNode));

    // Rehash any nodes that were displaced by this one (open-addressing requires this)
    for (size_t i = 1; i < size; i++) {
        size_t next = ((size_t)idx + i) % size;
        if (table[next].nodeID == 0) break;   // empty — stop

        LoRaNode displaced = table[next];
        memset(&table[next], 0, sizeof(LoRaNode)); // temporarily remove
        size_t newIdx = addNode(displaced.nodeID, displaced.role, displaced.position, displaced.lastUpdated);

        // Restore its fields at new index
        if (newIdx != -1) {
            table[newIdx].position    = displaced.position;
            table[newIdx].lastUpdated = displaced.lastUpdated;
        }
    }
}
    

// Serialise the entire table into a buffer for broadcasting
uint32_t LoraHashTable::serialiseSelf(uint8_t* buffer, size_t length) {

    size_t counter = 0;
    uint8_t* ptr = buffer;

    // Iterate through the table and serialise each node's data
    for (size_t i = 0; i < size; i++) {
        if (table[i].nodeID == 0) continue; // skip empty slots

        if ((size_t)(ptr - buffer) + BYTE_COUNT > length) return 0; //buffer is too small to serialise all.

        memcpy(ptr, &table[i].nodeID, 2);       ptr += 2;
        memcpy(ptr, &table[i].role, 1);         ptr += 1;
        memcpy(ptr, &table[i].position, 4);     ptr += 4;
        memcpy(ptr, &table[i].lastUpdated, 4);  ptr += 4;
        counter++;
    }

    size_t payloadLength = counter * BYTE_COUNT;

    if (payloadLength > length) return 0; // Not enough space in buffer

    return payloadLength; //return length of actual data in buffer
}


void LoraHashTable::updateTable(uint8_t* buffer, size_t length) {

    size_t nodeCount = length / BYTE_COUNT;
    uint8_t* ptr = buffer;

    for (size_t i = 0; i < nodeCount; i++) {
        uint16_t nodeID;
        memcpy(&nodeID, ptr, 2); ptr += 2;
        NodeRole role = (NodeRole)*ptr; ptr += 1;
        uint32_t position;
        memcpy(&position, ptr, 4); ptr += 4;
        uint32_t lastUpdated;
        memcpy(&lastUpdated, ptr, 4); ptr += 4;

        if (nodeID == 0) break; //end of valid data

        updateNode(nodeID, role, position, lastUpdated);
    }

    return;
}

void LoraHashTable::printTable() {
    printf("LoraHashTable:\n");
    for (size_t i = 0; i < size; i++) {
        if (table[i].nodeID == 0) continue; // skip empty slots
        printf("NodeID: %d, Role: %d, Position: %d, LastUpdated: %d\n", table[i].nodeID, table[i].role, table[i].position, table[i].lastUpdated);
    }
}

uint16_t LoraHashTable::findRouter() {
    for (size_t i = 0; i < size; i++) {
        if (table[i].nodeID == 0) continue; // skip empty slots
        if (table[i].role == NodeRole::ROUTER) {
            return table[i].nodeID;
        }
    }
    return 0; // router not found
}
