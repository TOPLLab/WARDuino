#include "interrupt_response.h"

#include <stdio.h>

#include <cstdint>
#include <cstdio>

#include "../Utils/util.h"

void Interrupt_send_JSON_subscribe_start_message(const Channel &output,
                                                 InterruptTypes interrupt_nr,
                                                 uint32_t id) {
    output.write(R"({"interrupt":"%02X","id":"%02X","kind":"%02X","sub":)",
                 interrupt_nr, id, INTERRUPT_RESPONSE_TYPE_SUBSCRIPTION);
}

void Interrupt_send_JSON_subscribe_end_message(const Channel &output) {
    output.write("}\n");
}

bool Interrupt_send_JSON_message(const Channel &output,
                                 InterruptTypes interrupt_nr,
                                 const uint8_t response_type, uint32_t id,
                                 char *subContent, uint8_t error_code) {
    if (!Interrupt_send_JSON_start_message(output, interrupt_nr, response_type,
                                           id, subContent != nullptr,
                                           error_code))
        return false;

    if (subContent != nullptr) output.write(subContent);

    return Interrupt_send_JSON_end_message(output);
}

bool Interrupt_send_JSON_start_message(const Channel &output,
                                       InterruptTypes interrupt_nr,
                                       const uint8_t response_type, uint32_t id,
                                       bool subContent, uint8_t error_code) {
    output.write(R"({"interrupt":"%02X","kind":"%02X","id":"%02X")",
                 interrupt_nr, response_type, id);
    switch (response_type) {
        case INTERRUPT_RESPONSE_TYPE_ERROR:
            output.write(R"(,"error_code":"%02X")", error_code);
        case INTERRUPT_RESPONSE_TYPE_SUCCESS:
        case INTERRUPT_RESPONSE_TYPE_SUBSCRIPTION:
            break;
        default:
            printf(
                "Interrupt_serialize_json_response: response type is "
                "unknown %02X\n",
                response_type);
            return false;
    }

    if (subContent) output.write(R"(,"sub":)");
    return true;
}

bool Interrupt_send_JSON_end_message(const Channel &output, bool newline) {
    return output.write("}%s", newline ? "\n" : "") > 0;
}

void Interrupt_send_JSON_success_message(const Channel &output,
                                         InterruptTypes interrupt_nr,
                                         uint32_t id, char *subContent) {
    Interrupt_send_JSON_message(output, interrupt_nr,
                                INTERRUPT_RESPONSE_TYPE_SUCCESS, id, subContent,
                                NO_ERROR);
}

void Interrupt_send_JSON_failure_message(const Channel &output,
                                         InterruptTypes interrupt_nr,
                                         uint32_t id, const uint8_t error_code,
                                         char *subContent) {
    Interrupt_send_JSON_message(output, interrupt_nr,
                                INTERRUPT_RESPONSE_TYPE_ERROR, id, subContent,
                                error_code);
}

ssize_t Interrupt_serialize_hexa_string_response(
    const InterruptTypes interrupt_nr, const uint32_t id,
    const uint8_t response_type, char *dest) {
    // format: interrupt_nr (1byte) | message_type (1byte) | id (LEB32)
    uint8_t buffer[100] = {};
    buffer[0] = interrupt_nr;
    buffer[1] = response_type;

    ssize_t written = write_32BIT_LEB(id, buffer + 2);
    HexUInt8Encoding result{};
    result.encoding = dest;
    uint8_to_hex(buffer, 2 + written, &result);
    return result.bytesWritten;
}
