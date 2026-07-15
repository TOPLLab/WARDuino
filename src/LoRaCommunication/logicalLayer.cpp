#include "logicalLayer.h"
#include <queue>

LogicalAdjacencyMatrix logicalNetwork;

//finds the index of a node in the matrix given its ID.
static int16_t findNodeIndex(uint16_t nodeID) {
    for (uint16_t i = 0; i < logicalNetwork.nodeCount; i++) {
        if (logicalNetwork.nodeIDs[i] == nodeID) {
            return i;
        }
    }
    return -1; // Node not found
}

//adds a undirected edge between two nodes.
void addLogicalConnection(uint16_t nodeA, uint16_t nodeB) {
    int16_t indexA = findNodeIndex(nodeA);
    int16_t indexB = findNodeIndex(nodeB);
    if (indexA != -1 && indexB != -1) {
        logicalNetwork.Adjacency[indexA][indexB] = nodeB;
        logicalNetwork.Adjacency[indexB][indexA] = nodeA; //undirected connection
    }
}

//clears the logical adjacency matrix, useful for testing multiple scenarios.
void clearLogicalConnections() {
    for (uint16_t i = 0; i < logicalNetwork.nodeCount; i++) {
        for (uint16_t j = 0; j < logicalNetwork.nodeCount; j++) {
            logicalNetwork.Adjacency[i][j] = 0;
        }
    }
    logicalNetwork.nodeCount = 0;
}

//BFS algorithm to find a route from source to destination, inspired by Algorithms and Data Structures 2: chapter 10: Graph Traversals
uint16_t determineRoute(uint16_t srcNodeID, uint16_t destNodeID, uint16_t* route, uint16_t maxHops) {
    int16_t srcIndex = findNodeIndex(srcNodeID);
    int16_t destIndex = findNodeIndex(destNodeID);
    if (srcIndex == -1 || destIndex == -1) {
        printf("Error: Source or destination node not found in logical network.\n");
        return 0;
    }
    //method: traverse the matrix using BFS, 
    // mark nodes as being visited to terminate the search (no loops), 
    // and keep track of the previous node to reconstruct the path.
    std::queue<uint16_t> q;
    uint8_t visited[MAX_NODES] = {0}; // 1 means visited, 0 means not visited
    int16_t prev[MAX_NODES]; //retains the list of previous nodes towards the source
    for (uint16_t i = 0; i < MAX_NODES; i++) {
        prev[i] = -1;
    }
    //start at source
    q.push(srcIndex);
    visited[srcIndex] = 1;
    //BFS loop
    while (!q.empty()) {
        uint16_t current = q.front();
        q.pop();
        
        // Found the destination
        if (current == destIndex) {
            break;
        }

        for (uint16_t neighbor = 0; neighbor < logicalNetwork.nodeCount; neighbor++) {
            //check if there's a connection and if the neighbor hasn't been visited
            if (logicalNetwork.Adjacency[current][neighbor] != 0 && visited[neighbor] == 0) {
                visited[neighbor] = 1;
                prev[neighbor] = current;
                q.push(neighbor);
            }
        }
    }
    //destination was never reached
    if (visited[destIndex] == 0) {
        printf("Error: no logical route from %d to %d.\n", srcNodeID, destNodeID);
        return 0;
    }
    //create the path from destination to source
    uint16_t reverseRoute[MAX_NODES];
    uint16_t pathLength = 0;
    for (int16_t current = destIndex; current != srcIndex; current = prev[current]) {
        reverseRoute[pathLength] = logicalNetwork.nodeIDs[current];
        pathLength++;
        if (pathLength > maxHops) {
            printf("Error: route from %d to %d exceeds maxHops.\n", srcNodeID, destNodeID);
            return 0;
        }
    }
    //path from source to destination, swapping of positions
    for (uint8_t i = 0; i < pathLength; i++)
        route[i] = reverseRoute[pathLength - 1 - i]; 
    return pathLength;
}
