#include "snapshot_state_encoder.h"

#include <netinet/in.h>

#include "../../src/Utils/util.h"

uint8_t* getCodePointer(Block* block, Module* m) {
    auto find =
        std::find_if(std::begin(m->block_lookup), std::end(m->block_lookup),
                     [&](const std::pair<uint8_t*, Block*>& pair) {
                         return pair.second == block;
                     });
    uint8_t* opcode = nullptr;
    if (find != std::end(m->block_lookup)) {
        opcode = find->first;
    }
    return opcode;
}

SnapshotBinaryEncoder::SnapshotBinaryEncoder(uint8_t t_interruptNumber,
                                             Module* t_module)
    : interruptNr(t_interruptNumber), m(t_module) {}

void SnapshotBinaryEncoder::encodePC(uint32_t pc) {
    this->stateToTransmit.push_back(pcState);
    encodeB32(pc);
}

void SnapshotBinaryEncoder::createStateMessage(std::string* dest,
                                               bool isLastMessage) {
    // marking the end or not of the state transmission
    this->stateToTransmit.push_back(isLastMessage ? 1 : 0);

    // copy bytes to buffer
    // 1: one byte to store the interruptkind
    // 4: 4 bytes to store the payload size
    // encoding.size(): payload size
    // + 1: to inform whether this message is the last
    uint32_t sizePayload = this->stateToTransmit.size();
    uint32_t sizeMessage = 1 + 4 + sizePayload;
    uint8_t* encoding = (uint8_t*)malloc(sizeMessage);
    uint32_t offset = 0;
    encoding[offset++] = this->interruptNr;
    // tmp -1 will be fixed as part of the refactoring of snapshot
    encodeB32(sizePayload - 1, encoding + offset);
    offset += 4;
    for (auto i = 0; i < sizePayload; ++i) {
        encoding[i + offset] = stateToTransmit[i];
    }

    // *2 for conversion to hexa
    uint32_t sizeMsgInHexa = sizeMessage * 2;
    char* hexa = (char*)malloc(sizeMsgInHexa + 1);  // + 1 for newline
    chars_as_hexa((unsigned char*)hexa, encoding,
                  sizeMessage);  // exclude newline

    free(encoding);

    // add newline
    hexa[sizeMsgInHexa] = '\n';
    dest->assign(hexa);
}

void SnapshotBinaryEncoder::createFirstMessage(
    std::string* dest, uint32_t amount_globals, uint32_t table_initial,
    uint32_t table_max, uint32_t table_size, uint32_t mem_max,
    uint32_t mem_initial, uint32_t mem_size) {
    // 4: to tell size payload
    // 1*3: one byte to tell the kind of state send
    // 7*4: bytes per argument encoded as B32
    uint32_t sizePayload = 3 + 7 * 4;
    // 1: interrupt kind
    // 4: used to store the size of payload
    uint32_t sizeMessage = 1 + 4 + sizePayload;
    uint8_t message[sizeMessage];

    uint32_t offset = 0;
    // interrupt
    message[offset++] = interruptLoadSnapshot;

    // size payload
    uint8_t sizePayloadB32[4] = {0};
    encodeB32(sizePayload, sizePayloadB32);
    memcpy(message + offset, sizePayloadB32, 4);
    offset += 4;

    // amount globals
    message[offset++] = globalsState;

    uint8_t amountGlobalsB32[4] = {0};
    encodeB32(amount_globals, amountGlobalsB32);
    memcpy(message + offset, amountGlobalsB32, 4);
    offset += 4;

    // table
    message[offset++] = tableState;

    uint8_t tableInitB32[4] = {0};
    encodeB32(table_initial, tableInitB32);
    memcpy(message + offset, tableInitB32, 4);
    offset += 4;

    uint8_t tableMaxB32[4] = {0};
    encodeB32(table_max, tableMaxB32);
    memcpy(message + offset, tableMaxB32, 4);
    offset += 4;

    uint8_t tableSizeB32[4] = {0};
    encodeB32(table_size, tableSizeB32);
    memcpy(message + offset, tableSizeB32, 4);
    offset += 4;

    // memory
    message[offset++] = memoryState;

    uint8_t memMaxB32[4] = {0};
    encodeB32(mem_max, memMaxB32);
    memcpy(message + offset, memMaxB32, 4);
    offset += 4;

    uint8_t memInitB32[4] = {0};
    encodeB32(mem_initial, memInitB32);
    memcpy(message + offset, memInitB32, 4);
    offset += 4;

    uint8_t memSizeB32[4] = {0};
    encodeB32(mem_size, memSizeB32);
    memcpy(message + offset, memSizeB32, 4);

    // transform to hexastring
    uint32_t sizeMessageInHexa = sizeMessage * 2;
    char* hexa =
        (char*)malloc(sizeMessageInHexa + 1);  // + 1 for newline termination

    chars_as_hexa((unsigned char*)hexa, message, sizeMessage);
    hexa[sizeMessageInHexa] = '\n';
    dest->assign(hexa);
}

