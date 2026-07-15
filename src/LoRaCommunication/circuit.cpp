#include "circuit.h"
#include "radio/radio.h"
#include <chrono>

//forward static function declarations
static void dispatchControl(ControlCell& cell, uint8_t* data);
static void dispatchRelay(Circuit* circuit, uint8_t* data, uint16_t circID);
static void fillControlCell(ControlCell& cell, uint16_t circID, CellCommand command, uint16_t srcNodeID, 
                    uint16_t destNodeID, uint8_t* payload, size_t payloadLength, uint8_t* digest, size_t digestLength);
static void fillRelayCell(RelayCell& cell, uint16_t circID, RelayCellCommand relayCommand, uint16_t srcNodeID, 
                    uint16_t destNodeID, size_t payloadLength);
static void serialiseAndSendCell(Cell* cell);
static Circuit* getCircuit(uint16_t circID);
static int findCircuitSlot();
static void createEntry(uint16_t prevCircID, uint16_t prevNodeID, uint16_t nextCircID, uint16_t nextNodeID);
static ForwardingEntry* getForwardingEntry(uint16_t prevCircID);
static ForwardingEntry* getReverseForwardingEntry(uint16_t nextCircID);

//Global circuit table: indicates the different circuits going through this node
Circuit CIRCUITS[CIRCUIT_AMOUNT];

//Global forwarding table: used by relay cells to know their previous and next node in the circuit
ForwardingEntry FORWARDING_TABLE[CIRCUIT_AMOUNT]; //max entries = max circuits; each node in the circuit has max 1 entry


// ------------------------------------ ANNOUNCE ------------------------------------ //

//Creates the information cell containing the newest information about its own node, and broadcasts that information to other nodes.
static void buildAndSendAnnounceCell(LoraHashTable& table, CellCommand command, uint16_t destNodeID) {
    ControlCell cell;
    fillControlCell(cell, 0, command, nodeID, destNodeID, nullptr, 0, nullptr, 0);
    uint8_t buffer[BYTE_COUNT];
    table.serialiseSelf(buffer, nodeID, sizeof(buffer)); //serialises itself
    memcpy(cell.payload, buffer, BYTE_COUNT);
    serialiseAndSendCell(&cell);
}

//broadcast to all nodes, simple abstraction above the buildAndSendAnnounceCell function.
void sendAnnounce(LoraHashTable& table) {
    printf("sendAnnounce\n");
    buildAndSendAnnounceCell(table, CellCommand::ANNOUNCE, GLOBAL_ID);
}

//receiver node returns an acknowledgement containing information on himself.
//simple abstraction above the buildAndSendAnnounceCell function.
static void sendAnnounceAck(LoraHashTable& table, uint16_t destNodeID) {
    printf("sendAnnounce acknowledgement\n");
    buildAndSendAnnounceCell(table, CellCommand::ANNOUNCE_ACK, destNodeID);
}

//given the (non-encrypted) information cell, updates its table of local peers.
static void updatePeersFromCell(ControlCell cell) {
    LocalPeers.updateTable(cell.payload);
    LocalPeers.printTable();
}

//Updates its local peers table and sends an acknowledgement.
static void handleAnnounce(ControlCell cell) {
    printf("handleAnnounce\n");
    updatePeersFromCell(cell);
    sendAnnounceAck(LocalPeers, cell.srcNodeID);
}

//To avoid infinite loops, the acknowledgement does not send information back.
static void handleAnnounceAck(ControlCell cell) {
    printf("handleAnnounce acknowledgement\n");
    updatePeersFromCell(cell);
}


// ------------------------------------ CREATE ------------------------------------ //

