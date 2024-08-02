#include "../WARDuino.h"

#include <algorithm>  // std::find
#include <cmath>
#include <cstring>

#include "../Interpreter/interpreter.h"
#include "../Memory/mem.h"
#include "../Primitives/primitives.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"

#define UNDEF (uint32_t)(-1)
#define pushUInt32(m, arg) m->stack[++(m)->sp].value.uint32 = arg

char exception[512];

// UTIL
bool resolvesym(char *filename, char *symbol, uint8_t external_kind, void **val,
                char **err) {
    if (nullptr != filename && !strcmp(filename, "env")) {
        switch (external_kind) {
            case 0x00:  // Function
            {
                return resolve_primitive(symbol, (Primitive *)val);
                break;
            }
            case 0x01:  // Table
            {
                *err = (char *)"Unsupported type of import: table (0x01)";
                return false;
            }
            case 0x02:  // Memory
            {
                return resolve_external_memory(symbol, (Memory **)val);
            }
            case 0x03:  // Global
            {
                *err = (char *)"Unsupported type of import: global (0x03)";
                return false;
            }
            default:
                *err = (char *)"Unsupported type of import";
                return false;
        }
    } else {
        *err = (char *)"Imports are only supported from the module env";
        return false;
    }
}

// global exception message
// char  exception[4096];

// Static definition of block_types
uint32_t block_type_results[4][1] = {{I32}, {I64}, {F32}, {F64}};

Type block_types[5];

void initTypes() {
    block_types[0].form = BLOCK;
    block_types[0].result_count = 0;
    block_types[1].form = BLOCK;
    block_types[1].result_count = 1;
    block_types[1].results = block_type_results[0];
    block_types[2].form = BLOCK;
    block_types[2].result_count = 1;
    block_types[2].results = block_type_results[1];
    block_types[3].form = BLOCK;
    block_types[3].result_count = 1;
    block_types[3].results = block_type_results[2];
    block_types[4].form = BLOCK;
    block_types[4].result_count = 1;
    block_types[4].results = block_type_results[3];
}

Type *get_block_type(uint8_t value_type) {
    switch (value_type) {
        case 0x40:
            return &block_types[0];
        case I32:
            return &block_types[1];
        case I64:
            return &block_types[2];
        case F32:
            return &block_types[3];
        case F64:
            return &block_types[4];
        default:
            FATAL("invalid block_type value_type: %d\n", value_type);
            return nullptr;
    }
}

// TODO: calculate this while parsing types
uint64_t get_type_mask(Type *type) {
    uint64_t mask = 0x80;

    if (type->result_count == 1) {
        mask |= 0x80 - type->results[0];
    }
    mask = mask << 4u;
    for (uint32_t p = 0; p < type->param_count; p++) {
        mask = mask << 4u;
        mask |= 0x80 - type->params[p];
    }
    return mask;
}

void parse_table_type(Module *m, uint8_t **pos) {
    m->table.elem_type = read_LEB(pos, 7);
    ASSERT(m->table.elem_type == ANYFUNC, "Table elem_type 0x%x unsupported",
           m->table.elem_type);

    uint32_t flags = read_LEB_32(pos);
    uint32_t tsize = read_LEB_32(pos);  // Initial size
    m->table.initial = tsize;
    m->table.size = tsize;
    // Limit maximum to 64K
    if (flags & 0x1u) {
        tsize = read_LEB_32(pos);  // Max size
        m->table.maximum = 0x10000 < tsize ? 0x10000 : tsize;
    } else {
        m->table.maximum = 0x10000;
    }
    debug("  table size: %d\n", tsize);
}

void parse_memory_type(Module *m, uint8_t **pos) {
    uint32_t flags = read_LEB_32(pos);
    uint32_t pages = read_LEB_32(pos);  // Initial size
    m->memory.initial = pages;
    m->memory.pages = pages;
    // Limit the maximum to 2GB
    if (flags & 0x1u) {
        pages = read_LEB_32(pos);  // Max size
        m->memory.maximum = (uint32_t)fmin(0x8000, pages);
    } else {
        m->memory.maximum = 0x8000;
    }
}

