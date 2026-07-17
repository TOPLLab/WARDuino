#include "debug_msg.h"

#include <cstddef>
#include <cstdint>

#include "../Utils/util.h"
#include "../WARDuino/vm_exception.h"

void DebugMessage_free(DebugMessage *msg) {
    if (msg->fullData != nullptr) free(msg->fullData);
    msg->fullData = nullptr;
    msg->data = nullptr;
    delete msg;
}

bool readMsgID(uint8_t **data, size_t size, uint32_t &id) {
    uint32_t min_bytes = 1;
    if (size < min_bytes) {
        id = 0;
        return false;
    }
    id = read_LEB_32(data);
    return true;
}

DebugMessage *DebugMessage_new(uint8_t *fullData, size_t size) {
    DebugMessage *dm = new DebugMessage;
    dm->fullData = fullData;
    dm->data = fullData + 1;  // +1 skip the interrupt byte
    bool success = readMsgID(&(dm->data), size - 1,
                             dm->id);  // -1 to substract interrupt byte
    if (!success) {
        VM_Exception_write("debug message has not enough data for ID\n");
        return nullptr;
    }
    dm->interrupt = (InterruptTypes)*fullData;
    dm->len = size - (dm->data - dm->fullData);
    return dm;
}