//function that initiates the first hop of a circuit by performing an ECDH key exchange
// with the first node and storing the derived session key.
void sendCreate(Circuit* circuit) {
    printf("sendCreate\n");
    //generate fresh ECDH key pair, new one for each hop. private stored in circuit until CREATED is received
    uint8_t publicKey[KEY_SIZE], privateKey[KEY_SIZE];
    ecdh_generate_keys(publicKey, privateKey);
    //the origin of the circuit always keeps the "ongoing" private key until the circuit has been extended once.
    memcpy(circuit->pendingPrivateKey, privateKey, KEY_SIZE); 
    //sends its own public key (non-encrypted) through the network.
    ControlCell createCell;
    fillControlCell(createCell, circuit->circID, CellCommand::CREATE, nodeID, 
        circuit->hopNodeIDs[0], publicKey, KEY_SIZE, nullptr, 0);
    serialiseAndSendCell(&createCell);
    //circuit state keeps track in which state the circuit is. Is useful to indicate when the circuit is fully built.
    circuit->state = CircuitState::AWAITING_CREATED;
}

//The receiver receives the origin's public key, generates own ECDH key pair, 
//derives shared session key and send information back in CREATED cell
static void handleCreate(Circuit* circuit, ControlCell cell) {
    printf("handleCreate\n");
    //initialises the circuit for the to-added cell.
    int idx = findCircuitSlot();
    if (idx == -1) {
        printf("No space for new circuit, dropping CREATE cell.\n");
        return;
    }
    circuit = &CIRCUITS[idx];
    circuit->circID = cell.circID;
    //generate fresh ECDH key pair and derive the shared session key
    uint8_t* theirPublicKey = cell.payload;
    uint8_t publicKey[KEY_SIZE], privateKey[KEY_SIZE], sessionKey[KEY_SIZE];
    ecdh_generate_keys(publicKey, privateKey);
    ecdh_shared_secret(privateKey, theirPublicKey, sessionKey);
    //hash the shared session key to prove integrity
    uint8_t digest[32];
    hashData(sessionKey, digest);
    //create the information cell to send back to the origin
    ControlCell createdCell;
    fillControlCell(createdCell, cell.circID, CellCommand::CREATED, nodeID, 
        cell.srcNodeID, publicKey, KEY_SIZE, digest, HASH_SIZE);
    serialiseAndSendCell(&createdCell);
    // Store the shared session key
    memcpy(circuit->hopSessionKeys[0].key, sessionKey, KEY_SIZE);
}

//Origin receives newly extended node's public key and verifies 
//the integrity of the derived session key using the digest.
static void handleCreated(Circuit* circuit, ControlCell cell) {
    printf("handleCreated\n");
    //derive shared session key
    uint8_t sessionKey[KEY_SIZE];
    ecdh_shared_secret(circuit->pendingPrivateKey, cell.payload, sessionKey); //payload is the public key of the extended node
    if(!verifyDigest(sessionKey, cell.digest)) {
        printf("Error: digest verification failed for circuit %d, dropping CREATED cell.\n", circuit->circID);
        return;
    }
    // clear private key, only the session key is needed
    memset(circuit->pendingPrivateKey, 0, KEY_SIZE); 
    memcpy(circuit->hopSessionKeys[circuit->hopCount].key, sessionKey, KEY_SIZE);
    circuit->hopCount++;
    //check whether the circuit is fully built
    if (circuit->hopCount < circuit->totalHops) {
        sendExtend(circuit);
    } else {
        circuit->state = CircuitState::READY;
        printf("!!! CIRCUIT %d WITH %d HOPS IS BUILT.\n", circuit->circID, circuit->hopCount);
    }
}


// ------------------------------------ DESTROY ------------------------------------ //

//function to send a DESTROY cell to the next hop in the circuit. Triggers the deletion of the whole circuit.
static void sendDestroyCell(uint16_t circID, uint16_t destNodeID) {
    printf("sendDestroy\n");
    ControlCell destroyCell;
    fillControlCell(destroyCell, circID, CellCommand::DESTROY, nodeID, destNodeID, nullptr, 0, nullptr, 0);
    serialiseAndSendCell(&destroyCell);
}