void skip_immediates(uint8_t **pos) {
    uint32_t count, opcode = **pos;
    *pos = *pos + 1;
    switch (opcode) {
        // varuint1
        case 0x3f ... 0x40:  // current_memory, grow_memory
            read_LEB(pos, 1);
            break;
            // varuint32, varint32
        case 0x0c ... 0x0d:  // br, br_if
        case 0x10:           // call
        case 0x20 ... 0x24:  // get/set_local, tee_local, get/set_global
        case 0x41:           // i32.const
            read_LEB_32(pos);
            break;
            // varuint32 + varuint1
        case 0x11:  // call_indirect
            // encoding: 0x11 x 0x00
            read_LEB_32(pos);  // read x
            read_LEB(pos, 7);  // 0x00 byte
            break;
            // varint64
        case 0x42:  // i64.const
            read_LEB(pos, 64);
            break;
            // uint32
        case 0x43:  // f32.const
            *pos += 4;
            break;
            // uint64
        case 0x44:  // f64.const
            *pos += 8;
            break;
            // block_type
        case 0x02 ... 0x04:  // block, loop, if
            read_LEB(pos, 7);
            break;
            // memory_immediate
        case 0x28 ... 0x3e:  // *.load*, *.store*
            read_LEB_32(pos);
            read_LEB_32(pos);
            break;
            // br_table
        case 0x0e:                     // br_table
            count = read_LEB_32(pos);  // target count
            for (uint32_t i = 0; i < count; i++) {
                read_LEB_32(pos);
            }
            read_LEB_32(pos);  // default target
            break;
        default:  // no immediates
            break;
    }
}

void find_blocks(Module *m) {
    Block *function;
    Block *block;
    Block *blockstack[BLOCKSTACK_SIZE];
    int top = -1;
    uint8_t opcode = 0x00;
    dbg_info("  find_blocks: function_count: %d\n", m->function_count);
    for (uint32_t f = m->import_count; f < m->function_count; f++) {
        function = &m->functions[f];
        debug("    fidx: 0x%x, start: 0x%p, end: 0x%p\n", f,
              function->start_ptr, function->end_ptr);
        uint8_t *pos = function->start_ptr;
        while (pos <= function->end_ptr) {
            opcode = *pos;
            switch (opcode) {  // NOLINT(hicpp-multiway-paths-covered)
                case 0x02:     // block
                case 0x03:     // loop
                case 0x04:     // if
                    block = (Block *)acalloc(1, sizeof(Block), "Block");
                    block->block_type = opcode;
                    block->type = get_block_type(*(pos + 1));
                    block->start_ptr = pos;
                    blockstack[++top] = block;
                    m->block_lookup[pos] = block;
                    break;
                case 0x05:  // else
                    ASSERT(blockstack[top]->block_type == 0x04,
                           "else not matched with if")
                    blockstack[top]->else_ptr = pos + 1;
                    break;
                case 0x0b:  // end
                    if (pos == function->end_ptr) {
                        break;
                    }
                    ASSERT(top >= 0, "blockstack underflow");
                    block = blockstack[top--];
                    block->end_ptr = pos;
                    if (block->block_type == 0x03) {
                        // loop: label after start
                        block->br_ptr = block->start_ptr + 2;
                    } else {
                        // block, if: label at end
                        block->br_ptr = pos;
                    }
                    debug(
                        "      block start: 0x%p, end: 0x%p,"
                        " br_addr: 0x%p, else_addr: 0x%p\n",
                        block->start_ptr, block->end_ptr, block->br_ptr,
                        block->else_ptr);
                    break;
            }
            skip_immediates(&pos);
        }

        ASSERT(top == -1, "Function ended in middle of block\n")
        ASSERT(opcode == 0x0b, "Function block did not end with 0xb\n")
    }
}
// End Control Instructions

void WARDuino::run_init_expr(Module *m, uint8_t type, uint8_t **pc) {
    // Run the init_expr
    RunningState current = instance()->program_state;
    WARDuino::instance()->program_state = WARDUINOinit;
    Block block;
    block.block_type = 0x01;
    block.type = get_block_type(type);
    block.start_ptr = *pc;

    m->pc_ptr = *pc;
    interpreter->push_block(m, &block, m->sp);
    // WARNING: running code here to get initial value!
    dbg_info("  running init_expr at 0x%p: %s\n", m->pc_ptr,
             block_repr(&block));
    interpreter->interpret(m);
    *pc = m->pc_ptr;

    ASSERT(m->stack[m->sp].value_type == type,
           "init_expr type mismatch 0x%x != 0x%x", m->stack[m->sp].value_type,
           type);
    WARDuino::instance()->program_state = current;
}

