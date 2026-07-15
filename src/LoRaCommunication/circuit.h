#include <stdlib.h>
#include <string.h>
#include <thread>
#include <chrono>

#include "../../lib/ecdh/ecdh.h"

#include "encryption.h"
#include "TorCell.h"
//#include "LoRaHash.h"
#include "logicalLayer.h"

#define GLOBAL_ID 65535
#define MAX_HOPS 4
#define MAX_STREAMS 5
#define KEY_SIZE ECC_PUB_KEY_SIZE //size of ECDH keys
#define CIRCUIT_AMOUNT 32

extern uint16_t nodeID;
extern NodeRole nodeRole;
extern LoraHashTable LocalPeers;

enum CircuitState {
    IDLE,               // not in use / no circuit
    AWAITING_CREATED,   // sent CREATE, waiting for CREATED
    AWAITING_EXTENDED,  // sent EXTEND, waiting for EXTENDED
    READY               // fully built
};

struct Circuit {
    uint16_t circID; //circuit ID
    CircuitState state = CircuitState::IDLE; //Keeps the state of the circuit while building, indicates when sendExtend must be sent
    uint8_t hopCount; //number of hops currently in the circuit (updated while creating)
    uint8_t totalHops; //number of total hops in the circuit (target)
    uint8_t pendingPrivateKey[KEY_SIZE]; // store private key between sending and handling

    //circuit nodes information
    HopSecretKey hopSessionKeys[MAX_HOPS]; //SESSION keys of each hop
    uint16_t hopNodeIDs[MAX_HOPS]; //nodeID per hop
};

//represents an entry for forwarding cells from node to node in the circuit
struct ForwardingEntry {
    uint16_t prevCircID;
    uint16_t prevNodeID;
    uint16_t nextCircID;
    uint16_t nextNodeID;
};

/** circuits datastructure */
extern Circuit CIRCUITS[CIRCUIT_AMOUNT]; //array to store active circuits, indexed by circuit ID

Circuit* buildCircuit(uint16_t destNodeID, uint8_t totalHops, LoraHashTable table, NodeRole myRole);
void sendAnnounce(LoraHashTable& table);
void sendCreate(Circuit* circuit);
void sendDestroy(Circuit* circuit);
void sendExtend(Circuit* circuit);
void sendDataForwards(Circuit* circuit, uint8_t* data, size_t length);
void LoRaMessageListener();