//function to clear session keys and hop information of a circuit, including the circuit object.
static void clearInternalCircuitState(Circuit* circuit) {
    if (circuit == nullptr) return;
    for (int i = 0; i < circuit->hopCount; i++) {
        memset(circuit->hopSessionKeys[i].key, 0, KEY_SIZE);
        memset(circuit->hopSessionKeys[i].nonce, 0, 16);
    }
    memset(circuit, 0, sizeof(Circuit));
}

//function to destroy a circuit beginning at the origin.
//all nodes in the circuit will delete their session keys and destroy their version of the circuit.
void sendDestroy(Circuit* circuit) {
    printf("sendDestroy\n");
    if (circuit == nullptr) return;
    sendDestroyCell(circuit->circID, circuit->hopNodeIDs[0]);
    clearInternalCircuitState(circuit);
}

//function to handle a received DESTROY cell.
static void handleDestroy(Circuit* circuit, uint16_t incomingCircID) {
    printf("handleDestroy\n");
    ForwardingEntry* entry = getForwardingEntry(incomingCircID);
    if (entry != nullptr) {
        sendDestroyCell(entry->nextCircID, entry->nextNodeID); //forward the destroy cell to the next node in the circuit
        memset(entry, 0, sizeof(ForwardingEntry)); //clear forwarding entry
    }
    clearInternalCircuitState(circuit);
    printf("Circuit %d destroyed.\n", incomingCircID);
}


// ------------------------------------ RELAY EXTEND ------------------------------------ //

//function to extend the circuit to the next node by performing an ECDH key exchange with the next node 
void sendExtend(Circuit* circuit) {
    printf("sendExtend\n");
    if (circuit->hopCount >= MAX_HOPS) {
        printf("Maximum hops reached for circuit %d, cannot extend further.\n", circuit->circID);
        return;
    };
    //generate fresh ECDH key pair, new one for each hop. private stored in circuit until EXTENDED is received
    uint8_t publicKey[KEY_SIZE], privateKey[KEY_SIZE];
    ecdh_generate_keys(publicKey, privateKey);
    memcpy(circuit->pendingPrivateKey, privateKey, KEY_SIZE);
    //Capture the ID of the next node in the circuit where to extend to
    uint16_t nextHopID = circuit->hopNodeIDs[circuit->hopCount];
    //create a RELAY EXTEND cell
    RelayCell extendCell;
    fillRelayCell(extendCell, circuit->circID, RelayCellCommand::EXTEND, nodeID, 
        circuit->hopNodeIDs[0], 2 + KEY_SIZE);
    //payload contains node address of the extending node (2 bytes) and origin's public key (32 bytes)
    memcpy(extendCell.payload, &nextHopID, 2);
    memcpy(extendCell.payload + 2, publicKey, KEY_SIZE);
    //compute checksum of payload
    uint8_t digest[HASH_SIZE];
    hashData(extendCell.payload, digest);
    memcpy(extendCell.digest, digest, HASH_SIZE);
    //send the RELAY EXTEND cell
    uint8_t data[CELL_SIZE];
    extendCell.serialise(data, CELL_SIZE);
    //encrypt the payload with the session keys of the previous hops in the circuit
    encryptOnion(circuit->hopCount, circuit->hopSessionKeys, data + RELAY_HEADER_SIZE, CELL_SIZE - RELAY_HEADER_SIZE);
    radio_transmit_bytes_extern(data, CELL_SIZE);
    circuit->state = CircuitState::AWAITING_EXTENDED;
}

