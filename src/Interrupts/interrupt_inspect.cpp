#include "interrupt_inspect.h"

#include <cstdint>

#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "../WARDuino/vm_exception.h"
#include "interrupt_response.h"

void printValue(const Channel &output, StackValue *v, uint32_t idx,
                bool end = false);

uint8_t *findOpcode(Module *m, Block *block);

void Interrupt_Inspect_handle_request(const Channel &requester, Module *m,
                                      DebugMessage *msg) {
    InspectStateRequest request{};
    uint8_t error_code{};
    if (!Interrupt_Inspect_deserialize_request(request, msg->data,
                                               error_code)) {
        Interrupt_send_JSON_failure_message(requester, msg->interrupt, msg->id,
                                            error_code);
        return;
    }

    bool includeSubContent = true;
    Interrupt_send_JSON_start_message(requester, msg->interrupt,
                                      INTERRUPT_RESPONSE_TYPE_SUCCESS, msg->id,
                                      includeSubContent, NO_ERROR);
    bool includeNewLine = false, includeHeader = false;
    Interrupt_Inspect_inspect_json_output(requester, m, request, includeHeader,
                                          includeNewLine);
    Interrupt_send_JSON_end_message(requester);
}

bool Interrupt_Inspect_deserialize_request(InspectStateRequest &request,
                                           uint8_t *data, uint8_t &error_code) {
    // format: nr of inspects | state x | state y | ...

    request.numberOfInspects = read_B16(&data);
    if (request.numberOfInspects == 0) {
        return false;
    }

    request.requestedState = (ExecutionState *)malloc(
        request.numberOfInspects * sizeof(enum ExecutionState));
    for (auto i = 0; i < request.numberOfInspects; i++) {
        switch (data[i]) {
            case pcState:
            case breakpointsState:
            case callstackState:
            case globalsState:
            case tableState:
            case memoryState:
            case branchingTableState:
            case stackState:
            case callbacksState:
            case eventsState:
            case errorState:
            case logicalClock:
            case heapState:
                request.requestedState[i] = (enum ExecutionState)data[i];
                break;
            default:
                error_code = INSPECT_ERROR_CODE_REQUEST_HAS_INVALID_STATE_KIND;
                return false;
        }
    }
    return true;
}

bool Interrupt_Inspect_inspect_json_output(const Channel &requester,
                                           const Module *m,
                                           const StateToInspect &state,
                                           bool includeHeader,
                                           bool includeNewline) {
    debug("asked for inspect\n");
    uint16_t idx = 0;
    auto toVA = [m](uint8_t *addr) {
        return toVirtualAddress(addr, (Module *)m);
    };
    bool addComma = false;

    if (includeHeader) {
        requester.write("DUMP!\n");
    }
    requester.write("{");
    while (idx < state.numberOfInspects) {
        switch (state.requestedState[idx++]) {
            case pcState: {  // PC
                requester.write("\"pc\":%" PRIu32 "", toVA(m->pc_ptr));
                addComma = true;

                break;
            }
            case breakpointsState: {
                requester.write("%s\"breakpoints\":[", addComma ? "," : "");
                addComma = true;
                size_t i = 0;
                for (auto bp : m->warduino->debugger->breakpoints) {
                    requester.write(
                        "%" PRIu32 "%s", toVA(bp),
                        (++i < m->warduino->debugger->breakpoints.size()) ? ","
                                                                          : "");
                }
                requester.write("]");
                break;
            }
            case callstackState: {
                requester.write("%s\"callstack\":[", addComma ? "," : "");
                addComma = true;
                for (int j = 0; j <= m->csp; j++) {
                    Frame *f = &m->callstack[j];
                    uint8_t bt = f->block->block_type;
                    uint32_t block_key =
                        (bt == 0 || bt == 0xff || bt == 0xfe)
                            ? 0
                            : toVA(findOpcode((Module *)m, f->block));
                    uint32_t fidx = bt == 0 ? f->block->fidx : 0;
                    int ra = f->ra_ptr == nullptr ? -1 : toVA(f->ra_ptr);
                    requester.write(
                        R"({"type":%u,"fidx":"0x%x","sp":%d,"fp":%d,"idx":%d,)",
                        bt, fidx, f->sp, f->fp, j);
                    requester.write("\"block_key\":%" PRIu32 ",\"ra\":%d}%s",
                                    block_key, ra, (j < m->csp) ? "," : "");
                }
                requester.write("]");
                break;
            }
            case stackState: {
                requester.write("%s\"stack\":[", addComma ? "," : "");
                addComma = true;
                for (int j = 0; j <= m->sp; j++) {
                    auto v = &m->stack[j];
                    printValue(requester, v, j, j == m->sp);
                }
                requester.write("]");
                break;
            }
            case globalsState: {
                requester.write("%s\"globals\":[", addComma ? "," : "");
                addComma = true;
                for (uint32_t j = 0; j < m->global_count; j++) {
                    auto v = m->globals + j;
                    printValue(requester, v, j, j == (m->global_count - 1));
                }
                requester.write("]");  // closing globals
                break;
            }
            case tableState: {
                requester.write(
                    R"(%s"table":{"max":%d, "init":%d, "elements":[)",
                    addComma ? "," : "", m->table.maximum, m->table.initial);
                addComma = true;
                for (uint32_t j = 0; j < m->table.size; j++) {
                    requester.write("%" PRIu32 "%s", m->table.entries[j],
                                    (j + 1) == m->table.size ? "" : ",");
                }
                requester.write("]}");  // closing table
                break;
            }
            case branchingTableState: {
                requester.write(R"(%s"br_table":{"size":"0x%x","labels":[)",
                                addComma ? "," : "", BR_TABLE_SIZE);
                for (uint32_t j = 0; j < BR_TABLE_SIZE; j++) {
                    requester.write("%" PRIu32 "%s", m->br_table[j],
                                    (j + 1) == BR_TABLE_SIZE ? "" : ",");
                }
                requester.write("]}");
                break;
            }
            case memoryState: {
                uint32_t total_elems = m->memory.pages * (uint32_t)PAGE_SIZE;
                requester.write(
                    R"(%s"memory":{"pages":%d,"max":%d,"init":%d,"bytes":[)",
                    addComma ? "," : "", m->memory.pages, m->memory.maximum,
                    m->memory.initial);
                addComma = true;
                for (uint32_t j = 0; j < total_elems; j++) {
                    requester.write("%" PRIu8 "%s", m->memory.bytes[j],
                                    (j + 1) == total_elems ? "" : ",");
                }
                requester.write("]}");  // closing memory
                break;
            }
            case callbacksState: {
                bool noOuterBraces = false;
                requester.write(
                    "%s%s", addComma ? "," : "",
                    CallbackHandler::dump_callbacksV2(noOuterBraces).c_str());
                addComma = true;
                break;
            }
            case eventsState: {
                requester.write("%s", addComma ? "," : "");
                m->warduino->debugger->dumpEvents(
                    0, CallbackHandler::event_count());
                addComma = true;
                break;
            }
            case errorState: {
                char *errMsg = VM_Exception_has_exception()
                                   ? VM_Exception_get_exception()
                                   : (char *)"";
                requester.write(R"(%s"exception":"%s")", addComma ? "," : "",
                                errMsg);
                addComma = true;
                break;
            }
            case logicalClock: {
                requester.write("%s\"clock\":{\"i\":%" PRIu32 ",\"e\":%" PRIu32
                                "}",
                                addComma ? "," : "",
                                m->warduino->logicalClock.nr_of_instructions,
                                m->warduino->logicalClock.nr_of_events);
                addComma = true;
                break;
            }
            case heapState: {
                uint32_t heap_used = m->warduino->get_heap_used();
                requester.write(R"(%s"heap":{"used":%d})", addComma ? "," : "",
                                heap_used);
                addComma = true;
                break;
            }
            default: {
                debug(
                    "dumpExecutionState: Received unknown state "
                    "request\n");
                return false;
            }
        }
    }
    requester.write("}");
    if (includeNewline) {
        requester.write("\n");
    }
    return true;
}

