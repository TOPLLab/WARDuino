#include <stdlib.h>
#include <string.h>
#include <cstdio>

//globals
#define MAX_NODES 16
#define BYTE_COUNT 11
// Packet layout:
//   nodeID: 2 byte
//.  role: 1 byte
//   position: 4 byte
//   lastUpdated: 4 byte
//  = 11 bytes/node
//
// 11 times 16 is 176 bytes (maximum)


//Nodes can be either a node in a local group or a router connecting multiple groups.
enum NodeRole {
    LOCAL,
    ROUTER
};

struct LoRaNode {
    uint16_t nodeID;      //unique ID of the node
    NodeRole role;        // role of the node (leaf or router)
    uint32_t position;    //position in space (needed?)
    uint32_t lastUpdated; //timestamp of the last update
};

class LoraHashTable {
public:
    void init(); // Initialize hash table
    int addNode(uint16_t nodeID, NodeRole role, uint32_t position, uint32_t timestamp); // Add a node to the hash table
    void updateNode(uint16_t nodeID, NodeRole role, uint32_t position, uint32_t timestamp);  // Update a given nodes timestamp/information (needed?)
    void removeNode(uint16_t nodeID);  // Remove a node from the hash table
    uint32_t serialiseSelf(uint8_t* buffer, uint16_t nodeID, size_t length); // Broadcast own hashtable to other nodes (needed for new nodes)
    void updateTable(uint8_t* buffer); // Update own table based on received buffer from other node
    void printTable(); // Print the contents of the hash table (for debugging)
    uint16_t findRouter(); //find router connected to local group

private:
    LoRaNode* table;
    size_t size;
};