//function to handle a received RELAY EXTEND cell. Is received by the actual last node 
//in the circuit, as only this node removes the last layer of encryption of the information cell.
//It sends a CREATE cell to the to-extend node with the origin's public key.
static void handleExtend(Circuit* circuit, RelayCell cell) {
    printf("handleExtend\n");
    //unencrypted payload contains node address of the extending node (2 bytes) and origin's public key (32 bytes)
    uint16_t nextNodeID;
    memcpy(&nextNodeID, cell.payload, 2);
    uint8_t* clientPubKey = cell.payload + 2;
    // Generate a fresh circID for the next node
    uint16_t newCircID = (rand() % 65536) + 1;
    // Store forwarding entry as: {incoming circID, incoming node} => {outgoing circID, outgoing node}
    createEntry(cell.circID, cell.srcNodeID, newCircID, nextNodeID);
    //sends the CREATE cell containing the origin's public key.
    ControlCell createCell;
    fillControlCell(createCell, newCircID, CellCommand::CREATE, nodeID, nextNodeID, clientPubKey, KEY_SIZE, nullptr, 0);
    serialiseAndSendCell(&createCell);
}

//forward an EXTENDED cell back to the previous node in the circuit towards the origin.
static void forwardCreated(Circuit* circuit, ControlCell cell) {
    printf("forwardCreated\n");
    ForwardingEntry* entry = getReverseForwardingEntry(cell.circID); //forwarding entry contains the previous circuit ID and previous node ID
    if (entry == nullptr) {
        printf("Error: no forwarding entry found for circuit %d, dropping CREATED cell.\n", cell.circID);
        return; // no entry (and thus no previous node in circuit) found, drop the information cell
    }
    ControlCell extendedCell;
    fillControlCell(extendedCell, entry->prevCircID, CellCommand::CREATED, nodeID, entry->prevNodeID, cell.payload, KEY_SIZE, cell.digest, HASH_SIZE);
    serialiseAndSendCell(&extendedCell);
}


// ------------------------------------ RELAY DATA ------------------------------------ //

//Sends data towards the other end of the circuit. If the data is larger than 
//the maximum payload size, it will be split into multiple cells and sent sequentially.
static void sendData(Circuit* circuit, uint16_t nextNodeID, uint16_t circID, uint8_t* data, 
                    size_t length, bool direction, RelayCellCommand command = DATA) {
    uint16_t maxPayload = CELL_SIZE - RELAY_HEADER_SIZE - HASH_SIZE;
    uint16_t offset = 0; //keep track of how much of the data has been sent
    while (offset < length) {
        uint16_t size = (length - offset) < maxPayload ? (length - offset) : maxPayload; //payload size
        //fill the information cell
        RelayCell dataCell;
        fillRelayCell(dataCell, circID, command, nodeID, nextNodeID, size);
        memcpy(dataCell.payload, data + offset, size);
        // write zeroes to remaining payload, prevent leakage of previous data if payload is smaller than max
        memset(dataCell.payload + size, 0, maxPayload - size);
        //hash the data for integrity checking
        uint8_t hash[HASH_SIZE];
        hashData(dataCell.payload, hash);
        memcpy(dataCell.digest, hash, HASH_SIZE);
        uint8_t buffer[CELL_SIZE];
        dataCell.serialise(buffer, CELL_SIZE);
        if (direction) {
            //if the data goes towards the last node in the circuit, the origin adds all 
            //the layers of encryption. Each node in the circuit will then remove its corresponding layer.
            encryptOnion(
                circuit->hopCount, 
                circuit->hopSessionKeys, 
                buffer + RELAY_HEADER_SIZE, 
                CELL_SIZE - RELAY_HEADER_SIZE
            );
        } else {
            //if the data goes towards the origin, each node adds 1 layer of encryption. 
            //The origin removes all the layers.
            encryptSingleHop(
                circuit->hopSessionKeys[0].key,
                circuit->hopSessionKeys[0].nonce,
                buffer + RELAY_HEADER_SIZE,
                CELL_SIZE - RELAY_HEADER_SIZE
            );
        }
        radio_transmit_bytes_extern(buffer, CELL_SIZE);
        offset += size;
    }
}

//function to send a RELAY DATA cell towards the end of the circuit.
void sendDataForwards(Circuit* circuit, uint8_t* data, size_t length) {
    printf("sendDataForwards\n");
    sendData(circuit, circuit->hopNodeIDs[0], circuit->circID, data, length, true);
}

