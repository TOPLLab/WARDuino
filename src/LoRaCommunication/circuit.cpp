#include "circuit.h"
#include "radio/radio.h"

//Global circuit table
Circuit CIRCUITS[CIRCUIT_AMOUNT];

//Global forwarding table
ForwardingEntry FORWARDING_TABLE[CIRCUIT_AMOUNT * MAX_HOPS]; //max entries = max circuits * max hops per circuit


//ANNOUNCE AND ANNOUNCED

void sendAnnounce(LoraHashTable& table) {
    printf("sendAnnounce\n");

    ControlCell cell;
    cell.circID = 0; // no circuit
    cell.destNodeID = 0;
    cell.srcNodeID = nodeID;
    cell.command = CellCommand::ANNOUNCE;
    cell.destNodeID = 0; // broadcast, no specific destination
    uint8_t buffer[CELL_SIZE - CONTROL_HEADER_SIZE - HASH_SIZE];
    table.serialiseSelf(buffer, sizeof(buffer));
    memcpy(cell.payload, buffer, sizeof(buffer));

    uint8_t data[CELL_SIZE];
    cell.serialise(data, CELL_SIZE);
    radio_transmit_bytes_extern(data, CELL_SIZE);
}


void handleAnnounce(ControlCell cell, LoraHashTable& table) {
    printf("handleAnnounce\n");
    
    table.updateTable(cell.payload, CELL_SIZE - CONTROL_HEADER_SIZE - HASH_SIZE);
    table.printTable();
}



//CREATE & CREATED

//function that initiates the first hop of a circuit by performing an ECDH key exchange
// with the first node and storing the derived session key.
void sendCreate(Circuit* circuit) {
    printf("sendCreate\n");

    //generate fresh ECDH key pair, new one for each hop. private stored in circuit until CREATED is received
    uint8_t publicKey[KEY_SIZE];
    uint8_t privateKey[KEY_SIZE];
    ecdh_generate_keys(publicKey, privateKey);
    memcpy(circuit->pendingPrivateKey, privateKey, KEY_SIZE);

    printf("Public key: ");
    for (size_t i = 0; i < KEY_SIZE; i++) printf("%02x", publicKey[i]);
    printf("\n");
    
    ControlCell createCell;
    createCell.circID = circuit->circID;
    createCell.command = CellCommand::CREATE;
    createCell.srcNodeID = nodeID;
    createCell.destNodeID = circuit->hopNodeIDs[0];
    memcpy(createCell.payload, publicKey, KEY_SIZE); //first part of Diffie-Hellman key exchange (public key)

    uint8_t data[CELL_SIZE];
    createCell.serialise(data, CELL_SIZE);
    radio_transmit_bytes_extern(data, CELL_SIZE);

    circuit->state = CircuitState::AWAITING_CREATED;
}


//receive their public key, generate own, 
// derive shared session key and send information back in CREATED cell
void handleCreate(Circuit* circuit, ControlCell cell) {
    printf("handleCreate\n");
    uint8_t* theirPublicKey = cell.payload;
    uint8_t publicKey[KEY_SIZE], privateKey[KEY_SIZE], sessionKey[KEY_SIZE];
    ecdh_generate_keys(publicKey, privateKey);
    ecdh_shared_secret(privateKey, theirPublicKey, sessionKey);

    printf("session key: ");
    for (size_t i = 0; i < KEY_SIZE; i++) printf("%02x", sessionKey[i]);
    printf("\n");

    uint8_t digest[32];
    hashData(sessionKey, digest);

    //createEntry(circuit->circID, cell.srcNodeID, cell.circID, cell.srcNodeID); //store the previous node ID to know where to send responses

    ControlCell createdCell;
    createdCell.circID = cell.circID;
    createdCell.command = CellCommand::CREATED;
    createdCell.srcNodeID = nodeID;
    createdCell.destNodeID = cell.srcNodeID;
    memcpy(createdCell.payload, publicKey, KEY_SIZE);
    memcpy(createdCell.digest, digest, HASH_SIZE);

    uint8_t data[CELL_SIZE];
    createdCell.serialise(data, CELL_SIZE);
    radio_transmit_bytes_extern(data, CELL_SIZE);

    // Store the key for this circuit
    memcpy(circuit->hopSessionKeys[0].key, sessionKey, KEY_SIZE);
    circuit->hopCount = 1;
}


