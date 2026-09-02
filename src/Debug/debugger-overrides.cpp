#include "debugger-private.h"

std::optional<uint32_t> resolve_imported_function(Module *m,
                                                  std::string function_name) {
    for (uint32_t fidx = 0; fidx < m->import_count; fidx++) {
        if (!strcmp(m->functions[fidx].import_field, function_name.c_str())) {
            return fidx;
        }
    }
    return {};
}

std::string read_string(uint8_t **pos) {
    std::string str;
    char c = *(*pos)++;
    while (c != '\0') {
        str += c;
        c = *(*pos)++;
    }
    return str;
}

void Debugger::add_override(Module *m, uint8_t *interruptData) {
    const std::string primitive_name = read_string(&interruptData);
    const std::optional<uint32_t> fidx =
        resolve_imported_function(m, primitive_name);
    if (!fidx) {
        debug("Cannot override the result for unknown function \"%s\".\n",
              primitive_name.c_str());
        debug("ack%x;0\n", interruptUnsetOverridePinValue);
        return;
    }

    const uint32_t param_count = m->functions[fidx.value()].type->param_count;
    std::vector<uint32_t> key(param_count + 1);
    for (uint32_t i = 0; i < param_count; i++) {
        key[i] = read_B32(&interruptData);
    }
    key[param_count] = fidx.value();

    const uint32_t result = read_B32(&interruptData);
    debug("ack%x;1\n", interruptSetOverridePinValue);
    overrides[key] = result;
}

void Debugger::remove_override(Module *m, uint8_t *interruptData) {
    const std::string primitive_name = read_string(&interruptData);
    const std::optional<uint32_t> fidx =
        resolve_imported_function(m, primitive_name);
    if (!fidx) {
        debug("Cannot remove override for unknown function \"%s\".\n",
              primitive_name.c_str());
        debug("ack%x;0\n", interruptUnsetOverridePinValue);
        return;
    }

    const uint32_t param_count = m->functions[fidx.value()].type->param_count;
    std::vector<uint32_t> key(param_count + 1);
    for (uint32_t i = 0; i < param_count; i++) {
        key[i] = read_B32(&interruptData);
    }
    key[param_count] = fidx.value();

    if (overrides.erase(key) == 0) {
        debug("ack%x;0\n", interruptUnsetOverridePinValue);
        return;
    }
    debug("ack%x;1\n", interruptUnsetOverridePinValue);
}

bool Debugger::get_mock_for_args(Module *m, uint32_t fidx, uint32_t &result) {
    const uint32_t param_count = m->functions[fidx].type->param_count;
    std::vector<uint32_t> key(param_count + 1);
    const ExecutionContext *ectx = m->warduino->execution_context;
    for (uint32_t i = 0; i < param_count; i++) {
        key[i] = ectx->stack[ectx->sp - (param_count - i - 1)].value.uint32;
    }
    key[param_count] = fidx;
    const auto it = overrides.find(key);
    if (it == overrides.end()) {
        return false;
    }
    result = it->second;
    return true;
}

