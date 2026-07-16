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
    char label = argv[1][0]; // first character of first argument: 'A', 'B',...

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
    //seed random number generator to the actual time
    srand(time(0));

    determineNode(argc, argv);

    radio_begin_extern(0, 0, 0, 0, 0, 0);

    LocalPeers.init(); //initialise the table of the local peers
    LocalPeers.addNode(nodeID, nodeRole, 0, 1000); //add self to the hash table
    sendAnnounce(LocalPeers); //send table information to other nodes, response is handled by LoRaMessageListener

    std::thread listenerThread(LoRaMessageListener); //main message-checking loop
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); //wait for listener

    if (builds == 1) {
        LocalPeers.printTable();

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

        const char* hello = "Hello World!";

        const char* incoming = "Third message incoming... It arrived!";


        printf("------------------------ build Circuit A - B - C - D -----------------------\n");
        Circuit* circuit = buildCircuit(444);
        if (circuit != nullptr) {
            while (circuit->state != CircuitState::READY) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            sendDataForwards(circuit, hello, strlen(hello) + 1);
        }


        printf("------------------------ build Circuit A - B - C -----------------------\n");
        Circuit* circuit2 = buildCircuit(333);
        if (circuit2 != nullptr) {
            while (circuit2->state != CircuitState::READY) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            sendDataForwards(circuit2, longmsg, strlen(longmsg) + 1);
        }
    

        printf("------------------------ build Circuit A - B -----------------------\n");
        Circuit* circuit3 = buildCircuit(222);
        if (circuit3 != nullptr) {
            while (circuit3->state != CircuitState::READY) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            sendDataForwards(circuit3, incoming, strlen(incoming) + 1);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        printf("------------------------ Destroy Circuit A - B - C - D -----------------------\n");
        sendDestroy(circuit);
        //not sent or received
        sendDataForwards(circuit, hello, strlen(hello) + 1);
    }

    listenerThread.join();

} 