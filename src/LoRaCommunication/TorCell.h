#include <stdlib.h>
#include <string.h>
#include <cstdio>

#define CELL_SIZE 512
#define CONTROL_HEADER_SIZE 7
#define RELAY_HEADER_SIZE 10

#ifndef HASH_SIZE
#define HASH_SIZE 32
#endif


enum CellCommand {
    PADDING,
    CREATE,
    CREATED,
    DESTROY,
    RELAY,
    ANNOUNCE
};

enum RelayCellCommand {
    DATA,
    BEGIN,
    END,
    TEARDOWN,
    CONNECTED,
    EXTEND,
    EXTENDED,
    TRUNCATE,
    TRUNCATED,
    SENDME,
    DROP
};

class Cell {
public:
    uint16_t circID;
    CellCommand command;
    uint16_t srcNodeID;
    uint16_t destNodeID;
    uint8_t digest[HASH_SIZE];
    virtual void serialise(uint8_t* buffer, size_t length) = 0;
};

class ControlCell: public Cell {
public:
    uint8_t payload[CELL_SIZE - CONTROL_HEADER_SIZE - HASH_SIZE];
    uint32_t payloadLength;
    void serialise(uint8_t* buffer, size_t length) override;
    static ControlCell deserialise(uint8_t* buffer, size_t length);
};

class RelayCell: public Cell {
public:
    RelayCellCommand relayCommand;
    uint32_t payloadLength;
    uint8_t payload[CELL_SIZE - RELAY_HEADER_SIZE - HASH_SIZE];
    void serialise(uint8_t* buffer, size_t length) override;
    static RelayCell deserialise(uint8_t* buffer, size_t length);
};