//function to send a RELAY DATA cell towards the origin.
void sendDataBackwards(Circuit* circuit, uint16_t prevNodeID, uint16_t prevCircID, 
                        uint8_t* data, size_t length, RelayCellCommand command = DATA) {
    printf("sendDataBackwards\n");
    sendData(circuit, prevNodeID, prevCircID, data, length, false, command);
}

//checks the validity of the digest, and returns a buffer with the content of the message.
static uint8_t* handleData(Circuit* circuit, RelayCell cell, uint8_t* buffer) {
    printf("handleData\n");
    uint8_t digest[HASH_SIZE];
    memcpy(digest, cell.digest, HASH_SIZE);
    if (!verifyDigest(cell.payload, digest)) {
        printf("Error: digest verification failed for circuit %d, dropping DATA cell.\n", cell.circID);
        return nullptr; //invalid digest, drop message
    }
    memcpy(buffer, cell.payload, cell.payloadLength);
    return buffer;
}

//If the data is destined for the origin, it has to remove all the layers of encryption added by the nodes in the circuit.
static void handleDataAtOrigin(Circuit* circuit, uint8_t* copy) {
    decryptOnion(circuit->hopCount, circuit->hopSessionKeys,
                copy + RELAY_HEADER_SIZE, CELL_SIZE - RELAY_HEADER_SIZE);
    RelayCell copyCell = RelayCell::deserialise(copy, CELL_SIZE);
    uint8_t buffer[CELL_SIZE - RELAY_HEADER_SIZE - HASH_SIZE];
    uint8_t* result = handleData(circuit, copyCell, buffer);
    if (result != nullptr) {
        printf("Received data: %s\n", result);
    }
}

//If the data is destined for the exit (last node), all layers were already one by one removed by the other nodes in the circuit.
// sends an acknowledgement back to the origin.
static void handleDataAtExit(Circuit* circuit, RelayCell cell) {
    uint8_t buffer[CELL_SIZE - RELAY_HEADER_SIZE - HASH_SIZE];
    uint8_t* result = handleData(circuit, cell, buffer);
    if (result != nullptr) {
        printf("Circuit %d: Received data: %s\n", circuit->circID ,result);
        const char* msg = "Data received successfully!";
        sendDataBackwards(circuit, cell.srcNodeID, cell.circID, (uint8_t*)msg, strlen(msg) + 1, DATA_ACK);
    }
}


// ------------------------------------ BUILD CIRCUIT ------------------------------------ //

//Builds a circuit from caller node to destination node.
Circuit* buildCircuit(uint16_t destNodeID, uint8_t totalHops, LoraHashTable table, NodeRole role) {
    if (totalHops < 1 || totalHops > MAX_HOPS) {
        printf("Error: invalid amount of hops: %d\n", totalHops);
        return nullptr;
    }
    //find empty slot for new circuit
    int idx = findCircuitSlot();
    if (idx == -1) return nullptr;
    //initalise the circuit object of the origin
    Circuit* circuit = &CIRCUITS[idx];
    circuit->circID = (rand() % 65536) + 1;
    circuit->hopCount = 0;
    circuit->totalHops = totalHops;
    circuit->state = CircuitState::IDLE;

    //create the logical network. Add all node's IDs and their edges.
    logicalNetwork.nodeCount = 4;
    logicalNetwork.nodeIDs[0] = 111;
    logicalNetwork.nodeIDs[1] = 222;
    logicalNetwork.nodeIDs[2] = 333;
    logicalNetwork.nodeIDs[3] = 444;
    addLogicalConnection(111, 222);
    addLogicalConnection(222, 333);
    addLogicalConnection(444, 333);
    //build the route for the circuit
    uint16_t route[4];
    uint16_t routeLength = determineRoute(111, 444, route, 4);
    if (routeLength == 0) {
        printf("Error: no route found from %d to %d.\n", 111, 444);
        return nullptr;
    }
    for (uint16_t i = 0; i < routeLength && route[i] != 0; i++) {
        circuit->hopNodeIDs[i] = route[i];
    }
    //send a create cell to the second node in the circuit that will snowball into creating the whole circuit
    sendCreate(circuit);
    return circuit;
}


