#include <stdlib.h>

struct LoRaNode {
    uint32_t nodeID;      //unique ID of the node
    uint32_t position;    //position in space (needed?)
    uint32_t lastUpdated; //timestamp of the last update
    uint8_t publicKey[32];    // RSA public key
    uint8_t privateKey[32];   // RSA private key
    uint8_t sessionKey[32];   // derived shared SHA-256 session key
};

class LoraHashTable {
public:
    void init(); // Initialize hash table
    uint8_t addNode(uint32_t nodeID, uint32_t position, uint32_t timestamp, uint8_t* publicKey); // Add a node to the hash table
    void updateNode(uint32_t nodeID, uint32_t position, uint32_t timestamp, uint8_t* publicKey);  // Update a given nodes timestamp/information (needed?)
    void removeNode(uint32_t nodeID);  // Remove a node from the hash table
    uint32_t serialiseSelf(uint8_t* buffer, size_t length); // Broadcast own hashtable to other nodes (needed for new nodes)
    void updateTable(uint8_t* buffer, size_t length); // Update own table based on received buffer from other node
    void deriveNodeSessionKey(uint32_t nodeID); // Set the session key for a given node
    uint8_t* getNodeSessionKey(uint32_t nodeID); // Retrieve the session key for a given node
private:
    LoRaNode* table;
    size_t size;
};