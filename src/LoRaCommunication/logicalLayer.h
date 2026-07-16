#include <stdlib.h>

#include "LoRaHash.h"

struct LogicalAdjacencyMatrix {   
    uint16_t nodeIDs[MAX_NODES]; //map the index to the node ID
    //matrix of node IDs representing a connection between nodes. 
    // 0 means no connection, nodeID means connection to the node with that ID. 
    uint16_t Adjacency[MAX_NODES][MAX_NODES];
    uint16_t nodeCount; //number of nodes in the logical network
};

extern LogicalAdjacencyMatrix logicalNetwork;

void addLogicalConnection(uint16_t nodeA, uint16_t nodeB);
void clearLogicalConnections();
uint16_t determineRoute(uint16_t srcNodeID, uint16_t destNodeID, uint16_t* route, uint16_t maxHops);

