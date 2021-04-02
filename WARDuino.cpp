#include "WARDuino.h"
#include <cmath>
#include <cstring>
#include "instructions.h"
#include "primitives.h"
#include <algorithm>    // std::find
#include <utility>
#include "debug.h"
#include "mem.h"
#include "util.h"

#define UNDEF (uint32_t)(-1)

char exception[512];

// UTIL
bool resolvesym(char *filename, char *symbol, uint8_t external_kind, void **val, char **err) {
    if (nullptr != filename && !strcmp(filename, "env")) {

        switch (external_kind) {
            case 0x00:  // Function
            {
                return resolve_primitive(symbol, (Primitive *) val);
                break;
            }
            case 0x01:  // Table
            {
                *err = (char *) "Unsupported type of import: table (0x01)";
                return false;
            }
            case 0x02:  // Memory
            {
                return resolve_external_memory(symbol, (Memory **) val);
            }
            case 0x03:  // Global
            {
                *err = (char *) "Unsupported type of import: global (0x03)";
                return false;
            }
            default:
                *err = (char *) "Unsupported type of import";
                return false;
        }
    } else {
        *err = (char *) "Imports are only supported from the module env";
        return false;
    }
}

// global exception message
// char  exception[4096];

// Static definition of block_types
uint32_t block_type_results[4][1] = {{I32},
                                     {I64},
                                     {F32},
                                     {F64}};

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
        default: FATAL("invalid block_type value_type: %d\n", value_type);
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
        m->memory.maximum = (uint32_t) fmin(0x8000, pages);
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
            read_LEB_32(pos);         // read x
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
        case 0x0e:                             // br_table
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
    uint8_t opcode = 0x00;dbg_info("  find_blocks: function_count: %d\n", m->function_count);
    for (uint32_t f = m->import_count; f < m->function_count; f++) {
        function = &m->functions[f];
        debug("    fidx: 0x%x, start: 0x%p, end: 0x%p\n", f,
              function->start_ptr, function->end_ptr);
        uint8_t *pos = function->start_ptr;
        while (pos <= function->end_ptr) {
            opcode = *pos;
            switch (opcode) { // NOLINT(hicpp-multiway-paths-covered)
                case 0x02:  // block
                case 0x03:  // loop
                case 0x04:  // if
                    block = (Block *) acalloc(1, sizeof(Block), "Block");
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

void run_init_expr(Module *m, uint8_t type, uint8_t **pc) {
    // Run the init_expr
    Block block;
    block.block_type = 0x01;
    block.type = get_block_type(type);
    block.start_ptr = *pc;

    m->pc_ptr = *pc;
    push_block(m, &block, m->sp);
    // WARNING: running code here to get initial value!
    dbg_info("  running init_expr at 0x%p: %s\n", m->pc_ptr,
             block_repr(&block));
    interpret(m);
    *pc = m->pc_ptr;

    ASSERT(m->stack[m->sp].value_type == type,
           "init_expr type mismatch 0x%x != 0x%x", m->stack[m->sp].value_type,
           type);
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

Module *WARDuino::load_module(uint8_t *bytes, uint32_t byte_count,
                              Options options) {
    debug("Loading module of size %d \n", byte_count);
    uint8_t valueType;
    uint32_t word;
    Module *m;
    // Allocate the module
    m = (Module *) acalloc(1, sizeof(Module), "Module");
    m->warduino = this;
    m->options = options;
    // Empty stacks
    m->sp = -1;
    m->fp = -1;
    m->csp = -1;

    m->bytes = bytes;
    m->byte_count = byte_count;
    // run constructor with already allocated memory
    new(&m->block_lookup) std::map<uint8_t *, Block *>;
    m->start_function = UNDEF;

    // Check the module
    uint8_t *pos = bytes;
    word = read_uint32(&pos);
    debug("Magic number is 0x%x\n", word);
    ASSERT(word == WA_MAGIC, "Wrong module magic 0x%x\n", word);
    word = read_uint32(&pos);
    ASSERT(word == WA_VERSION, "Wrong module version 0x%x\n", word);
    // Read the sections
    uint8_t *bytes_end = bytes + byte_count;
    while (pos < bytes_end) {
        uint32_t id = read_LEB(&pos, 7);
        uint32_t section_len = read_LEB_32(&pos);
        uint8_t *start_pos = pos;
        debug("Reading section %d at 0x%p, length %d\n", id, pos, section_len);
        switch (id) {
            case 0: {
                dbg_warn("Parsing Custom(0) section (length: 0x%x)\n", section_len);
                uint8_t *end_pos = pos + section_len;
                char *name = read_string(&pos, nullptr);
                dbg_warn("  Section name '%s'\n", name);
                if (strncmp(name, "dylink", 7) == 0) {
                    // https://github.com/WebAssembly/tool-conventions/blob/master/DynamicLinking.md
                    // TODO: make use of these
                    uint32_t memorysize = read_LEB_32(&pos);
                    uint32_t tablesize = read_LEB_32(&pos);
                    (void) memorysize;
                    (void) tablesize;
                } else {
                    dbg_warn("Ignoring unknown custom section '%s'\n", name);
                }
                pos = end_pos;
                break;
            }
            case 1: dbg_warn("Parsing Type(1) section (length: 0x%x)\n", section_len);
                m->type_count = read_LEB_32(&pos);
                m->types = (Type *) acalloc(m->type_count, sizeof(Type),
                                            "Module->types");

                for (uint32_t c = 0; c < m->type_count; c++) {
                    Type *type = &m->types[c];
                    type->form = read_LEB(&pos, 7);
                    ASSERT(type->form == FUNC,
                           "%u-th type def was not a function type", c);

                    // read vector params
                    type->param_count = read_LEB_32(&pos);
                    type->params = (uint32_t *) acalloc(
                            type->param_count, sizeof(uint32_t), "type->params");
                    for (uint32_t p = 0; p < type->param_count; p++) {
                        type->params[p] = read_LEB_32(&pos);
                    }

                    // read vector results
                    type->result_count = read_LEB_32(&pos);
                    type->results = (uint32_t *) acalloc(
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
                dbg_warn("Parsing Import(2) section (length: 0x%x)\n", section_len);
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

                    switch (external_kind) { // NOLINT(hicpp-multiway-paths-covered)
                        case 0x00:  // Function
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
                            (void) mutability;
                            break;
                    }

                    void *val;
                    char *err,
                            *sym = (char *) malloc(module_len + field_len + 5);

                    // TODO add special case form primitives with resolvePrim
                    do {
                        // Try using module as handle filename
                        if (resolvesym(import_module, import_field, external_kind, &val, &err)) {
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
                        if (resolvesym(nullptr, sym, external_kind, &val, &err)) {
                            break;
                        }

                        // If enabled, try without the leading underscore (added
                        // by emscripten for external symbols)
                        if (m->options.dlsym_trim_underscore &&
                            (strncmp("env", import_module, 4) == 0) &&
                            (strncmp("_", import_field, 1) == 0)) {
                            sprintf(sym, "%s", import_field + 1);
                            if (resolvesym(nullptr, sym, external_kind, &val, &err)) {
                                break;
                            }
                        }

                        // Try the plain symbol by itself with module
                        // name/handle
                        sprintf(sym, "%s", import_field);
                        if (resolvesym(nullptr, sym, external_kind, &val, &err)) {
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
                            m->functions = (Block *) arecalloc(
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

                            func->func_ptr = (void (*)()) val;
                            break;
                        }
                        case 0x01:  // Table
                        {
                            ASSERT(!m->table.entries,
                                   "More than 1 table not supported\n");
                            Table *tval = (Table *) val;
                            m->table.entries = (uint32_t *) val;
                            ASSERT(m->table.initial <= tval->maximum,
                                   "Imported table is not large enough\n");
                            dbg_warn("  setting table.entries to: %p\n",
                                     *(uint32_t **) val);
                            m->table.entries = *(uint32_t **) val;
                            m->table.size = tval->size;
                            m->table.maximum = tval->maximum;
                            m->table.entries = tval->entries;
                            break;
                        }
                        case 0x02:  // Memory
                        {
                            ASSERT(!m->memory.bytes,
                                   "More than 1 memory not supported\n");
                            auto *mval = (Memory *) val;
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
                            m->globals = (StackValue *) arecalloc(
                                    m->globals, m->global_count - 1,
                                    m->global_count, sizeof(StackValue), "globals");
                            StackValue *glob = &m->globals[m->global_count - 1];
                            glob->value_type = content_type;

                            switch (content_type) { // NOLINT(hicpp-multiway-paths-covered)
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
                        default: FATAL("Import of kind %d not supported\n",
                                       external_kind);
                    }
                }
                break;
            }
            case 3: {
                dbg_warn("Parsing Function(3) section (length: 0x%x)\n", section_len);
                m->function_count += read_LEB_32(&pos);
                debug("  import_count: %d, new count: %d\n", m->import_count,
                      m->function_count);

                Block *functions;
                functions = (Block *) acalloc(m->function_count, sizeof(Block),
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
                m->table.entries = (uint32_t *) acalloc(
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
                m->memory.bytes = (uint8_t *) acalloc(
                        m->memory.pages * PAGE_SIZE, 1, //sizeof(uint32_t),
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
                    (void) mutability;
                    uint32_t gidx = m->global_count;
                    m->global_count += 1;
                    m->globals = (StackValue *) arecalloc(
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
                dbg_warn("Parsing Export(7) section (length: 0x%x)\n", section_len);
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
            dbg_warn("Parsing Start(8) section (length: 0x%x)\n", section_len);
                m->start_function = read_LEB_32(&pos);
                break;
            case 9: {
                dbg_warn("Parsing Element(9) section (length: 0x%x)\n", section_len);
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
                                offset, (uint32_t) ((uint64_t) m->table.entries),
                                offset - (uint32_t) ((uint64_t) m->table.entries));
                        // offset = offset -
                        // (uint32_t)((uint64_t)m->table.entries & 0xFFFFFFFF);
                        offset =
                                offset - (uint32_t) ((uint64_t) m->table.entries);
                    }

                    uint32_t num_elem = read_LEB_32(&pos);
                    dbg_warn("  table.entries: %p, offset: 0x%x\n",
                             m->table.entries, offset);
                    if (!m->options.disable_memory_bounds) {
                        ASSERT(offset + num_elem <= m->table.size,
                               "table overflow %d+%d > %d\n", offset, num_elem,
                               m->table.size);
                    }
                    for (uint32_t n = 0; n < num_elem; n++) {
                        debug(
                                "  write table entries %p, offset: 0x%x, n: 0x%x, "
                                "addr: %p\n",
                                m->table.entries, offset, n,
                                &m->table.entries[offset + n]);
                        m->table.entries[offset + n] =
                                read_LEB_32(&pos);
                    }
                }
                pos = start_pos + section_len;
                break;
                // 9 and 11 are similar so keep them together, 10 is below 11
            }
            case 11: {
                dbg_warn("Parsing Data(11) section (length: 0x%x)\n", section_len);
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
                               "memory overflow %d+%d > %d\n", offset, size,
                               (uint32_t) (m->memory.pages * PAGE_SIZE));
                    }dbg_info(
                            "  setting 0x%x bytes of memory at 0x%p + offset "
                            "0x%x\n",
                            size, m->memory.bytes, offset);
                    memcpy(m->memory.bytes + offset, pos, size);
                    pos += size;
                }

                break;
            }
            case 10: {
                dbg_warn("Parsing Code(10) section (length: 0x%x)\n", section_len);
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
                        (void) tidx;  // TODO: use tidx?
                    }

                    if (function->local_count > 0) {
                        function->local_value_type = (uint8_t *) acalloc(
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
            default: FATAL("Section %d unimplemented\n", id);
                pos += section_len;
        }
    }

    find_blocks(m);

    if (m->start_function != UNDEF) {
        uint32_t fidx = m->start_function;
        bool result;
        dbg_warn("Running start function 0x%x ('%s')\n", fidx,
                 m->functions[fidx].export_name);

        dbg_dump_stack(m);
        ASSERT(m->functions[fidx].type->result_count == 0,
               "start function 0x%x must not have arguments!", fidx);

        if (fidx < m->import_count) {
            // THUNK thunk_out(m, fidx);     // import/thunk call
        } else {
            setup_call(m, fidx);  // regular function call
        }

        if (m->csp < 0) {
            // start function was a direct external call
            result = true;
        } else {
            // run the function setup by setup_call
            result = interpret(m);
        }
        if (!result) {
            FATAL("Exception: %s\n", exception);
        }
    }

    this->modules.push_back(m);

    return m;
}

void WARDuino::unload_module(Module *m) {
    auto it = std::find(this->modules.begin(), this->modules.end(), m);
    if (it != this->modules.end())
        this->modules.erase(it);

    if (m->types != nullptr) {
        for (uint32_t i = 0; i < m->type_count; i++) {
            free(m->types[i].params);
            free(m->types[i].results);
        }
        free(m->types);
    }

    if (m->functions != nullptr) {
        for (uint32_t i = 0; i < m->function_count; ++i) {
            free(m->functions[i].export_name);
        }
        free(m->functions);
    }

    if (m->globals != nullptr)
        free(m->globals);

    if (m->table.entries != nullptr)
        free(m->table.entries);

    if (m->memory.bytes != nullptr)
        free(m->memory.bytes);

    free(m);
}

WARDuino::WARDuino() {
    install_primitives();
    initTypes();
}

// if entry == NULL,  attempt to invoke 'main' or '_main'
// Return value of false means exception occured
bool WARDuino::invoke(Module *m, uint32_t fidx) {
    bool result;
    m->sp = -1;
    m->fp = -1;
    m->csp = -1;

    dbg_trace("Interpretation starts\n");dbg_dump_stack(m);
    setup_call(m, fidx);dbg_trace("Call setup\n");
    result = interpret(m);dbg_trace("Interpretation ended\n");dbg_dump_stack(m);
    return result;
}

int WARDuino::run_module(Module *m) {
    uint32_t fidx = this->get_export_fidx(m, "main");
    if (fidx == UNDEF) fidx = this->get_export_fidx(m, "Main");
    if (fidx == UNDEF) fidx = this->get_export_fidx(m, "_main");
    if (fidx == UNDEF) fidx = this->get_export_fidx(m, "_Main");
    ASSERT(fidx != UNDEF, "Main not found");
    this->invoke(m, fidx);

    return m->stack->value.uint32;
}

// Called when an interrupt comes in (not concurently the same function)
// parse numer per 2 chars (HEX) (stop if non-hex)
// Don't use print in interup handlers
void WARDuino::handleInterrupt(size_t len, uint8_t *buff) {
    for (size_t i = 0; i < len; i++) {
        bool success = true;
        int r = -1 /*undef*/;

        // TODO replace by real binary
        switch (buff[i]) {
            case '0' ... '9':
                r = buff[i] - '0';
                break;
            case 'A' ... 'F':
                r = buff[i] - 'A' + 10;
                break;
            default:
                success = false;
        }

        if (!success) {
            if (this->interruptEven) {
                if (!this->interruptBuffer.empty()) {
                    // done, send to process
                    auto *data = (uint8_t *) acalloc(
                            sizeof(uint8_t), this->interruptBuffer.size(),
                            "interrupt buffer");
                    memcpy(data, this->interruptBuffer.data(),
                           this->interruptBuffer.size() * sizeof(uint8_t));
                    this->parsedInterrups.push_back(data);
                    this->interruptBuffer.clear();
                }
            } else {
                this->interruptBuffer.clear();
                this->interruptEven = true;
                dbg_warn("Dropped interrupt: could not process");
            }
        } else {  // good parse
            if (!this->interruptEven) {
                this->interruptLastChar = (this->interruptLastChar << 4u) + (uint8_t) r;
                this->interruptBuffer.push_back(this->interruptLastChar);
            } else {
                this->interruptLastChar = (uint8_t) r;
            }
            this->interruptEven = !this->interruptEven;
        }
    }
}

uint8_t *WARDuino::getInterrupt() {
    if (!this->parsedInterrups.empty()) {
        uint8_t *ret = this->parsedInterrups.front();
        this->parsedInterrups.pop_front();
        return ret;
    } else {
        return nullptr;
    }
}

void WARDuino::addBreakpoint(uint8_t *loc) { this->breakpoints.insert(loc); }

void WARDuino::delBreakpoint(uint8_t *loc) { this->breakpoints.erase(loc); }

bool WARDuino::isBreakpoint(uint8_t *loc) {
    return this->breakpoints.find(loc) != this->breakpoints.end();
}

// CallbackHandler class

std::unordered_map<std::string, Callback> *CallbackHandler::callbacks = new std::unordered_map<std::string, Callback>();
std::queue<Event> *CallbackHandler::events = new std::queue<Event>();

void CallbackHandler::add_callback(const std::string &id, Callback c) {
    printf("Add Callback(%i)\n", c.function_index);
    callbacks->insert(std::pair<std::string, Callback>(id, c));
}

void CallbackHandler::push_event(char *topic, unsigned char *payload, unsigned int length) {
    // TODO save results to queue
    // TODO generic parameters. Should parameters be added to wasm linear memory?
    // TODO make callback struct a class and move push_event to it
    auto e = new Event(topic, reinterpret_cast<char *>(payload));
    printf("Push event {id: %s, topic: %s, payload: %s}\n", e->callback_function_id.c_str(), e->topic, e->payload);
    events->push(*e);
}

void CallbackHandler::resolve_event() {
    if (CallbackHandler::events->empty()) {
        return;
    }

    Event event = CallbackHandler::events->front();
    CallbackHandler::events->pop();

    auto iterator = CallbackHandler::callbacks->find(event.callback_function_id);
    if (iterator != CallbackHandler::callbacks->end()) {
        iterator->second.resolve_event(event);
    } else {
        // TODO error: event for non-existing iterator
        return;
    }
}

// Callback class

Callback::Callback(Module *m, uint32_t fidx) {
    this->function_index = fidx;
    this->module = m;
}

void Callback::resolve_event(const Event &e) {
    // Save runtime state of VM
    uint8_t *pc_ptr = module->pc_ptr;   // program counter
    int sp = module->sp;                // operand stack pointer
    int fp = module->fp;                // current frame pointer into stack
    StackValue stack[STACK_SIZE];       // main operand stack
    std::copy(std::begin(module->stack), std::end(module->stack), std::begin(stack));
    int csp = module->csp;              // callstack pointer
    Frame callstack[CALLSTACK_SIZE];    // callstack
    std::copy(std::begin(module->callstack), std::end(module->callstack), std::begin(callstack));
    uint32_t br_table[BR_TABLE_SIZE];   // br_table branch indexes
    std::copy(std::begin(module->br_table), std::end(module->br_table), std::begin(br_table));

    // TODO Empty stacks
    module->sp = -1;
    module->fp = -1;
    module->csp = -1;

    // TODO push arguments (5 args)

    // TODO setup function

    // TODO validate argument count

    // TODO call function (call interpret - only callback function on callstack)

    // TODO restore state of VM

    // TODO copy over result of function call
}

// Event class

Event::Event(char *topic, char *payload) {
    this->callback_function_id = "MQTT";
    this->topic = topic;
    this->payload = payload;
}
