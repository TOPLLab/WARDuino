#include <cstdint>
#include <cstring>
#include <thread>
#include <chrono>
#include <random>
#include <format>
#include "circuit.h"
#include "radio/radio.h"

LoraHashTable LocalPeers; //hash table of nodes in local group, used by all noddes
LoraHashTable RouterPeers; //hash table of all other routers, used by router nodes
uint16_t nodeID;
uint8_t builds; //indicates if the node builds a circuit
NodeRole nodeRole;


//method to generate random IDs found on internet (https://www.youtube.com/watch?v=IUoqMTGGo6k&t=5s).
uint64_t generateRandomNumber() {
    std::random_device rd; //random number from hardware
    std::mt19937 gen(rd()); // seed the generator
    std::uniform_int_distribution<uint64_t> distribution(1, 65534); //65535 is reserved as GLOBAL_ID
    return distribution(gen); 
}


//initialises node's ID and role
void determineNode(int argc, char* argv[]) {
    char label = argv[1][0];
    nodeID = generateRandomNumber();
    nodeRole = LOCAL;
    builds = 0; //decided on later, for now all nodes don't create circuits
    printf("Node label: %c starting with random ID: %d\n", label, nodeID);
}


//determines the creators of circuits
void determineBuilders(uint16_t expectedNodeCount, uint16_t* IDs) {
    //determine which nodes will build circuits, based on their IDs
    uint16_t buildersAmount = (expectedNodeCount / 2) + 1; //majority of the nodes will build circuits
    for (uint16_t i = 0; i < buildersAmount; i++) {
        if (IDs[i] == nodeID) {
            builds = 1; //specific own node is a builder, every node checks for itself
            printf("Node %d is a circuit builder.\n", nodeID);
            return;
        }
    }
    printf("Node %d is not a circuit builder.\n", nodeID); //if not in the builder list, it's not a builder
}


//build a circuit to a specific node and send a message to it
void sendToNode(uint16_t destNodeID) {
    printf("------------------------ build Circuit to %d -----------------------\n", destNodeID);
    std::string string = "Message from node " + std::to_string(nodeID) + " to node " + std::to_string(destNodeID);
    const char* msg = string.c_str();
    Circuit* circuit = buildCircuit(destNodeID);
    if (circuit != nullptr) {
        while (circuit->state != CircuitState::READY) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        sendDataForwards(circuit, msg, strlen(msg) + 1);
    }
}


//send a message to all other nodes in the network
void sendToAllOtherNodes(uint16_t* IDs, uint16_t nodesAmount) {
    for (uint16_t i = 0; i < nodesAmount; i++) {
        if (IDs[i] == nodeID) {
            continue; //skip self
        }
        sendToNode(IDs[i]);
    }
}


//send a message to a targeted node in the logical network
void sendToSameNode(uint16_t targetID) {
    if (targetID != nodeID) {
        sendToNode(targetID);
    }
}


//build a linear logical network based on the sorted IDs of the nodes
void linearLogicalNetwork(uint16_t* IDs, uint16_t nodesAmount) {
    clearLogicalConnections();
    logicalNetwork.nodeCount = nodesAmount;
    for (uint16_t i = 0; i < nodesAmount; i++) {
        logicalNetwork.nodeIDs[i] = IDs[i];
    }
    for (uint16_t i = 0; i < nodesAmount - 1; i++) { //straight line connection based on sorted IDs
        addLogicalConnection(IDs[i], IDs[i + 1]);
    }
}

//build a complex logical network for 8 nodes: ressembles this structure:
//     C   -   G
//   /         |
// A - B - F - H         with each letter representing a node
// |   |  /
// D    E
void complexlogicalNetworkEightNodes(uint16_t* IDs, uint16_t nodesAmount) {
    clearLogicalConnections();
    logicalNetwork.nodeCount = nodesAmount;
    for (uint16_t i = 0; i < nodesAmount; i++) {
        logicalNetwork.nodeIDs[i] = IDs[i];
    }
    addLogicalConnection(IDs[0], IDs[1]);
    addLogicalConnection(IDs[0], IDs[2]); 
    addLogicalConnection(IDs[0], IDs[3]);
    
    addLogicalConnection(IDs[1], IDs[4]);
    addLogicalConnection(IDs[1], IDs[5]);

    addLogicalConnection(IDs[2], IDs[6]);

    addLogicalConnection(IDs[4], IDs[5]);

    addLogicalConnection(IDs[5], IDs[7]);

    addLogicalConnection(IDs[6], IDs[7]);
}






int main(int argc, char* argv[]) {
    if (argc < 3) { printf("Usage: ./node_A A expected_node_count\n"); return 1; }
    //seed random number generator to the actual time
    srand(time(0));

    //capture the expected node count from command line arguments, used to know when to start circuit-building
    const uint16_t maxExpectedNodeCount = 16;
    uint16_t expectedNodeCount = atoi(argv[2]);

    determineNode(argc, argv);

    radio_begin_extern(0, 0, 0, 0, 0, 0);

    LocalPeers.init(); //initialise the table of the local peers
    LocalPeers.addNode(nodeID, nodeRole, 0, 1000); //add self to the hash table
    sendAnnounce(LocalPeers); //send table information to other nodes, response is handled by LoRaMessageListener

    std::thread listenerThread(LoRaMessageListener); //main message-checking loop
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); //wait for listener

    //wait for enough nodes to be announced before building circuits.
    while (LocalPeers.countNodes() < expectedNodeCount) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    printf("Enough nodes have been announced, starting circuit-building...\n");

    uint16_t IDs[maxExpectedNodeCount];
    uint16_t nodesAmount = LocalPeers.sortedNodeIDs(IDs, sizeof(IDs)/sizeof(IDs[0])); //sorted IDs of peers, including self

    determineBuilders(expectedNodeCount, IDs); //decide which nodes will build circuits, based on their IDs

    if (builds == 1) {
        LocalPeers.printTable();

        //linearLogicalNetwork(IDs, nodesAmount); //linear logical network based on increasing IDs
        complexlogicalNetworkEightNodes(IDs, nodesAmount); //complex logical network for 8 nodes

        //sendToAllOtherNodes(IDs, nodesAmount); //send messages to all other nodes
        sendToSameNode(IDs[2]); //send messages to the same node in the logical network
    }

    listenerThread.join();
}







/*
const char* longmsg = 
"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
*/