// ------------------------------------ MESSAGE DISPATCH: CONTROL CELLS ------------------------------------ //

//Internal loop of the node: reads incoming information nodes and handles them accordingly.
void LoRaMessageListener() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        //awaiting the receipt of a message
        uint8_t data[CELL_SIZE];
        uint16_t state = radio_receive_bytes_extern(data, CELL_SIZE);
        if (state != 0) continue;
        //read the non-encrypted part of the message
        ControlCell controlCell = ControlCell::deserialise(data, CELL_SIZE);
        CellCommand command = controlCell.command;
        uint16_t destNodeID = controlCell.destNodeID;
        //broadcast-level commands, no circuit needed
        if (command == CellCommand::ANNOUNCE) {
            handleAnnounce(controlCell);
            continue;
        }
        if (command == CellCommand::ANNOUNCE_ACK && destNodeID == nodeID) {
            handleAnnounceAck(controlCell); //targeted acknowledgement
            continue;
        }
        //Drop anything not addressed to us
        printf("Received cell destNodeID: %d, my nodeID: %d\n", destNodeID, nodeID);
        if (destNodeID != nodeID) continue;
        //dispatch the message based on its control command.
        dispatchControl(controlCell, data);
    }
}

//dispatch procedure that handles control cells differently based on their command
static void dispatchControl(ControlCell& cell, uint8_t* data) {
    uint16_t circID = cell.circID;
    Circuit* circuit = getCircuit(circID);
    switch (cell.command) {
    case CREATE:
        handleCreate(circuit, cell);
        break;
    case CREATED: {
        //created travels back to origin
        ForwardingEntry* entry = getReverseForwardingEntry(circID);
        if (entry != nullptr) {
            // We are a relay, forward it back
            circuit = getCircuit(entry->prevCircID);
            if (circuit == nullptr) break; // no circuit, drop
            forwardCreated(circuit, cell); //forward CREATED cell back to previous node in circuit
        } else {
            //Node is origin: handle CREATED
            if (circuit == nullptr) break; // no circuit, drop
            handleCreated(circuit, cell);
        }
        break;
    }
    case DESTROY:
        handleDestroy(circuit, circID);
        break;
    case RELAY:
        dispatchRelay(circuit, data, circID);
        break;
    default:
        break;
    }
}


// ------------------------------------ MESSAGE DISPATCH: RELAY CELLS ------------------------------------ //

//When a relay cell forwards information to the origin or exit, it rewrites relay 
//information such as the next circuit ID, next node's ID and its own ID. This is 
//crucial for message traversing through the circuit.
static void rewriteCellHeader(uint8_t* data, uint16_t circID, uint16_t nextNodeID, uint16_t srcNodeID) {
    data[0] = (circID >> 8) & 0xFF;
    data[1] =  circID & 0xFF;
    data[3] = (nextNodeID >> 8) & 0xFF;
    data[4] =  nextNodeID & 0xFF;
    data[5] = (srcNodeID >> 8) & 0xFF;
    data[6] =  srcNodeID & 0xFF;
}

//sends the information cell to the next node in the circuit.
static void forwardToNextHop(uint8_t* data, ForwardingEntry* entry) {
    rewriteCellHeader(data, entry->nextCircID, entry->nextNodeID, nodeID);
    radio_transmit_bytes_extern(data, CELL_SIZE);
}

//sends the information cell to the previous node in the circuit. The node
//needs to encrypt the data with 1 layer of encryption (crucial for data anonymity).
static void forwardToPrevHop(uint8_t* copy, ForwardingEntry* entry, HopSecretKey& key) {
    rewriteCellHeader(copy, entry->prevCircID, entry->prevNodeID, nodeID);
    encryptSingleHop(key.key, key.nonce, copy + RELAY_HEADER_SIZE, CELL_SIZE - RELAY_HEADER_SIZE);
    radio_transmit_bytes_extern(copy, CELL_SIZE);
}

