#include "snapshot_state_encoder.h"

#include <netinet/in.h>

#include "../../src/Interrupts/interrupt_inspect.h"
#include "../../src/Interrupts/interrupts.h"
#include "../../src/Utils/util.h"
#include "serialisation.h"

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

void SnapshotBinaryEncoder::createStateMessage(std::string& dest, uint8_t idAck,
                                               bool isLastMessage) {
    // marking the end or not of the state transmission
    this->stateToTransmit.push_back(isLastMessage ? 1 : 0);

    dest.clear();

    // copy bytes to buffer
    // 4: 4 bytes to store the payload size
    // encoding.size(): payload size
    // + 1: to inform whether this message is the last
    uint32_t sizePayload = this->stateToTransmit.size();
    uint32_t sizeMessage = 4 + sizePayload;
    uint8_t* encoding = (uint8_t*)malloc(sizeMessage);
    uint32_t offset = 0;
    //    encoding[offset++] = this->interruptNr;
    // tmp -1 will be fixed as part of the refactoring of snapshot
    encodeB32(sizePayload - 1, encoding + offset);
    offset += 4;
    for (auto i = 0; i < sizePayload; ++i) {
        encoding[i + offset] = stateToTransmit[i];
    }

    // *2 for conversion to hexa
    uint32_t sizeMsgInHexa = sizeMessage * 2;
    char* hexa = (char*)malloc(sizeMsgInHexa +
                               2);  // + 2 for newline and string termination
    chars_as_hexa((unsigned char*)hexa, encoding,
                  sizeMessage);  // exclude newline

    free(encoding);

    // add newline
    hexa[sizeMsgInHexa] = '\n';
    // string termination
    hexa[sizeMsgInHexa + 1] = '\0';

    std::string interruptHex{};
    Serialiser::uint8ToHexString(this->interruptNr, interruptHex);

    std::string idHex{};
    Serialiser::uint8ToHexString(idAck, idHex);

    dest = interruptHex + idHex + hexa;
}

void SnapshotBinaryEncoder::createFirstMessage(
    std::string& dest, uint8_t idAck, uint32_t amount_globals,
    uint32_t table_initial, uint32_t table_max, uint32_t table_size,
    uint32_t mem_max, uint32_t mem_initial, uint32_t mem_size) {
    dest.clear();

    std::string interruptHex{};
    Serialiser::uint8ToHexString(interruptLoadSnapshot, interruptHex);
    dest = interruptHex;

    // id ack
    std::string idHex{};
    Serialiser::uint8ToHexString(idAck, idHex);
    dest += idHex;

    // size payload
    // 4: to tell size payload
    // 1*3: one byte to tell the kind of state send
    // 7*4: bytes per argument encoded as B32
    uint32_t sizePayload = 3 + 7 * 4;
    uint8_t sizePayloadB32[4] = {0};
    this->encodeB32(sizePayload, sizePayloadB32);
    std::string sizePayloadHex{};
    Serialiser::uint8BufferToHex(sizePayloadB32, 4, sizePayloadHex);
    dest += sizePayloadHex;

    // amount globals
    std::string globalHex{};
    Serialiser::uint8ToHexString(globalsState, globalHex);
    dest += globalHex;

    uint8_t amountGlobalsB32[4] = {0};
    this->encodeB32(amount_globals, amountGlobalsB32);
    //    memcpy(message + offset, amountGlobalsB32, 4);
    //    offset += 4;
    std::string globalsAmountHex{};
    Serialiser::uint8BufferToHex(amountGlobalsB32, 4, globalsAmountHex);
    dest += globalsAmountHex;

    // table
    std::string tableHex{};
    Serialiser::uint8ToHexString(tableState, tableHex);
    dest += tableHex;

    uint8_t tableInitB32[4] = {0};
    this->encodeB32(table_initial, tableInitB32);
    std::string tableInitHex{};
    Serialiser::uint8BufferToHex(tableInitB32, 4, tableInitHex);
    dest += tableInitHex;

    uint8_t tableMaxB32[4] = {0};
    this->encodeB32(table_max, tableMaxB32);
    std::string tableMaxHex{};
    Serialiser::uint8BufferToHex(tableMaxB32, 4, tableMaxHex);
    dest += tableMaxHex;

    uint8_t tableSizeB32[4] = {0};
    this->encodeB32(table_size, tableSizeB32);
    std::string tableSizeHex{};
    Serialiser::uint8BufferToHex(tableSizeB32, 4, tableSizeHex);
    dest += tableSizeHex;

    // memory
    std::string memHex{};
    Serialiser::uint8ToHexString(memoryState, memHex);
    dest += memHex;

    uint8_t memMaxB32[4] = {0};
    this->encodeB32(mem_max, memMaxB32);
    std::string memMaxHex{};
    Serialiser::uint8BufferToHex(memMaxB32, 4, memMaxHex);
    dest += memMaxHex;

    uint8_t memInitB32[4] = {0};
    this->encodeB32(mem_initial, memInitB32);
    std::string memInitHex{};
    Serialiser::uint8BufferToHex(memInitB32, 4, memInitHex);
    dest += memInitHex;

    uint8_t memSizeB32[4] = {0};
    this->encodeB32(mem_size, memSizeB32);
    std::string memSizeHex{};
    Serialiser::uint8BufferToHex(memSizeB32, 4, memSizeHex);
    dest += memSizeHex;
    dest += "\n";
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

void SnapshotBinaryEncoder::encodeCallbacks(std::vector<Callback>& callbacks) {
    this->stateToTransmit.push_back(callbacksState);
    // encode number of topics
    std::set<std::string> topics{};
    for (auto callback : callbacks) {
        topics.insert(callback.topic);
    }
    encodeB32(topics.size());

    for (auto topic : topics) {
        // encode the topic
        encodeString(topic);

        // search all indexes for the same topic
        std::set<uint32_t> indexes{};
        for (auto callback : callbacks) {
            if (callback.topic != topic) {
                continue;
            }
            indexes.insert(callback.table_index);
        }

        // encode amount table indexes
        encodeB32(indexes.size());
        // encode each table index
        for (auto tidx : indexes) {
            encodeB32(tidx);
        }
    }
}
void SnapshotBinaryEncoder::encodeEvents(std::vector<Event>& events) {
    this->stateToTransmit.push_back(eventsState);
    this->encodeB32(events.size());
    for (auto ev : events) {
        this->encodeString(ev.topic);
        this->encodeString(ev.payload);
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

void SnapshotBinaryEncoder::encodeString(std::string s) {
    encodeB32(s.size());

    // encode string content
    uint8_t* buff = (uint8_t*)malloc(s.size());
    std::memcpy(buff, s.c_str(), s.size());
    for (int i = 0; i < s.size(); ++i) {
        this->stateToTransmit.push_back(buff[i]);
    }
    free(buff);
}
