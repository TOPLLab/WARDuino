#include <stdlib.h>
#include <string.h>
#include <thread>
#include <chrono>

#include "../../lib/ecdh/ecdh.h"

#include "encryption.h"
#include "TorCell.h"
#include "LoRaHash.h"

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
    uint32_t streamIDs[MAX_STREAMS] = {0}; //active stream IDs on this circuit
    uint8_t hopCount; //number of hops currently in the circuit (updated while creating)
    uint8_t totalHops; //number of total hops in the circuit (target)

    //uint16_t prevNodeID = 0; //indicates to which cell to send the response of extending the circuit, creates a sort of path from end to beginning
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

void sendAnnounce(LoraHashTable& table);
void handleAnnounce(ControlCell cell, LoraHashTable& table);

//CREATE & CREATED
void sendCreate(Circuit* circuit);
void handleCreate(Circuit* circuit, ControlCell cell);
void handleCreated(Circuit* circuit, ControlCell cell);

//DESTROY
void sendDestroy(uint16_t circID);
void handleDestroy(Circuit* circuit);

//RELAY EXTEND & EXTENDED
void sendExtend(Circuit* circuit);
void handleExtend(Circuit* circuit, ControlCell cell);
void handleExtended(Circuit* circuit, ControlCell cell);

//RELAY DATA
void sendData(Circuit* circuit, uint8_t* data, size_t length);
uint8_t* handleData(Circuit* circuit, RelayCell cell, uint8_t* buffer);

//MESSAGE DISPATCH
void LoRaMessageListener();
void dispatchRelay(Circuit* circuit, uint8_t* data);

//CIRCUIT BUILDING
Circuit* getCircuit(uint16_t circID); //circuit lookup function
int findCircuitSlot();
Circuit* buildCircuit(uint16_t destNodeID, uint8_t totalHops, LoraHashTable table, NodeRole myRole);


void createEntry(uint16_t prevCircID, uint16_t prevNodeID, uint16_t nextCircID, uint16_t nextNodeID);
ForwardingEntry* getForwardingEntry(uint16_t prevCircID);
ForwardingEntry* getReverseForwardingEntry(uint16_t nextCircID);