#include <iostream>
#include "../debug.h"
#include "../WARDuino.h"
#include "../WARDuino.h"
#include "../instructions.h"
#include <csignal>
#include <sys/mman.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern "C" {
// TODO: Stat files, alternative needed for arduino
#include <sys/stat.h>
// END
}

WARDuino wac;

volatile bool handlingInterrupt = false;

void signalHandler(int /* signum */) {
    if (handlingInterrupt) return;

    printf("CHANGE REQUESTED!");
    struct stat statbuff{};
    if (stat("/tmp/change", &statbuff) == 0 && statbuff.st_size > 0) {
        auto *data = (uint8_t *) malloc(statbuff.st_size * sizeof(uint8_t));
        FILE *fp = fopen("/tmp/change", "rb");
        fread(data, statbuff.st_size, 1, fp);
        fclose(fp);
        wac.handleInterrupt(statbuff.st_size, data);
    }

    handlingInterrupt = false;
}

uint8_t *mmap_file(char *path, int *len) {
    int          fd;
    int          res;
    struct stat  sb;
    uint8_t     *bytes;

    fd = open(path, O_RDONLY);
    if (fd < 0) { FATAL("could not open file '%s'\n", path); }
    res = fstat(fd, &sb);
    if (res < 0) { FATAL("could not stat file '%s' (%d)\n", path, res); }

    bytes = (uint8_t*) mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (len) {
        *len = sb.st_size;  // Return length if requested
    }
    if (bytes == MAP_FAILED) { FATAL("could not mmap file '%s'", path); }
    return bytes;
}

// Parse and add arguments to the stack
void parse_args(Module *m, Type *type, int argc, char argv[][40]) {
    for (int i=0; i<argc; i++) {
        for (int j=0; argv[i][j]; j++) {
            argv[i][j] = tolower(argv[i][j]); // lowecase
        }
        m->sp++;
        StackValue *sv = &m->stack[m->sp];
        sv->value_type = type->params[i];
        switch (type->params[i]) {
        case I32: sv->value.uint32 = strtoul(argv[i], NULL, 0); break;
        case I64: sv->value.uint64 = strtoull(argv[i], NULL, 0); break;
        case F32: if (strncmp("-nan", argv[i], 4) == 0) {
                      sv->value.f32 = -NAN;
                  } else {
                      sv->value.f32 = atof(argv[i]);
                  }; break;
        case F64: if (strncmp("-nan", argv[i], 4) == 0) {
                      sv->value.f64 = -NAN;
                  } else {
                      sv->value.f64 = atof(argv[i]);
                  }; break;
        }
    }
}

void invoke(Module* m, char* call_f, char args[][40]) {
    uint32_t fidx =  -1; 
    m->sp  = -1;
    m->fp  = -1;
    m->csp = -1;
    //TODO move to the WARDuino class     
    for (uint32_t f = 0; f < m->function_count; f++) {
	    char *fname = m->functions[f].export_name;
	    if (!fname) {
		    continue;
	    }
	    if (strncmp(call_f, fname, 1024) == 0) {
		    fidx = f;
		    break;
	    }
    }

    Block *func = &m->functions[fidx];
    Type *type  = func->type; 
    parse_args(m, type, type->param_count, args);
    setup_call(m, fidx);
    interpret(m);

    printf("result :: %llu ", m->stack->value.uint64);
}

/**
 * Run code, ececute interrups in /tmp/change if a USR1 signal comes
*/
int main(int argc, char** argv) {
    uint8_t  *bytes;
    int byte_count;
    signal(SIGUSR1, signalHandler);
    //Load the path name 
    char *mod_path = argv[1];

    bytes = mmap_file(mod_path, &byte_count);
    
    if (bytes == NULL) {
        fprintf(stderr, "Could not load %s", mod_path);
        return 2;
    }

    Module* m = wac.load_module(bytes, byte_count, {});
    //(assert_return (invoke "fac-rec" (i64.const 25)) (i64.const 7034535277573963776))
    char args[][40]  = { "25" };
    char fcall[]     = "fac-rec";
    invoke(m, fcall, args);
    //actual test 
    return 0;
}
