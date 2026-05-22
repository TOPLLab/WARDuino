#include <cstdint>
#include <cstring>
#include <thread>
#include <chrono>
#include "circuit.h"
#include "radio/radio.h"

LoraHashTable LocalPeers; //hash table of nodes in local group, used by all noddes
LoraHashTable RouterPeers; //hash table of all other routers, used by router nodes
uint16_t nodeID;
uint8_t builds; //indicates if the node builds a circuit
NodeRole nodeRole;

//determine which node the program belongs to (UDP)
void determineNode(int argc, char* argv[]) {
    char label = argv[1][0]; // first character of first argument: 'A', 'B', or 'C'

    if (label == 'A') {
        nodeID = 111;
        nodeRole = LOCAL;
        builds = 1;
    } else if (label == 'B') {
        nodeID = 222;
        nodeRole = LOCAL;
        builds = 0;
    } else if (label == 'C') {
        nodeID = 333;
        nodeRole = LOCAL;
        builds = 0;
    } else if (label == 'D') {
        nodeID = 444;
        nodeRole = LOCAL;
        builds = 0;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) { printf("Usage: ./node_A A\n"); return 1; }
    determineNode(argc, argv);

    radio_begin_extern(0, 0, 0, 0, 0, 0);

    LocalPeers.init();
    uint8_t publicKey[32] = {0}; // replace with real public key
    LocalPeers.addNode(nodeID, nodeRole, 0, 1000); //add self to the hash table
    sendAnnounce(LocalPeers); //send table information to other nodes, response is handled by LoRaMessageListener

    std::thread listenerThread(LoRaMessageListener); //main message-checking loop
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); //wait for listener

    if (builds == 1) {
        printf("build Circuit\n");
        Circuit* circuit = buildCircuit(333, 2, LocalPeers, nodeRole);

        while (circuit->state != CircuitState::READY) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        LocalPeers.printTable();
        const char* msg = "Hello, World!";
        sendData(circuit, (uint8_t*)msg, strlen(msg) + 1);
    }

    listenerThread.join();
} 