uint8_t *findOpcode(Module *m, Block *block) {
    auto find =
        std::find_if(std::begin(m->block_lookup), std::end(m->block_lookup),
                     [&](const std::pair<uint8_t *, Block *> &pair) {
                         return pair.second == block;
                     });
    uint8_t *opcode = nullptr;
    if (find != std::end(m->block_lookup)) {
        opcode = find->first;
    } else {
        // FIXME FATAL?
        debug("find_opcode: not found\n");
        exit(33);
    }
    return opcode;
}

void printValue(const Channel &output, StackValue *v, uint32_t idx, bool end) {
    char buff[256];

    switch (v->value_type) {
        case I32:
            snprintf(buff, 255, R"("type":"i32","value":%)" PRIi32,
                     v->value.uint32);
            break;
        case I64:
            snprintf(buff, 255, R"("type":"i64","value":%)" PRIi64,
                     v->value.uint64);
            break;
        case F32:
            snprintf(buff, 255, R"("type":"F32","value":%.9g)", v->value.f32);
            break;
        case F64:
            snprintf(buff, 255, R"("type":"F32","value":%.17g)", v->value.f64);
            break;
        default:
            snprintf(buff, 255, R"("type":"%02x","value":"%)" PRIx64 "\"",
                     v->value_type, v->value.uint64);
    }
    output.write(R"({"idx":%d,%s}%s)", idx, buff, end ? "" : ",");
}

StateToInspect *Interrupt_Inspect_new_state_to_inspect() {
    return new StateToInspect;
}

void Interrupt_Inspect_free_state_to_inspect(StateToInspect *to_free) {
    if (to_free != nullptr && to_free->requestedState != nullptr) {
        free(to_free->requestedState);
    }
    delete to_free;
}

bool Interrupt_Inspect_copy_state_to_inspect(
    StateToInspect &dest, const StateToInspect &state_to_cpy) {
    if (state_to_cpy.requestedState == nullptr ||
        state_to_cpy.numberOfInspects == 0) {
        return false;
    }
    dest.numberOfInspects = state_to_cpy.numberOfInspects;
    dest.requestedState = (ExecutionState *)malloc(
        state_to_cpy.numberOfInspects * sizeof(enum ExecutionState));
    if (dest.requestedState == nullptr) {
        return false;
    }
    memcpy(dest.requestedState, state_to_cpy.requestedState,
           state_to_cpy.numberOfInspects * sizeof(enum ExecutionState));
    return true;
}