//function to handle CREATED cell. 
//extract sender's public key and digest, verify digest and store session key
void handleCreated(Circuit* circuit, ControlCell cell) {
    printf("handleCreated\n");
    uint8_t theirPublicKey[KEY_SIZE];
    uint8_t digest[HASH_SIZE];
    memcpy(theirPublicKey, cell.payload, KEY_SIZE);
    memcpy(digest, cell.digest, HASH_SIZE);

    //derive shared secret key
    uint8_t sessionKey[KEY_SIZE];
    ecdh_shared_secret(circuit->pendingPrivateKey, theirPublicKey, sessionKey);

    printf("session key: ");
    for (size_t i = 0; i < KEY_SIZE; i++) printf("%02x", sessionKey[i]);
    printf("\n");

    //invalid digest, drop message
    if(!verifyDigest(sessionKey, digest)) return;

    // clear private key, only sessionKey is now needed
    memset(circuit->pendingPrivateKey, 0, KEY_SIZE); 

    //store session key for first hop
    memcpy(circuit->hopSessionKeys[0].key, sessionKey, KEY_SIZE);
    circuit->hopCount = 1;

    // Now check if we need to extend further
    if (circuit->hopCount < circuit->totalHops) {
        sendExtend(circuit); // triggers the next hop
    } else {    
        circuit->state = CircuitState::READY;
        printf("Circuit %d with %d hops is built.\n", circuit->circID, circuit->hopCount);
    }
}



//DESTROY

//function to send a DESTROY cell to the next hop in the circuit.
//all nodes in the circuit will delete their session keys and destroy the circuit.
void sendDestroy(uint16_t circID) {
    printf("sendDestroy\n");
    ControlCell destroyCell;
    destroyCell.circID = circID;
    destroyCell.command = CellCommand::DESTROY;
    memset(destroyCell.payload, 0, sizeof(destroyCell.payload));
    uint8_t data[CELL_SIZE];
    destroyCell.serialise(data, CELL_SIZE);
    radio_transmit_bytes_extern(data, CELL_SIZE);
}

//function to handle a received DESTROY cell by deleting session keys and destroying the circuit.
void handleDestroy(Circuit* circuit) {
    printf("handleDestroy\n");
    for (int i = 0; i < circuit->hopCount; i++) {
        memset(circuit->hopSessionKeys[i].key, 0, KEY_SIZE);
        memset(circuit->hopSessionKeys[i].nonce, 0, 16);
    }
    //how to know when to terminate?
    sendDestroy(circuit->circID); //FIX: next hop's circID

    circuit->hopCount = 0;
    circuit->circID = 0;
}



//RELAY EXTEND & EXTENDED