//As a relay node ,given a relay cell, checks what it needs to do with it. It may send it forwards, backwards.
//Origin nodes may receive encrypted data they don't directly understand. At that point, it will need to 
//decrypt all layers of information.
static void ForwardRelayCell(Circuit* circuit, uint8_t* data, uint8_t* copy, uint16_t incomingCircID, uint16_t incomingSrcNodeID) {
    ForwardingEntry* entry = getForwardingEntry(incomingCircID);
    if (entry != nullptr && incomingSrcNodeID == entry->prevNodeID) {
            if (entry == nullptr) return;
            // rewrite circID and dest towards next hop in circuit
            forwardToNextHop(data, entry);
        } else {
            // reverse lookup toward prev hop
            entry = getReverseForwardingEntry(incomingCircID);
            if (entry == nullptr) {
                //DATA forwarded towards origin, decrypt all layers
                handleDataAtOrigin(circuit, copy);
                return;
            }
            forwardToPrevHop(copy, entry, circuit->hopSessionKeys[0]);
        }
        return;
}

//Dispatcher method for RELAY cells.
static void dispatchRelay(Circuit* circuit, uint8_t* data, uint16_t circID) {
    printf("dispatchRelay\n");
    if(circuit == nullptr) {
        //data could be forwarding from the last node: check if given circID is in the forwarding table
        ForwardingEntry* entry = getReverseForwardingEntry(circID);
        if (entry == nullptr) return; // no forwarding entry, drop message
        circuit = getCircuit(entry->prevCircID);
        if (circuit == nullptr) return; // no circuit, drop
    }
    //captures information about where the data is coming from
    uint16_t incomingCircID = (data[0] << 8) | data[1];
    uint16_t incomingSrcNodeID = (data[5] << 8) | data[6];
    //makes a copy of the received data. Nodes always unencrypts a layer to reach the data's information.
    //In some cases, it should not unencrypt this layer. But the node only knows this after unencrypting the layer.
    uint8_t copy[CELL_SIZE];
    memcpy(copy, data, CELL_SIZE);
    //decrypt the payload region
    HopSecretKey circuitKey = circuit->hopSessionKeys[0];
    decryptSingleHop(circuitKey.key, circuitKey.nonce, data + RELAY_HEADER_SIZE, CELL_SIZE - RELAY_HEADER_SIZE);
    RelayCell cell = RelayCell::deserialise(data, CELL_SIZE);
    //checks if this cell is meant for us
    uint8_t hash[HASH_SIZE];
    hashData(cell.payload, hash);
    bool digestMatch = (memcmp(hash, cell.digest, HASH_SIZE) == 0);
    printf("digest match: %d\n", digestMatch);
    printf("relayCommand: %d payloadLength: %d\n", cell.relayCommand, cell.payloadLength);
    //If the cell is not meant for us, forward it.
    if (!digestMatch) {
        ForwardRelayCell(circuit, data, copy, incomingCircID, incomingSrcNodeID);
        return;
    }
    printf("Relay command: %d\n", cell.relayCommand);
    //actual dispatch
    switch (cell.relayCommand) {
        case EXTEND:
            handleExtend(circuit, cell);
            break;
        //case EXTENDED:
            //handleExtended(circuit, cell);
            //break;
        case DATA:
            handleDataAtExit(circuit, cell);
            break;
        case DATA_ACK:
            handleDataAtOrigin(circuit, copy);
            break;
        default:
            break;
    }
}


// ------------------------------------ HELPFUNCTIONS ------------------------------------ //