//
// Public API
//
uint32_t WARDuino::get_export_fidx(Module *m, const char *name) {
    // Find name function index
    for (uint32_t f = 0; f < m->function_count; f++) {
        char *fname = m->functions[f].export_name;
        if (!fname) {
            continue;
        }
        if (strncmp(name, fname, 1024) == 0) {
            return f;
        }
    }
    return static_cast<uint32_t>(-1);
}

void WARDuino::instantiate_module(Module *m, uint8_t *bytes,
                                  uint32_t byte_count) {
    uint32_t word;
    uint8_t valueType;

    // Allocate stacks
    m->stack = (StackValue *)acalloc(STACK_SIZE, sizeof(StackValue), "Stack");
    m->callstack = (Frame *)acalloc(CALLSTACK_SIZE, sizeof(Frame), "Callstack");
    m->br_table =
        (uint32_t *)acalloc(BR_TABLE_SIZE, sizeof(uint32_t), "Branch table");

    // Empty stacks
    m->sp = -1;
    m->fp = -1;
    m->csp = -1;

    m->bytes = bytes;
    m->byte_count = byte_count;
    // run constructor with already allocated memory
    new (&m->block_lookup) std::map<uint8_t *, Block *>;
    m->start_function = UNDEF;

    // Check the module
    uint8_t *pos = bytes;
    word = read_uint32(&pos);
    debug("Magic number is 0x%x\n", word);
    ASSERT(word == WA_MAGIC, "Wrong module magic 0x%" PRIx32 "\n", word);
    word = read_uint32(&pos);
    ASSERT(word == WA_VERSION, "Wrong module version 0x%" PRIx32 "\n", word);
    // Read the sections
    uint8_t *bytes_end = bytes + byte_count;

    // Needed for run_init_expr
    RunningState oldState = this->program_state;
    this->program_state = WARDUINOrun;

    while (pos < bytes_end) {
        uint32_t id = read_LEB(&pos, 7);
        uint32_t section_len = read_LEB_32(&pos);
        uint8_t *start_pos = pos;
        debug("Reading section %d at 0x%p, length %d\n", id, pos, section_len);
        switch (id) {
            case 0: {
                dbg_warn("Parsing Custom(0) section (length: 0x%x)\n",
                         section_len);
                uint8_t *end_pos = pos + section_len;
                char *name = read_string(&pos, nullptr);
                dbg_warn("  Section name '%s'\n", name);
                if (strncmp(name, "dylink", 7) == 0) {
                    // https://github.com/WebAssembly/tool-conventions/blob/master/DynamicLinking.md
                    // TODO: make use of these
                    uint32_t memorysize = read_LEB_32(&pos);
                    uint32_t tablesize = read_LEB_32(&pos);
                    (void)memorysize;
                    (void)tablesize;
                } else {
                    dbg_warn("Ignoring unknown custom section '%s'\n", name);
                }
                pos = end_pos;
                break;
            }
            case 1:
                dbg_warn("Parsing Type(1) section (length: 0x%x)\n",
                         section_len);
                m->type_count = read_LEB_32(&pos);
                m->types = (Type *)acalloc(m->type_count, sizeof(Type),
                                           "Module->types");

                for (uint32_t c = 0; c < m->type_count; c++) {
                    Type *type = &m->types[c];
                    type->form = read_LEB(&pos, 7);
                    ASSERT(type->form == FUNC,
                           "%" PRIu32 " -th type def was not a function type",
                           c);

                    // read vector params
                    type->param_count = read_LEB_32(&pos);
                    type->params = (uint32_t *)acalloc(
                        type->param_count, sizeof(uint32_t), "type->params");
                    for (uint32_t p = 0; p < type->param_count; p++) {
                        type->params[p] = read_LEB_32(&pos);
                    }

                    // read vector results
                    type->result_count = read_LEB_32(&pos);
                    type->results = (uint32_t *)acalloc(
                        type->result_count, sizeof(uint32_t), "type->results");
                    for (uint32_t r = 0; r < type->result_count; r++) {
                        type->results[r] = read_LEB_32(&pos);
                    }
                    // TODO: calculate this above and remove get_type_mask
                    type->mask = get_type_mask(type);
                    debug("  form: 0x%x, params: %d, results: %d\n", type->form,
                          type->param_count, type->result_count);
                }
                break;
            case 2: {
                dbg_warn("Parsing Import(2) section (length: 0x%x)\n",
                         section_len);
                uint32_t import_count = read_LEB_32(&pos);
                for (uint32_t gidx = 0; gidx < import_count; gidx++) {
                    uint32_t module_len, field_len;
                    char *import_module = read_string(&pos, &module_len);
                    char *import_field = read_string(&pos, &field_len);

                    uint8_t external_kind = *(pos++);  // read byte and move

                    debug("  import: %d/%d, external_kind: %d, %s.%s\n", gidx,
                          import_count, external_kind, import_module,
                          import_field);

                    uint32_t type_index = 0, fidx;
                    uint8_t content_type = 0, mutability;

                    switch (
                        external_kind) {  // NOLINT(hicpp-multiway-paths-covered)
                        case 0x00:        // Function
                            type_index = read_LEB_32(&pos);
                            break;
                        case 0x01:  // Table
                            parse_table_type(m, &pos);
                            break;
                        case 0x02:  // Memory
                            parse_memory_type(m, &pos);
                            break;
                        case 0x03:  // Global
                            content_type = read_LEB(&pos, 7);
                            // TODO: use mutability
                            mutability = read_LEB(&pos, 1);
                            (void)mutability;
                            break;
                    }

                    void *val;
                    char *err,
                        *sym = (char *)malloc(module_len + field_len + 5);

                    // TODO add special case form primitives with resolvePrim
                    do {
                        // Try using module as handle filename
                        if (resolvesym(import_module, import_field,
                                       external_kind, &val, &err)) {
                            break;
                        }

                        // Try concatenating module and field using underscores
                        // Also, replace '-' with '_'
                        sprintf(sym, "_%s__%s_", import_module, import_field);
                        int sidx = -1;
                        while (sym[++sidx]) {
                            if (sym[sidx] == '-') {
                                sym[sidx] = '_';
                            }
                        }
                        if (resolvesym(nullptr, sym, external_kind, &val,
                                       &err)) {
                            break;
                        }

                        // If enabled, try without the leading underscore (added
                        // by emscripten for external symbols)
                        if (m->options.dlsym_trim_underscore &&
                            (strncmp("env", import_module, 4) == 0) &&
                            (strncmp("_", import_field, 1) == 0)) {
                            sprintf(sym, "%s", import_field + 1);
                            if (resolvesym(nullptr, sym, external_kind, &val,
                                           &err)) {
                                break;
                            }
                        }

                        // Try the plain symbol by itself with module
                        // name/handle
                        sprintf(sym, "%s", import_field);
                        if (resolvesym(nullptr, sym, external_kind, &val,
                                       &err)) {
                            break;
                        }

                        FATAL("Error: %s\n", err);
                    } while (false);

                    debug("  found '%s.%s' as symbol '%s' at address %p\n",
                          import_module, import_field, sym, val);
                    free(sym);

                    // Store in the right place
                    switch (external_kind) {
                        case 0x00:  // Function
                        {
                            fidx = m->function_count;
                            m->import_count += 1;
                            m->function_count += 1;
                            m->functions = (Block *)arecalloc(
                                m->functions, fidx, m->import_count,
                                sizeof(Block), "Block(imports)");

                            Block *func = &m->functions[fidx];
                            func->import_module = import_module;
                            func->import_field = import_field;
                            func->type = &m->types[type_index];
                            debug(
                                "  import: %s.%s, fidx: 0x%x, type_index: "
                                "0x%x\n",
                                func->import_module, func->import_field, fidx,
                                type_index);

                            func->func_ptr = (void (*)())val;
                            break;
                        }
                        case 0x01:  // Table
                        {
                            ASSERT(!m->table.entries,
                                   "More than 1 table not supported\n");
                            auto *tval = (Table *)val;
                            m->table.entries = (uint32_t *)val;
                            ASSERT(m->table.initial <= tval->maximum,
                                   "Imported table is not large enough\n");
                            dbg_warn("  setting table.entries to: %p\n",
                                     *(uint32_t **)val);
                            m->table.entries = *(uint32_t **)val;
                            m->table.size = tval->size;
                            m->table.maximum = tval->maximum;
                            m->table.entries = tval->entries;
                            break;
                        }
                        case 0x02:  // Memory
                        {
                            ASSERT(!m->memory.bytes,
                                   "More than 1 memory not supported\n");
                            auto *mval = (Memory *)val;
                            ASSERT(m->memory.initial <= mval->maximum,
                                   "Imported memory is not large enough\n");
                            dbg_warn(
                                "  setting memory pages: %d, max: %d, bytes: "
                                "%p\n",
                                mval->pages, mval->maximum, mval->bytes);
                            m->memory.pages = mval->pages;
                            m->memory.maximum = mval->maximum;
                            m->memory.bytes = mval->bytes;
                            break;
                        }
                        case 0x03:  // Global
                        {
                            m->global_count += 1;
                            m->globals = (StackValue *)arecalloc(
                                m->globals, m->global_count - 1,
                                m->global_count, sizeof(StackValue), "globals");
                            StackValue *glob = &m->globals[m->global_count - 1];
                            glob->value_type = content_type;

                            switch (
                                content_type) {  // NOLINT(hicpp-multiway-paths-covered)
                                case I32:
                                    memcpy(&glob->value.uint32, val, 4);
                                    break;
                                case I64:
                                    memcpy(&glob->value.uint64, val, 8);
                                    break;
                                case F32:
                                    memcpy(&glob->value.f32, val, 4);
                                    break;
                                case F64:
                                    memcpy(&glob->value.f64, val, 8);
                                    break;
                            }
                            debug(
                                "    setting global %d (content_type %d) to "
                                "%p: %s\n",
                                m->global_count - 1, content_type, val,
                                value_repr(glob));
                            break;
                        }
                        default:
                            FATAL("Import of kind %d not supported\n",
                                  external_kind);
                    }
                }
                break;
            }
            case 3: {
                dbg_warn("Parsing Function(3) section (length: 0x%x)\n",
                         section_len);
                m->function_count += read_LEB_32(&pos);
                debug("  import_count: %d, new count: %d\n", m->import_count,
                      m->function_count);

                Block *functions;
                functions = (Block *)acalloc(m->function_count, sizeof(Block),
                                             "Block(function)");
                if (m->import_count != 0) {
                    memcpy(functions, m->functions,
                           sizeof(Block) * m->import_count);
                }
                m->functions = functions;

                for (uint32_t f = m->import_count; f < m->function_count; f++) {
                    uint32_t tidx = read_LEB_32(&pos);
                    m->functions[f].fidx = f;
                    m->functions[f].type = &m->types[tidx];
                    debug("  function fidx: 0x%x, tidx: 0x%x\n", f, tidx);
                }
                break;
            }
            case 4: {
                dbg_warn("Parsing Table(4) section\n");
                uint32_t table_count = read_LEB_32(&pos);
                debug("  table count: 0x%x\n", table_count);
                ASSERT(table_count == 1, "More than 1 table not supported");
                // Allocate the table
                // for (uint32_t c=0; c<table_count; c++) {
                parse_table_type(m, &pos);
                // If it's not imported then don't mangle it
                m->options.mangle_table_index = false;
                m->table.entries = (uint32_t *)acalloc(
                    m->table.size, sizeof(uint32_t), "Module->table.entries");
                //}
                break;
            }
            case 5: {
                dbg_warn("Parsing Memory(5) section\n");
                uint32_t memory_count = read_LEB_32(&pos);
                debug("  memory count: 0x%x\n", memory_count);
                ASSERT(memory_count == 1, "More than 1 memory not supported\n");
                // Allocate memory
                // for (uint32_t c=0; c<memory_count; c++) {
                parse_memory_type(m, &pos);
                m->memory.bytes = (uint8_t *)acalloc(
                    m->memory.pages * PAGE_SIZE, 1,  // sizeof(uint32_t),
                    "Module->memory.bytes");
                //}
                break;
            }
            case 6: {
                dbg_warn("Parsing Global(6) section\n");
                uint32_t global_count = read_LEB_32(&pos);
                for (uint32_t g = 0; g < global_count; g++) {
                    // Same allocation Import of global above
                    uint8_t type = read_LEB(&pos, 7);
                    // TODO: use mutability
                    uint8_t mutability = read_LEB(&pos, 1);
                    (void)mutability;
                    uint32_t gidx = m->global_count;
                    m->global_count += 1;
                    m->globals = (StackValue *)arecalloc(
                        m->globals, gidx, m->global_count, sizeof(StackValue),
                        "globals");
                    m->globals[gidx].value_type = type;

                    // Run the init_expr to get global value
                    run_init_expr(m, type, &pos);

                    m->globals[gidx] = m->stack[m->sp--];
                }
                pos = start_pos + section_len;
                break;
            }
            case 7: {
                dbg_warn("Parsing Export(7) section (length: 0x%x)\n",
                         section_len);
                uint32_t export_count = read_LEB_32(&pos);
                for (uint32_t e = 0; e < export_count; e++) {
                    char *name = read_string(&pos, nullptr);

                    uint32_t kind = *(pos++);  // read and move pos
                    uint32_t index = read_LEB_32(&pos);
                    if (kind != 0x00) {
                        dbg_warn(
                            "  ignoring non-function export '%s'"
                            " kind 0x%x index 0x%x\n",
                            name, kind, index);
                        continue;
                    }
                    m->functions[index].export_name = name;
                    debug("  export: %s (0x%x)\n", name, index);
                }
                break;
            }
            case 8:
                /**
                 * If the module has a start node defined, the function it
                 * refers should be called by the loader after the instance is
                 * initialized, including its Memory and Table though Data and
                 * Element sections, and before the exported functions are
                 * callable. The start function must not take any arguments or
                 * return anything The function is identified by function index,
                 * can be an import, and can also be exported There can only be
                 * at most one start node per module
                 */
                dbg_warn("Parsing Start(8) section (length: 0x%x)\n",
                         section_len);
                m->start_function = read_LEB_32(&pos);
                break;
            case 9: {
                dbg_warn("Parsing Element(9) section (length: 0x%x)\n",
                         section_len);
                uint32_t element_count = read_LEB_32(&pos);

                for (uint32_t c = 0; c < element_count; c++) {
                    uint32_t index = read_LEB_32(&pos);
                    ASSERT(index == 0, "Only 1 default table in MVP");

                    // Run the init_expr to get offset
                    run_init_expr(m, I32, &pos);

                    uint32_t offset = m->stack[m->sp--].value.uint32;

                    if (m->options.mangle_table_index) {
                        // offset is the table address + the index (not sized
                        // for the pointer size) so get the actual (sized) index
                        debug(
                            "   origin offset: 0x%x, table addr: 0x%x, new "
                            "offset: 0x%x\n",
                            offset, (uint32_t)((uint64_t)m->table.entries),
                            offset - (uint32_t)((uint64_t)m->table.entries));
                        // offset = offset -
                        // (uint32_t)((uint64_t)m->table.entries & 0xFFFFFFFF);
                        offset =
                            offset - (uint32_t)((uint64_t)m->table.entries);
                    }

                    uint32_t num_elem = read_LEB_32(&pos);
                    dbg_warn("  table.entries: %p, offset: 0x%x\n",
                             m->table.entries, offset);
                    if (!m->options.disable_memory_bounds) {
                        ASSERT(offset + num_elem <= m->table.size,
                               "table overflow %" PRIu32 "+%" PRIu32
                               " > %" PRIu32 "\n",
                               offset, num_elem, m->table.size);
                    }
                    for (uint32_t n = 0; n < num_elem; n++) {
                        debug(
                            "  write table entries %p, offset: 0x%x, n: 0x%x, "
                            "addr: %p\n",
                            m->table.entries, offset, n,
                            &m->table.entries[offset + n]);
                        m->table.entries[offset + n] = read_LEB_32(&pos);
                    }
                }
                pos = start_pos + section_len;
                break;
                // 9 and 11 are similar so keep them together, 10 is below 11
            }
            case 11: {
                dbg_warn("Parsing Data(11) section (length: 0x%x)\n",
                         section_len);
                uint32_t seg_count = read_LEB_32(&pos);
                for (uint32_t s = 0; s < seg_count; s++) {
                    uint32_t midx = read_LEB_32(&pos);
                    ASSERT(midx == 0, "Only 1 default memory in MVP");

                    // Run the init_expr to get the offset
                    run_init_expr(m, I32, &pos);

                    uint32_t offset = m->stack[m->sp--].value.uint32;

                    // Copy the data to the memory offset
                    uint32_t size = read_LEB_32(&pos);
                    if (!m->options.disable_memory_bounds) {
                        ASSERT(offset + size <= m->memory.pages * PAGE_SIZE,
                               "memory overflow %" PRIu32 "+%" PRIu32
                               " > %" PRIu32 "\n",
                               offset, size,
                               (uint32_t)(m->memory.pages * PAGE_SIZE));
                    }
                    dbg_info(
                        "  setting 0x%x bytes of memory at 0x%p + offset "
                        "0x%x\n",
                        size, m->memory.bytes, offset);
                    memcpy(m->memory.bytes + offset, pos, size);
                    pos += size;
                }

                break;
            }
            case 10: {
                dbg_warn("Parsing Code(10) section (length: 0x%x)\n",
                         section_len);
                uint32_t body_count = read_LEB_32(&pos);
                for (uint32_t b = 0; b < body_count; b++) {
                    Block *function = &m->functions[m->import_count + b];
                    uint32_t body_size = read_LEB_32(&pos);
                    uint8_t *payload_start = pos;
                    uint8_t *save_pos;
                    uint32_t local_count = read_LEB_32(&pos);
                    uint32_t tidx, lidx, lecount;

                    // Local variable handling

                    // Get number of locals for alloc
                    save_pos = pos;
                    function->local_count = 0;
                    for (uint32_t l = 0; l < local_count; l++) {
                        lecount = read_LEB_32(&pos);
                        function->local_count += lecount;
                        tidx = read_LEB(&pos, 7);
                        (void)tidx;  // TODO: use tidx?
                    }

                    if (function->local_count > 0) {
                        function->local_value_type = (uint8_t *)acalloc(
                            function->local_count, sizeof(uint8_t),
                            "function->local_value_type");
                    }

                    // Restore position and read the locals
                    pos = save_pos;
                    lidx = 0;
                    for (uint32_t l = 0; l < local_count; l++) {
                        lecount = read_LEB_32(&pos);
                        valueType = read_LEB(&pos, 7);
                        for (uint32_t i = 0; i < lecount; i++) {
                            function->local_value_type[lidx++] = valueType;
                        }
                    }

                    function->start_ptr = pos;
                    function->end_ptr = payload_start + body_size - 1;
                    function->br_ptr = function->end_ptr;
                    ASSERT(*(function->end_ptr) == 0x0b,
                           "Code section did not end with 0x0b\n");
                    pos = function->end_ptr + 1;
                }
                break;
            }
            default:
                FATAL("Section %" PRIu32 " unimplemented\n", id);
                pos += section_len;
        }
    }

    find_blocks(m);
    debug("findblocks finished\n");

    if (m->start_function != UNDEF) {
        debug("has startfun \n");
        uint32_t fidx = m->start_function;
        bool result;
        // dbg_warn("Running start function 0x%x ('%s')\n", fidx,
        // m->functions[fidx].export_name);

        // dbg_dump_stack(m);

        ASSERT(m->functions[fidx].type->result_count == 0,
               "start function 0x%" PRIx32 " must not have arguments!", fidx);

        if (fidx < m->import_count) {
            // THUNK thunk_out(m, fidx);     // import/thunk call
        } else {
            interpreter->setup_call(m, fidx);  // regular function call
        }

        if (m->csp < 0) {
            // start function was a direct external call
            result = true;
        } else {
            // run the function setup by setup_call
            debug("running startfun \n");
            result = interpreter->interpret(m);
        }
        if (!result) {
            FATAL("Exception: %s\n", exception);
        }
    }

    this->program_state = oldState;
}