//function to extend the circuit by performing an ECDH key exchange with the next node 
void sendExtend(Circuit* circuit) {
    printf("sendExtend\n");
    if (circuit->hopCount >= MAX_HOPS) return;

    //generate fresh ECDH key pair, new one for each hop. private stored in circuit until EXTENDED is received
    uint8_t publicKey[KEY_SIZE];
    uint8_t privateKey[KEY_SIZE];
    ecdh_generate_keys(publicKey, privateKey); //generate fresh ECDH key pair, new one for each hop
    memcpy(circuit->pendingPrivateKey, privateKey, KEY_SIZE);

    RelayCell extendCell;
    extendCell.circID = circuit->circID;
    extendCell.command = CellCommand::RELAY;
    extendCell.relayCommand = RelayCellCommand::EXTEND;
    extendCell.srcNodeID = nodeID;
    extendCell.destNodeID = circuit->hopNodeIDs[0]; //send extend through first hop in circuit
    extendCell.payloadLength = 2 + KEY_SIZE;

    uint16_t nextHopID = circuit->hopNodeIDs[circuit->hopCount]; // where to extend to
    memcpy(extendCell.payload, &nextHopID, 2);
    memcpy(extendCell.payload + 2, publicKey, KEY_SIZE);

    //compute checksum of payload
    uint8_t digest[HASH_SIZE];
    hashData(extendCell.payload, digest);
    memcpy(extendCell.digest, digest, HASH_SIZE);

    printf("A public key: ");
    for (size_t i = 0; i < KEY_SIZE; i++) printf("%02x", publicKey[i]);
    printf("\n");

    uint8_t data[CELL_SIZE];
    extendCell.serialise(data, CELL_SIZE);

    //encrypt the payload with the session keys of the previous hops in the circuit
    encryptOnion(circuit->hopCount, circuit->hopSessionKeys, data + RELAY_HEADER_SIZE, CELL_SIZE - RELAY_HEADER_SIZE);
    radio_transmit_bytes_extern(data, CELL_SIZE);

    circuit->state = CircuitState::AWAITING_EXTENDED;
}

//function to handle a received RELAY EXTEND cell
void handleExtend(Circuit* circuit, RelayCell cell) {
    printf("handleExtend\n");
    //payload contains next node address (4 bytes) and client's public key (32 bytes)
    uint16_t nextNodeID;
    memcpy(&nextNodeID, cell.payload, 2);
    uint8_t* clientPubKey = cell.payload + 2;

    printf("payload for next hop: ");
    for (size_t i = 0; i < KEY_SIZE + 2; i++) printf("%02x", cell.payload[i]);
    printf("\n");


    // Generate a fresh circID for the next node
    uint16_t newCircID = (rand() % 65536) + 1; // random, non-zero

    // Store forwarding entry as: {incoming circID, incoming node} => {outgoing circID, outgoing node}
    createEntry(cell.circID, cell.srcNodeID, newCircID, nextNodeID);


    ControlCell createCell;
    createCell.circID = newCircID;
    createCell.command = CellCommand::CREATE;
    createCell.srcNodeID = nodeID;
    createCell.destNodeID = nextNodeID;
    memcpy(createCell.payload, clientPubKey, KEY_SIZE);

    uint8_t data[CELL_SIZE];
    createCell.serialise(data, CELL_SIZE);

    ControlCell check = ControlCell::deserialise(data, CELL_SIZE);

    radio_transmit_bytes_extern(data, CELL_SIZE); // to nextNodeID
}

//function to handle a received RELAY EXTENDED cell
void handleExtended(Circuit* circuit, RelayCell cell) {
    printf("handleExtended\n");

    uint8_t* theirPublicKey = cell.payload;

    //derive shared secret key
    uint8_t sessionKey[KEY_SIZE];
    ecdh_shared_secret(circuit->pendingPrivateKey, theirPublicKey, sessionKey);

    printf("session key: ");
    for (size_t i = 0; i < KEY_SIZE; i++) printf("%02x", sessionKey[i]);
    printf("\n");

    // clear private key, only sessionKey is now needed
    memset(circuit->pendingPrivateKey, 0, KEY_SIZE); 

    //store session key for actual hop
    memcpy(circuit->hopSessionKeys[circuit->hopCount].key, sessionKey, KEY_SIZE);
    circuit->hopCount++;

    // Now check if we need to extend further
    if (circuit->hopCount < circuit->totalHops) {
        sendExtend(circuit); // triggers the next hop
    } else {
        circuit->state = CircuitState::READY;
        printf("Circuit %d with %d hops is built.\n", circuit->circID, circuit->hopCount);
    }
}