//abstraction function to fill a control cell with the given parameters
static void fillControlCell(
    ControlCell& cell, 
    uint16_t circID, 
    CellCommand command, 
    uint16_t srcNodeID, 
    uint16_t destNodeID, 
    uint8_t* payload, size_t payloadLength,
    uint8_t* digest, size_t digestLength
) {
    cell.circID = circID;
    cell.command = command;
    cell.srcNodeID = srcNodeID;
    cell.destNodeID = destNodeID;
    if (payload != nullptr && payloadLength > 0) {
        memcpy(cell.payload, payload, payloadLength);
    }
    if (digest != nullptr && digestLength > 0) {
        memcpy(cell.digest, digest, digestLength);
    }
}

//abstraction function to fill a relay cell with the given parameters
static void fillRelayCell(
    RelayCell& cell, 
    uint16_t circID, 
    RelayCellCommand relayCommand, 
    uint16_t srcNodeID, 
    uint16_t destNodeID, 
    size_t payloadLength
) {
    cell.circID = circID;
    cell.command = CellCommand::RELAY;
    cell.relayCommand = relayCommand;
    cell.srcNodeID = srcNodeID;
    cell.destNodeID = destNodeID;
    cell.payloadLength = payloadLength;
}

//Abstracts the serialising and transmitting of information cells
static void serialiseAndSendCell(Cell* cell) {
    uint8_t buffer[CELL_SIZE];
    cell->serialise(buffer, CELL_SIZE);
    radio_transmit_bytes_extern(buffer, CELL_SIZE);
}

//function to look up a circuit object using its ID
static Circuit* getCircuit(uint16_t circID) {
    for (int i = 0; i < CIRCUIT_AMOUNT; i++) {
        if (CIRCUITS[i].circID == circID) {
            return &CIRCUITS[i];
        }
    }
    return nullptr;
}

//function to find an empty circuit slot
static int findCircuitSlot() {
    for (size_t i = 0; i < CIRCUIT_AMOUNT; i++) {
        if (CIRCUITS[i].circID == 0) return i; //empty slot
    }
    return -1; //no empty slot
}

//creates an entry for a certain node. Such an entry contains the previous and 
//next node (together with their respective circuit IDs) of a node. That way, 
//it can easily forward information to them, without knowing the structure of the total circuit.
static void createEntry(uint16_t prevCircID, uint16_t prevNodeID, uint16_t nextCircID, uint16_t nextNodeID) {
    for (size_t i = 0; i < CIRCUIT_AMOUNT * MAX_HOPS; i++) {
        if (FORWARDING_TABLE[i].prevCircID == 0) { //empty slot
            FORWARDING_TABLE[i].prevCircID = prevCircID;
            FORWARDING_TABLE[i].prevNodeID = prevNodeID;
            FORWARDING_TABLE[i].nextCircID = nextCircID;
            FORWARDING_TABLE[i].nextNodeID = nextNodeID;
            printf("prevCircID: %d, prevNodeID: %d, nextCircID: %d, nextNodeID: %d\n", FORWARDING_TABLE[i].prevCircID, FORWARDING_TABLE[i].prevNodeID, FORWARDING_TABLE[i].nextCircID, FORWARDING_TABLE[i].nextNodeID);
            return;
        }
    }
}

//lookup function to find the forwarding entry based on the circuit ID of the previous node.
static ForwardingEntry* getForwardingEntry(uint16_t prevCircID) {
    for (size_t i = 0; i < CIRCUIT_AMOUNT * MAX_HOPS; i++) {
        if (FORWARDING_TABLE[i].prevCircID == prevCircID) {
            return &FORWARDING_TABLE[i];
        }
    }
    return nullptr; //not found
}

//reverse lookup function to find the forwarding entry based on the circuit ID of the next node.
static ForwardingEntry* getReverseForwardingEntry(uint16_t nextCircID) {
    for (size_t i = 0; i < CIRCUIT_AMOUNT * MAX_HOPS; i++) {
        if (FORWARDING_TABLE[i].nextCircID == nextCircID) {
            return &FORWARDING_TABLE[i];
        }
    }
    return nullptr; //not found
}