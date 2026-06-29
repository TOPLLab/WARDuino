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

//updates a node's information in the table
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

//remvoes a node from the table
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

// Serialise a node's information based on its ID into a buffer for broadcasting
uint32_t LoraHashTable::serialiseSelf(uint8_t* buffer, uint16_t nodeID, size_t length) {
    if (BYTE_COUNT > length) return 0; //buffer is too small to serialise all.
    uint8_t* ptr = buffer;

    // Iterate through the table and serialise each node's data
    for (size_t i = 0; i < size; i++) {
        if (table[i].nodeID != nodeID) continue; // find the right slot

        memcpy(ptr, &table[i].nodeID, 2);       ptr += 2;
        memcpy(ptr, &table[i].role, 1);         ptr += 1;
        memcpy(ptr, &table[i].position, 4);     ptr += 4;
        memcpy(ptr, &table[i].lastUpdated, 4);  ptr += 4;
        return 1;    
    }
    return 0; //node not found
}

//updates the table with the information of 1 node
void LoraHashTable::updateTable(uint8_t* buffer) {
    uint8_t* ptr = buffer;
    uint16_t nodeID;
    memcpy(&nodeID, ptr, 2); ptr += 2;
    NodeRole role = (NodeRole)*ptr; ptr += 1;
    uint32_t position;
    memcpy(&position, ptr, 4); ptr += 4;
    uint32_t lastUpdated;
    memcpy(&lastUpdated, ptr, 4); ptr += 4;
    updateNode(nodeID, role, position, lastUpdated);
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
