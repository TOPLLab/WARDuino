
#include "debug.h"
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>

void end() {
    while (1) {
    };
}

#if DEBUG || TRACE || WARN || INFO
char _value_str[256];

char *value_repr(StackValue *v) {
    switch (v->value_type) {
        case I32:
            snprintf(_value_str, 255, "0x%" PRIx32 ":i32", v->value.uint32);
            break;
        case I64:
            snprintf(_value_str, 255, "0x%" PRIx64 ":i64", v->value.uint64);
            break;
        case F32:
            snprintf(_value_str, 255, "%.7g:f32", v->value.f32);
            break;
        case F64:
            snprintf(_value_str, 255, "%.7g:f64", v->value.f64);
            break;
        default:
            snprintf(_value_str, 255, "BAD ENCODING %" PRIx64 ":%02x",
                     v->value.uint64, -v->value_type);
    }
    return _value_str;
}

char _block_str[1024];
char *block_repr(Block *b) {
    if (b->block_type == 0) {
        snprintf(_block_str, 1023, "fn0x%x<%d/%d->%d>", b->fidx,
                 b->type->param_count, b->local_count, b->type->result_count);
    } else {
        snprintf(_block_str, 1023, "%s<0/0->%d>",
                 b->block_type == 0x01
                     ? "init"
                     : b->block_type == 0x02
                           ? "block"
                           : b->block_type == 0x03 ? "loop" : "if",
                 b->type->result_count);
    }
    return _block_str;
}

#endif

#if DEBUG && TRACE
void dbg_dump_stack(Module *m) {
    printf("      * stack:     [");
    for (int i = 0; i <= m->sp; i++) {
        if (i == m->fp) {
            printf("* ");
        }
        printf("%s", value_repr(&m->stack[i]));
        if (i != m->sp) {
            printf(" ");
        }
    }
    printf("]\n");
    printf("      * callstack: [");
    for (int i = 0; i <= m->csp; i++) {
        Frame *f = &m->callstack[i];
        (void)f;
        printf("%s(sp:%d/fp:%d/ra:0x%x)", block_repr(f->block), f->sp, f->fp,
               f->ra);
        if (i != m->csp) {
            printf(" ");
        }
    }
    printf("]\n");
}
#endif