void SnapshotBinaryEncoder::encodeBreakpoints(std::set<uint32_t> breakpoints) {
    this->stateToTransmit.push_back(breakpointsState);
    this->stateToTransmit.push_back((uint8_t)breakpoints.size());
    for (auto bp : breakpoints) {
        encodeB32(bp);
    }
}

void SnapshotBinaryEncoder::encodeCallstack(std::vector<Frame>* frames) {
    this->stateToTransmit.push_back(callstackState);
    uint16_t s = frames->size();
    encodeB16(s);
    for (auto frame : *frames) {
        this->stateToTransmit.push_back(frame.block->block_type);
        this->encodeSignedB32(frame.sp);
        this->encodeSignedB32(frame.fp);
        int ra =
            frame.ra_ptr == nullptr ? -1 : toVirtualAddress(frame.ra_ptr, m);
        this->encodeSignedB32(ra);
        if (frame.block->block_type == 0) {  // func
            this->encodeB32(frame.block->fidx);
        } else if (frame.block->block_type != 0xff &&
                   frame.block->block_type != 0xfe) {
            uint32_t blockKey =
                toVirtualAddress(getCodePointer(frame.block, m), m);
            this->encodeB32(blockKey);
        }
    }
}

void SnapshotBinaryEncoder::encodeB32(uint32_t value, uint8_t* buffer) {
    uint8_t* buff = buffer == nullptr ? (uint8_t*)malloc(4) : buffer;

    buff[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    buff[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buff[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buff[3] = static_cast<uint8_t>(value & 0xFF);

    if (buffer == nullptr) {
        for (int i = 0; i < 4; ++i) {
            this->stateToTransmit.push_back(buff[i]);
        }
        free(buff);
    }
}

void SnapshotBinaryEncoder::encodeB16(uint16_t value, uint8_t* buffer) {
    uint8_t* buff = buffer == nullptr ? (uint8_t*)malloc(2) : buffer;

    buff[0] = (value >> 8) & 0xFF;
    buff[1] = value & 0xFF;
    if (buffer == nullptr) {
        for (int i = 0; i < 2; ++i) {
            this->stateToTransmit.push_back(buff[i]);
        }
        free(buff);
    }
}
void SnapshotBinaryEncoder::encodeSignedB32(int32_t value, uint8_t* buffer) {
    // Convert the integer to big-endian byte order
    uint8_t* buff =
        buffer == nullptr ? (uint8_t*)malloc(sizeof(int32_t)) : buffer;
    value = htonl(value);

    // Copy the bytes of the integer to the buffer
    std::memcpy(buff, &value, sizeof(int));
    if (buffer == nullptr) {
        for (int i = 0; i < sizeof(int32_t); ++i) {
            this->stateToTransmit.push_back(buff[i]);
        }
        free(buff);
    }
}