Module *WARDuino::load_module(uint8_t *bytes, uint32_t byte_count,
                              Options options) {
    debug("Loading module of size %d \n", byte_count);
    Module *m;
    // Allocate the module
    m = (Module *)acalloc(1, sizeof(Module), "Module");
    m->warduino = this;
    m->options = options;

    this->instantiate_module(m, bytes, byte_count);

    this->modules.push_back(m);

    debug("return module \n");
    return m;
}

void WARDuino::unload_module(Module *m) {
#ifndef ARDUINO
    this->debugger
        ->disconnect_proxy();  // TODO should this be in unload module?
#endif
    auto it = std::find(this->modules.begin(), this->modules.end(), m);
    if (it != this->modules.end()) this->modules.erase(it);
    this->free_module_state(m);
    free(m);
}

WARDuino::WARDuino() {
    this->debugger = new Debugger(0);
    this->interpreter = new Interpreter();
    install_primitives();
    initTypes();
}

// Return value of false means exception occurred
bool WARDuino::invoke(Module *m, uint32_t fidx, uint32_t arity,
                      StackValue *args) {
    bool result;
    m->sp = -1;
    m->fp = -1;
    m->csp = -1;

    for (uint32_t i = 0; i < arity; ++i) {
        m->stack[++m->sp] = *args;
        args++;
    }

    dbg_trace("Interpretation starts\n");
    dbg_dump_stack(m);
    interpreter->setup_call(m, fidx);
    dbg_trace("Call setup\n");
    result = interpreter->interpret(m);
    dbg_trace("Interpretation ended\n");
    dbg_dump_stack(m);
    return result;
}