//forward an EXTENDED cell back to the previous node in the circuit
void forwardExtended(Circuit* circuit, ControlCell cell) {
    printf("forwardExtended\n");

    ForwardingEntry* entry = getReverseForwardingEntry(cell.circID); //find forwarding entry to get prev circID and prev nodeID
    if (entry == nullptr) {
        return; // no entry, drop
     }

    RelayCell extendedCell;
    extendedCell.circID = entry->prevCircID; // forward with previous circID
    extendedCell.command = CellCommand::RELAY;
    extendedCell.relayCommand = RelayCellCommand::EXTENDED;
    extendedCell.srcNodeID = nodeID;
    extendedCell.destNodeID = entry->prevNodeID; // back towards the origin
    memcpy(extendedCell.payload, cell.payload, KEY_SIZE); // new node's public key

    // Set digest on plaintext before encrypting
    uint8_t digest[HASH_SIZE];
    hashData(extendedCell.payload, digest);
    memcpy(extendedCell.digest, digest, HASH_SIZE);

    uint8_t data[CELL_SIZE];
    extendedCell.serialise(data, CELL_SIZE);

    encryptOnion(circuit->hopCount, circuit->hopSessionKeys, data + RELAY_HEADER_SIZE, CELL_SIZE - RELAY_HEADER_SIZE);

    radio_transmit_bytes_extern(data, CELL_SIZE);
}



//RELAY DATA

//function to send a RELAY cell with the given payload, way to send true data over the circuit.
//if the data is larger than the maximum payload size, it will be split into multiple cells and sent sequentially.
void sendData(Circuit* circuit, uint8_t* data, size_t length) {
    printf("sendData\n");
    uint16_t maxPayload = CELL_SIZE - RELAY_HEADER_SIZE - HASH_SIZE;
    uint16_t offset = 0; //keep track of how much of the data has been sent
    while (offset < length) {
        printf("sendData loop, offset: %d, total: %zu\n", offset, length);

        RelayCell dataCell;
        dataCell.circID = circuit->circID;
        dataCell.command = CellCommand::RELAY;
        dataCell.relayCommand = RelayCellCommand::DATA;
        dataCell.srcNodeID = nodeID;
        dataCell.destNodeID = circuit->hopNodeIDs[0];

        uint16_t size = (length - offset) < maxPayload ? (length - offset) : maxPayload; //payload size
        dataCell.payloadLength = size;
        memcpy(dataCell.payload, data + offset, size);
        // write zeroes to remaining payload, prevent leakage of previous data if payload is smaller than max
        memset(dataCell.payload + size, 0, maxPayload - size);

        uint8_t hash[HASH_SIZE];
        hashData(dataCell.payload, hash); //hash the data for integrity checking
        memcpy(dataCell.digest, hash, HASH_SIZE);

        uint8_t buffer[CELL_SIZE];
        dataCell.serialise(buffer, CELL_SIZE);
        RelayCell check = RelayCell::deserialise(buffer, CELL_SIZE);
        encryptOnion(circuit->hopCount, circuit->hopSessionKeys, buffer + RELAY_HEADER_SIZE, CELL_SIZE - RELAY_HEADER_SIZE);
        radio_transmit_bytes_extern(buffer, CELL_SIZE);
        offset += size;
    }
}

uint8_t* handleData(Circuit* circuit, RelayCell cell, uint8_t* buffer) {
    printf("handleData\n");
    uint8_t digest[HASH_SIZE];
    memcpy(digest, cell.digest, HASH_SIZE);
    if (!verifyDigest(cell.payload, digest)) return nullptr; //invalid digest, drop message
    
    memcpy(buffer, cell.payload, cell.payloadLength);
    return buffer;
}


//MESSAGE DISPATCH

