#pragma once
#include <unistd.h>

#include <cstdint>

#include "../Utils/sockets.h"
#include "./interrupts.h"

#define INTERRUPT_RESPONSE_TYPE_SUCCESS 01
#define INTERRUPT_RESPONSE_TYPE_ERROR 02
#define INTERRUPT_RESPONSE_TYPE_SUBSCRIPTION 03

bool Interrupt_send_JSON_message(const Channel &output,
                                 InterruptTypes interrupt_nr,
                                 const uint8_t response_type, uint32_t id,
                                 char *subContent, uint8_t error_code);

bool Interrupt_send_JSON_start_message(const Channel &output,
                                       const InterruptTypes interrupt_nr,
                                       const uint8_t response_type,
                                       const uint32_t id, bool hasSubContent,
                                       const uint8_t error_code);

bool Interrupt_send_JSON_end_message(const Channel &output,
                                     bool newline = true);

void Interrupt_send_JSON_success_message(const Channel &output,
                                         InterruptTypes interrupt_nr,
                                         uint32_t id,
                                         char *subContent = nullptr);

void Interrupt_send_JSON_failure_message(const Channel &output,
                                         InterruptTypes interrupt_nr,
                                         uint32_t id, const uint8_t error_code,
                                         char *subContent = nullptr);

ssize_t Interrupt_serialize_hexa_string_response(
    const InterruptTypes interrupt_nr, const uint32_t id, uint8_t response_type,
    char *dest);