void WARDuino::setInterpreter(Interpreter *interpreter) {
    this->interpreter = interpreter;
}

int WARDuino::run_module(Module *m) {
    uint32_t fidx = this->get_main_fidx(m);

    // execute main
    if (fidx != UNDEF) {
        this->invoke(m, fidx);
        return m->stack->value.uint32;
    }

    // wait
    m->warduino->debugger->pauseRuntime(m);
    return interpreter->interpret(m, true);
}

// Called when an interrupt comes in (not concurre
// ntly the same function)
// parse numer per 2 chars (HEX) (stop if non-hex)
// Don't use print in interrupt handlers
void WARDuino::handleInterrupt(size_t len, uint8_t *buff) const {
    this->debugger->addDebugMessage(len, buff);
}

WARDuino *WARDuino::singleton = nullptr;

WARDuino *WARDuino::instance() {
    if (singleton == nullptr) singleton = new WARDuino();
    return singleton;
}

// Removes all the state of a module
void WARDuino::free_module_state(Module *m) {
    if (m->types != nullptr) {
        for (uint32_t i = 0; i < m->type_count; i++) {
            free(m->types[i].params);
            free(m->types[i].results);
        }
        free(m->types);
        m->types = nullptr;
    }

    if (m->functions != nullptr) {
        for (uint32_t i = 0; i < m->function_count; ++i)
            free(m->functions[i].export_name);
        free(m->functions);
        m->functions = nullptr;
    }

    if (m->globals != nullptr) {
        free(m->globals);
        m->globals = nullptr;
    }

    if (m->table.entries != nullptr) {
        free(m->table.entries);
        m->table.entries = nullptr;
    }

    if (m->memory.bytes != nullptr) {
        free(m->memory.bytes);
        m->memory.bytes = nullptr;
    }

    if (m->stack != nullptr) {
        free(m->stack);
        m->stack = nullptr;
    }

    if (m->callstack != nullptr) {
        for (int j = 0; j <= m->csp; j++) {
            Frame *f = &m->callstack[j];
            if (f->block != nullptr && (f->block->block_type == 0xfe ||
                                        f->block->block_type == 0xff)) {
                free(f->block);
            }
        }
        free(m->callstack);
        m->callstack = nullptr;
    }

    if (m->br_table != nullptr) {
        free(m->br_table);
        m->br_table = nullptr;
    }

    m->function_count = 0;
    m->byte_count = 0;
    m->type_count = 0;

    m->import_count = 0;
    m->global_count = 0;
    m->pc_ptr = 0;
    m->sp = -1;
    m->fp = -1;
    m->csp = -1;

    if (m->exception != nullptr) {
        free(m->exception);  // safe to remove?
    }

    m->memory.pages = 0;
    m->memory.initial = 0;
    m->memory.maximum = 0;
    m->table.elem_type = 0;
    m->table.initial = 0;
    m->table.maximum = 0;
    m->table.size = 0;

    m->block_lookup.clear();
}

void WARDuino::update_module(Module *m, uint8_t *wasm, uint32_t wasm_len) {
    m->warduino->program_state = WARDUINOinit;

    this->free_module_state(m);
    this->instantiate_module(m, wasm, wasm_len);
    uint32_t fidx = this->get_main_fidx(m);

    // execute main
    if (fidx != UNDEF) {
        interpreter->setup_call(m, fidx);
        m->warduino->program_state = WARDUINOrun;
    }

    // wait
    m->warduino->debugger->pauseRuntime(m);
}

uint32_t WARDuino::get_main_fidx(Module *m) {
    uint32_t fidx = this->get_export_fidx(m, "main");
    if (fidx == UNDEF) fidx = this->get_export_fidx(m, "Main");
    if (fidx == UNDEF) fidx = this->get_export_fidx(m, "_main");
    if (fidx == UNDEF) fidx = this->get_export_fidx(m, "_Main");
    return fidx;
}