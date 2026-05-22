#include "TorCell.h"

//ControlCell methods

//serialise the control cell into a byte buffer for transmission
void ControlCell::serialise(uint8_t* buffer, size_t length) { //command byte is either control or relay command
    if (length != CELL_SIZE) {
        printf("Error: Buffer length must be exactly %d bytes\n", CELL_SIZE);
        return;
    }
    //printf("Serialising ControlCell\n");

    buffer[0] = (circID >> 8) & 0xFF; //high byte
    buffer[1] = circID & 0xFF; //low byte

    buffer[2] = command;

    buffer[3] = (destNodeID >> 8) & 0xFF;
    buffer[4] =  destNodeID & 0xFF;

    buffer[5] = (srcNodeID >> 8) & 0xFF;
    buffer[6] =  srcNodeID & 0xFF;


    //copy digest and payload into buffer
    memcpy(buffer + CONTROL_HEADER_SIZE, digest, HASH_SIZE);
    memcpy(buffer + CONTROL_HEADER_SIZE + HASH_SIZE, payload, CELL_SIZE - CONTROL_HEADER_SIZE - HASH_SIZE);

}

//deserialise a byte buffer into the control cell's fields
ControlCell ControlCell::deserialise(uint8_t* buffer, size_t length) {
    if (length != CELL_SIZE) {
        printf("Error: Buffer length must be exactly %d bytes\n", CELL_SIZE);
        return ControlCell(); //return empty cell on error
    }
    //printf("Deserialising ControlCell\n");
    ControlCell cell;
    cell.circID     = (buffer[0] << 8) | buffer[1]; //combine high and low byte for circuit ID
    cell.command    = (CellCommand)buffer[2]; //cast command byte to enum
    cell.destNodeID = (buffer[3] << 8) | buffer[4];
    cell.srcNodeID  = (buffer[5] << 8) | buffer[6];


    memcpy(cell.digest, buffer + CONTROL_HEADER_SIZE, HASH_SIZE);
    memcpy(cell.payload, buffer + CONTROL_HEADER_SIZE + HASH_SIZE, CELL_SIZE - CONTROL_HEADER_SIZE - HASH_SIZE);

    return cell;
}

//RelayCell methods

//serialise the relay cell into a byte buffer for transmission
void RelayCell::serialise(uint8_t* buffer, size_t length) {
    if (length != CELL_SIZE) {
        printf("Error: Buffer length must be exactly %d bytes\n", CELL_SIZE);
        return;
    }
    //printf("Serialising RelayCell\n");

    buffer[0] = (circID >> 8) & 0xFF; //high byte
    buffer[1] = (circID & 0xFF); //low byte

    buffer[2] = RELAY;

    buffer[3] = (destNodeID >> 8) & 0xFF;
    buffer[4] =  destNodeID & 0xFF;

    buffer[5] = (srcNodeID >> 8) & 0xFF;
    buffer[6] =  srcNodeID & 0xFF;

    buffer[7] = (payloadLength >> 8) & 0xFF;
    buffer[8] = (payloadLength & 0xFF);

    buffer[9] = relayCommand;

    //copy digest and payload into buffer
    memcpy(buffer + RELAY_HEADER_SIZE, digest, HASH_SIZE);
    memcpy(buffer + RELAY_HEADER_SIZE + HASH_SIZE, payload, CELL_SIZE - RELAY_HEADER_SIZE - HASH_SIZE);

}
    
//deserialise a byte buffer into the relay cell's fields
RelayCell RelayCell::deserialise(uint8_t* buffer, size_t length) {
    if (length != CELL_SIZE) {
        printf("Error: Buffer length must be exactly %d bytes\n", CELL_SIZE);
        return RelayCell(); //return empty cell on error
    }
    //printf("Deserialising RelayCell\n");
    RelayCell cell;
    cell.circID         = (buffer[0] << 8)  | buffer[1];
    cell.command        = (CellCommand)buffer[2];
    cell.destNodeID     = (buffer[3] << 8)  | buffer[4];
    cell.srcNodeID      = (buffer[5] << 8)  | buffer[6];
    cell.payloadLength  = (buffer[7] << 8) | buffer[8];
    cell.relayCommand   = (RelayCellCommand)buffer[9];

    memcpy(cell.digest, buffer + RELAY_HEADER_SIZE, HASH_SIZE);
    memcpy(cell.payload, buffer + RELAY_HEADER_SIZE + HASH_SIZE, CELL_SIZE - RELAY_HEADER_SIZE - HASH_SIZE);

    return cell;
}