//function to listen for incoming LoRa messages and dispatch them based on command
void LoRaMessageListener() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        //printf("LOOP\n");
        uint8_t data[CELL_SIZE]; // buffer for received data
        uint16_t state = radio_receive_bytes_extern(data, CELL_SIZE);
        if (state == 0) {

            printf("dispatchControl\n");
            ControlCell controlCell = ControlCell::deserialise(data, CELL_SIZE);
            CellCommand command = controlCell.command;
            uint16_t circID = controlCell.circID;
            uint16_t destNodeID = controlCell.destNodeID;

            printf("Received cell destNodeID: %d, my nodeID: %d\n", destNodeID, nodeID);

            //a new node announces itself: update table and send aknowledgement
            if (command == CellCommand::ANNOUNCE) {
                handleAnnounce(controlCell, LocalPeers);
                continue;
            }

            //message not destined for own node
            if (destNodeID != nodeID) continue; 

            printf("Command: %d\n", command);

            //TODO: determine circuit ID and look up circuit to get session keys for decryption
            Circuit* circuit = getCircuit(circID);
            
            if (circuit == nullptr && command == CellCommand::CREATE) { //create a circuit
                int idx = findCircuitSlot();
                if (idx == -1) continue; // no space
                circuit = &CIRCUITS[idx];
                circuit->circID = circID;
                circuit->hopCount = 0;
                circuit->state = CircuitState::IDLE;
            } else if (circuit == nullptr && command == CellCommand::CREATED) { //see below
            } else if (circuit == nullptr) {
                continue; // unknown circuit, drop
            }

            ForwardingEntry* entry;
            switch (command) {
            case CREATE:
                handleCreate(circuit, controlCell);
                break;
            case CREATED:
                entry = getReverseForwardingEntry(circID);
                if(entry != nullptr) {
                    //Node is relay: forward information
                    circuit = getCircuit(entry->prevCircID);
                    if (circuit == nullptr) break; // no circuit, drop
                    forwardExtended(circuit, controlCell); //forward CREATED cell back to previous node in circuit
                } else {
                    //Node is origin: handle CREATED
                    if (circuit == nullptr) break; // no circuit, drop
                    handleCreated(circuit, controlCell);
                }
                break;
            case DESTROY:
                //handleDestroy(circuit);
                break;
            case RELAY:
                dispatchRelay(circuit, data); //payload of control cell contains the encrypted relay cell
                break;   
            default:
                break;
            }
        } else {
            //printf("No Data Received\n");
        }
    }
}

void dispatchRelay(Circuit* circuit, uint8_t* data) {
    printf("dispatchRelay\n");

    HopSecretKey circuitKey = circuit->hopSessionKeys[0]; //fix: not always first!!

     // Decrypt the raw buffer first, same region that was encrypted
    decryptSingleHop(circuitKey.key, circuitKey.nonce, data + RELAY_HEADER_SIZE, CELL_SIZE - RELAY_HEADER_SIZE);

    // Now deserialise the decrypted buffer
    RelayCell cell = RelayCell::deserialise(data, CELL_SIZE);

    // Check if this cell is meant for us
    uint8_t hash[HASH_SIZE];
    hashData(cell.payload, hash);
    printf("digest match: %d\n", memcmp(hash, cell.digest, HASH_SIZE) == 0);
    printf("relayCommand: %d payloadLength: %d\n", cell.relayCommand, cell.payloadLength);

    if (!(memcmp(hash, cell.digest, HASH_SIZE) == 0)) {
        ForwardingEntry* entry = getForwardingEntry(cell.circID);

        if (entry != nullptr && cell.srcNodeID == entry->prevNodeID) {
            if (entry == nullptr) return;
            // rewrite circID and dest towards next hop in circuit
            data[0] = (entry->nextCircID >> 8) & 0xFF;
            data[1] =  entry->nextCircID & 0xFF;
            data[3] = (entry->nextNodeID >> 8) & 0xFF;
            data[4] =  entry->nextNodeID & 0xFF;
            data[5] = (nodeID >> 8) & 0xFF;  // rewrite srcNodeID to self
            data[6] =  nodeID & 0xFF;
        } else {
            // reverse lookup toward prev hop
            entry = getReverseForwardingEntry(cell.circID);
            if (entry == nullptr) {
                return; // no entry, drop
            }
            data[0] = (entry->prevCircID >> 8) & 0xFF;
            data[1] =  entry->prevCircID & 0xFF;
            data[3] = (entry->prevNodeID >> 8) & 0xFF;
            data[4] =  entry->prevNodeID & 0xFF;
            data[5] = (nodeID >> 8) & 0xFF;  // rewrite srcNodeID to self
            data[6] =  nodeID & 0xFF;
        }
        radio_transmit_bytes_extern(data, CELL_SIZE);
        return;
    }

    uint8_t* result = nullptr;
    printf("Relay command: %d\n", cell.relayCommand);
    switch (cell.relayCommand) {
        case EXTEND:
            handleExtend(circuit, cell);
            break;
        case EXTENDED:
            handleExtended(circuit, cell);
            break;
        case DATA:
            uint8_t buffer[CELL_SIZE - RELAY_HEADER_SIZE - HASH_SIZE];
            result = handleData(circuit, cell, buffer);
            if (result != nullptr) {
                printf("Received data: %s\n", result);
            }
            break;
        default:
            break;
    }
}

