#pragma once
#include <cstdint>
#include <iostream>
#include <set>
#include <vector>

#include "../../../src/WARDuino.h"

class SnapshotBinaryEncoder {
   private:
    const uint8_t interruptNr{};
    Module* m{};

    std::vector<uint8_t> stateToTransmit{};

    void encodeB32(uint32_t value, uint8_t* buffer = nullptr);

    void encodeB16(uint16_t value, uint8_t* buffer = nullptr);

    void encodeSignedB32(int32_t value, uint8_t* buffer = nullptr);

   public:
    SnapshotBinaryEncoder(uint8_t t_interruptNumber, Module* t_module);

    void createFirstMessage(std::string* dest, uint32_t amount_globals,
                            uint32_t table_initial, uint32_t table_max,
                            uint32_t table_size, uint32_t mem_max,
                            uint32_t mem_initial, uint32_t mem_size);

    void createStateMessage(std::string* dest, bool isLastMessage = true);

    void encodePC(uint32_t pc);

    void encodeBreakpoints(std::set<uint32_t> breakpoints);

    void encodeCallstack(std::vector<Frame>* frames);
};