//function to look up a circuit by its ID, returns null if not found
Circuit* getCircuit(uint16_t circID) {
    for (int i = 0; i < CIRCUIT_AMOUNT; i++) {
        if (CIRCUITS[i].circID == circID) {
            return &CIRCUITS[i];
        }
    }
    return nullptr;
}

int findCircuitSlot() {
    for (size_t i = 0; i < CIRCUIT_AMOUNT; i++) {
        if (CIRCUITS[i].circID == 0) return i; //empty slot
    }
    return -1; //no empty slot
}

//WIP: builds a circuit from caller node to destination node.
//Should work for both nodes in the local group as well as nodes in other groups (through a router).
Circuit* buildCircuit(uint16_t destNodeID, uint8_t totalHops, LoraHashTable table, NodeRole role) {
    if (totalHops < 1 || totalHops > MAX_HOPS) {
        printf("Error: invalid amount of hops: %d\n", totalHops);
        return nullptr;
    }

    int idx = findCircuitSlot(); //find empty slot for new circuit
    if (idx == -1) return nullptr; //no space for new circuit

    Circuit* circuit = &CIRCUITS[idx];
    circuit->circID = idx + 1; //FIX: UNIQUE ID
    circuit->hopCount = 0;
    circuit->totalHops = totalHops;
    circuit->state = CircuitState::IDLE;
    //circuit->hopNodeIDs[totalHops - 1] = destNodeID; //put destination node as last

    //TODO: calculate a route to destination node
    //table.determineRoute(destNodeID, role)

    //hardcoded path
    circuit->hopNodeIDs[0] = 222;
    circuit->hopNodeIDs[1] = 333;
    circuit->hopNodeIDs[2] = 444;

    //send CREATE will snowball into creating the whole circuit
    sendCreate(circuit);
    return circuit;
}




void createEntry(uint16_t prevCircID, uint16_t prevNodeID, uint16_t nextCircID, uint16_t nextNodeID) {
    for (size_t i = 0; i < CIRCUIT_AMOUNT * MAX_HOPS; i++) {
        if (FORWARDING_TABLE[i].prevCircID == 0) { //empty slot
            FORWARDING_TABLE[i].prevCircID = prevCircID;
            FORWARDING_TABLE[i].prevNodeID = prevNodeID;
            FORWARDING_TABLE[i].nextCircID = nextCircID;
            FORWARDING_TABLE[i].nextNodeID = nextNodeID;
            return;
        }
    }
}

//lookup function to find the forwarding entry based on the prev circID
ForwardingEntry* getForwardingEntry(uint16_t prevCircID) {
    for (size_t i = 0; i < CIRCUIT_AMOUNT * MAX_HOPS; i++) {
        if (FORWARDING_TABLE[i].prevCircID == prevCircID) {
            return &FORWARDING_TABLE[i];
        }
    }
    return nullptr; //not found
}

//reverse lookup function to find the forwarding entry based on the next circID
ForwardingEntry* getReverseForwardingEntry(uint16_t nextCircID) {
    for (size_t i = 0; i < CIRCUIT_AMOUNT * MAX_HOPS; i++) {
        if (FORWARDING_TABLE[i].nextCircID == nextCircID) {
            return &FORWARDING_TABLE[i];
        }
    }
    return nullptr; //not found
}