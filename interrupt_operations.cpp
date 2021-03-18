#include "interrupt_operations.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <inttypes.h>
#include <iterator>
#include "string.h"
#include "debug.h"
#include "mem.h"
#include "util.h"
#include "WARDuino.h"
#include "interrupt_protocol.h"

/**
 * Validate if there are interrupts and execute them
 *
 * The various kinds of interrups are preceded by an identifier:
 *
 * - `0x01` : Continue running
 * - `0x02` : Halt the execution
 * - `0x03` : Pause execution
 * - `0x04` : Execute one operaion and then pause
 * - `0x06` : Add a breakpoint, the adress is specified as a pointer.
 *            The pointer should be specified as: 06[length][pointer]
 *            eg: 06 06 55a5994fa3d6
 * - `0x07` : Remove the breakpoint at the adress specified as a pointer if it
 *            exists (see `0x06`)
 * - `0x10` : Dump information about the program
 * - `0x11` :                  show locals
 * - `0x20` : Replace the content body of a function by a new function given
 *            as payload (immediately following `0x10`), see #readChange
 */

enum InteruptTypes {
    interruptRUN = 0x01,
    interruptHALT = 0x02,
    interruptPAUSE = 0x03,
    interruptSTEP = 0x04,
    interruptBPAdd = 0x06,
    interruptBPRem = 0x07,
    interruptDUMP = 0x10,
    interruptDUMPLocals = 0x11,
    interruptUPDATEFun = 0x20,
    interruptUPDATELocal = 0x21,
		interruptConnect = 0x22,
		interruptRecvState = 0x23
};

enum ReceiveState {
		pcState = 0x01,
		breakpointsState = 0x02,
		callstackState = 0x03,
		globalsState = 0x04,
		tblState = 0x05,
		memState = 0x06,
		brtblState= 0x07,
		stackvalsState = 0x08
};

bool receivingData = false; 

void doDumpLocals(Module *m);

void freeState(Module *m, uint8_t *interruptData);

uintptr_t readPointer(uint8_t **data);

bool saveState(Module *m, uint8_t *interruptData);

//TODO inefficient. Keep extra state at each pushblock to ease opcode retrieval?
uint8_t* find_opcode(Module *m, Block * block){
		auto find = std::find_if(std::begin(m->block_lookup), std::end(m->block_lookup),
						[&](const std::pair<uint8_t*, Block *> &pair){
						return pair.second == block;
		});
		uint8_t* opcode  = nullptr;
		if (find != std::end(m->block_lookup)){
				opcode = find->first;
		}
		else{
				//FIXME FATAL?
			debug("Result not found\n");
			exit(33);
		}
		return opcode;
}

void format_constant_value(char *buf, StackValue *v) {
  switch (v->value_type) {
  case I32:
    snprintf(buf, 255, R"("type":"i32","value":%)" PRIi32, v->value.uint32);
    break;
  case I64:
    snprintf(buf, 255, R"("type":"i64","value":%)" PRIi64, v->value.uint64);
    break;
  case F32:
    snprintf(buf, 255, R"("type":"f32","value":%.7f)", v->value.f32);
    break;
  case F64:
    snprintf(buf, 255, R"("type":"f64","value":%.7f)", v->value.f64);
    break;
  default:
    snprintf(buf, 255, R"("type":"%02x","value":"%)" PRIx64 "\"", v->value_type,
             v->value.uint64);
  }
}


void doDump(Module *m) {
  debug("asked for doDump\n");
  m->warduino->printing.longPrint = true;

  m->warduino->printing.printo(DUMP_START);
  m->warduino->printing.printo("{");

  // current PC
  m->warduino->printing.printo(R"("pc":"%p",)", (void *)m->pc_ptr);
  // current FP //TODO remove
  m->warduino->printing.printo(R"("fp":"%d",)", m->fp);
	//
  // start of bytes
  m->warduino->printing.printo(R"("start":["%p"],)", (void *)m->bytes);

  m->warduino->printing.printo("\"breakpoints\":[");
	debug("getting breakpoints\n");
  {
    size_t i = 0;
    for (auto bp : m->warduino->breakpoints) {
      m->warduino->printing.printo(R"("%p"%s)", bp,
             (++i < m->warduino->breakpoints.size()) ? "," : "");
    }
  }
  m->warduino->printing.printo("],");
  // Functions

	debug("getting functions\n");
  m->warduino->printing.printo("\"functions\":[");

  for (size_t i = m->import_count; i < m->function_count; i++) {
    //TODO remove extra unnecessery function state. 
    m->warduino->printing.printo(R"({"fidx":"0x%x","from":"%p","to":"%p","args":%d,"locs":%d}%s)", m->functions[i].fidx,
           static_cast<void *>(m->functions[i].start_ptr),
           static_cast<void *>(m->functions[i].end_ptr),
           m->functions[i].type->param_count,
           m->functions[i].local_count,
           (i < m->function_count - 1) ? "," : "");
  }

	debug("getting callstack\n");
  // Callstack
  m->warduino->printing.printo("],\"callstack\":[");
  for (int i = 0; i <= m->csp; i++) {
		debug("getting frame %d\n", i);
    Frame *f = &m->callstack[i];
		if (f->block->block_type == 0)
				debug("is func idx %d frame %d\n", f->block->fidx, i);
		uint8_t *block_key = f->block->block_type == 0 ? nullptr : find_opcode(m, f->block);
    m->warduino->printing.printo(R"({"type":%u,"fidx":"0x%x","sp":%d,"fp":%d,"block_key":"%p", "ra":"%p"}%s)",
           f->block->block_type, f->block->fidx, f->sp, f->fp, block_key,
           static_cast<void *>(f->ra_ptr), (i < m->csp) ? "," : "");
  }
	debug("getting globals\n");
  // GLobals
  m->warduino->printing.printo("],\"globals\":[");
  for (int i = 0; i < m->global_count; i++) {
    char _value_str[256];
    auto v = m->globals + i;
    format_constant_value(_value_str, v);
    m->warduino->printing.printo(R"({"idx":%d,%s}%s)", i, _value_str, ((i+1) < m->global_count) ? "," : "");
  }
  m->warduino->printing.printo("]");//closing globals

	debug("getting table\n");
  m->warduino->printing.printo(",\"table\":{\"max\":%d, \"elements\":[", m->table.maximum);
	m->warduino->printing.printRaw(m->table.size, sizeof(uint32_t), (char *) m->table.entries);
  m->warduino->printing.printo("]}");//closing table

	debug("getting memory\n");
  //memory
  uint32_t total_elems = m->memory.pages * (uint32_t) PAGE_SIZE;//TODO debug PAGE_SIZE
  m->warduino->printing.printo(",\"memory\":{\"pages\":%d,\"total\":%d,\"bytes\":[", m->memory.pages, total_elems);
	m->warduino->printing.printRaw(total_elems, sizeof(uint8_t), (char *) m->memory.bytes);
  m->warduino->printing.printo("]}");//closing memory

	debug("getting br_table\n");
  m->warduino->printing.printo(",\"br_table\":{\"size\":\"0x%x\",\"labels\":[",BR_TABLE_SIZE);
	m->warduino->printing.printRaw((uint32_t) BR_TABLE_SIZE, sizeof(uint32_t),(char*) m->br_table);
	m->warduino->printing.printo("]}");
  m->warduino->printing.longPrint = false;
  m->warduino->printing.printo("}\n%s",DUMP_END); //closing dump
}

uint32_t read_B32(uint8_t **bytes){
		uint8_t *b = *bytes;
		uint32_t n = (b[0] << 24) + (b[1] << 16) + (b[2] << 8) + b[3];
		*bytes +=4;
		return n;
}//TODO replace with read_LEB_32?

int read_B32_signed(uint8_t **bytes){
		uint8_t *b = *bytes;
		int n = (b[0] << 24) + (b[1] << 16) + (b[2] << 8) + b[3];
		*bytes +=4;
		return n;
}//TODO replace with read_LEB_32?

uint16_t read_B16(uint8_t **bytes){
		uint8_t *b = *bytes;
		uint32_t n = (b[0] << 8) + b[1];
		*bytes +=2;
		return n;
}

void freeState(Module *m, uint8_t* interruptData){
	debug("freeing the program state\n");
	uint8_t * first_msg = nullptr;
	uint8_t * endfm =  nullptr;
	first_msg = interruptData + 1; //skip interruptRecvState
	endfm =  first_msg + read_B32(&first_msg);
	//removing all breakpoints
	m->warduino->breakpoints.clear();
	m->csp = -1;
	m->sp = -1;
	//TODO first free memory
	//TODO allocate memory where needed
	while(first_msg < endfm){
			switch (*first_msg++) {
				case globalsState:{
							 debug("receiving globals info\n");
							 uint32_t amount = read_B32(&first_msg);
							 debug("total globals %d\n", amount);
							 break;
					}
				case tblState:{
							 debug("receiving table info\n");
							 m->table.initial = read_B32(&first_msg);
							 m->table.maximum = read_B32(&first_msg);
							 uint32_t size = read_B32(&first_msg);
							 debug("init %d max %d size %d\n",
											 m->table.initial, m->table.maximum, size);
							 if(m->table.size != size){
									 debug("old table size %d\n", m->table.size);
									 if(m->table.size !=0 )
											 free(m->table.entries);
									 m->table.entries = (uint32_t *) acalloc(
                        size, sizeof(uint32_t), "Module->table.entries");
							 }
							 m->table.size = 0; //allows to accumulatively add entries
							 break;
					}
				case memState:{
							 debug("receiving memory info\n");
							 uint32_t max = read_B32(&first_msg); //FIXME: init & max not needed
							 uint32_t init = read_B32(&first_msg);
							 uint32_t page = read_B32(&first_msg);
							 debug("max %d init %d current page %d\n", max, init, page);
							 break;
					}
				default: {
							 debug("receiving unknown command\n");
							 exit(33); //FIXME replace
							 break;
					}
			}
	}
	debug("done with first msg\n");
}



uintptr_t readPointer(uint8_t **data){
		uint8_t len = (*data)[0];
		uintptr_t bp = 0x0;
		for (size_t i = 0; i < len; i++) {
				bp <<= sizeof(uint8_t) * 8;
				bp |= (*data)[i + 1];
		}
		*data += 1 + len; //skip pointer
		return bp;
}

bool saveState(Module *m, uint8_t *interruptData){
	uint8_t *program_state = nullptr;
	uint8_t *endstate = nullptr;
	program_state = interruptData + 1; //skip interruptRecvState
	endstate = program_state + read_B32(&program_state);

	while (program_state < endstate) {
			switch (*program_state++) {
					case pcState: {// PC
						debug("receiving new pc\n");
						m->pc_ptr = (uint8_t *) readPointer(&program_state);
						debug("new pc %p\n", static_cast<void *>(m->pc_ptr));
						break;
					}
					case breakpointsState: { //breakpoints
						uint8_t quantity_bps = *program_state++;
						debug("receiving new breakpoints %" PRIu8 "\n", quantity_bps);
						for (size_t i = 0; i < quantity_bps; i++) {
								auto bp =(uint8_t *) readPointer(&program_state);
								debug("bp %p\n", static_cast<void *>(bp));
								m->warduino->addBreakpoint(bp);
						}
						debug("done with bps\n");
						break;
					}
				case callstackState:{
				  debug("receiving callstack\n");
				  uint16_t quantity = read_B16(&program_state);
					debug("quantity frames %" PRIu16 "\n", quantity);
			   	for (size_t i = 0; i < quantity; i++){
							uint8_t block_type = *program_state;
							program_state++;
							m->csp +=1;
							Frame f = m->callstack[m->csp];
							f.sp = read_B32_signed(&program_state);
							f.fp = read_B32_signed(&program_state);
							f.ra_ptr = (uint8_t *) readPointer(&program_state);
						  debug("type=%" PRIu8 " sp=%d fp=%d ra_ptr=%p\n",
											block_type, f.sp, f.fp,
											static_cast<void*> (f.ra_ptr));
							if(block_type == 0){ // a function
									debug("function block\n");
									uint32_t fidx = read_B32(&program_state);
									debug("function block idx=%" PRIu32 "\n", fidx);
									Block fblock = m->functions[fidx];
									debug("block for fidx=%" PRIu32 "\n", fblock.fidx);
									if(fblock.fidx != fidx){
										exit(54);
									}
									f.block = &fblock;//m->functions[fidx];
									//FIXME fix incorrect fp
									//m->fp = m->sp - ((int) type->param_count) + 1;
									m->fp = f.fp;
									debug("new m->fp=%d\n", m->fp);
							}
							else {
									debug("non function block\n");
									uint8_t *block_key = (uint8_t*) readPointer(&program_state);
									debug("block_key=%p\n",
													static_cast<void*> (block_key));
									f.block = m->block_lookup[block_key];
							}
					}
					debug("new top callstack %d\n", m->csp);
					break;
				}
				case globalsState:{
										//GLobals
									 break;
							 }
				case tblState:{
						debug("receivin table state\n");
						uint8_t tbl_type = (uint8_t) *program_state++; //for now only funcref
						uint32_t quantity = read_B32(&program_state);
						debug("table type %" PRIu8 " quantity of elements %" PRIu32 "\n",
										tbl_type, quantity);
						for (size_t i = 0; i < quantity; i++) {
								uint32_t ne = read_B32(&program_state);
								m->table.entries[m->table.size++] = ne;
								debug("table entry added %" PRIu32 " at %d\n",
												m->table.entries[m->table.size -1],
												m->table.size -1);
						}
				    break;
				}
				case memState: {
										//memory 
										break;
								}
				case brtblState: {
										//BR_Talbe
									break;
								}
				case stackvalsState: {
						//FIXME the float does add numbers at the end. The extra numbers are present in the send information when dump occurs
						debug("receiving stack values\n");
						uint16_t quantity_sv = read_B16(&program_state);
						debug("read quantity_sv %" PRIu16 "\n", quantity_sv);
						for (size_t i = 0; i < quantity_sv; i++) {
								uint8_t valtype = *program_state++;
								m->sp +=1;
								StackValue *sv = &m->stack[m->sp];
								sv->value.uint64 = 0; //init whole union to 0
						    char _value_str[256];
								switch (valtype){ //TODO clean with if else
										case 0:{
										 debug("receiving i32\n");
										 sv->value_type = I32;
										 memcpy(&sv->value, program_state, 4);
										 program_state +=4;
										 break;
										}
										case 1:{
										 debug("receiving i64\n");
										 sv->value_type = I64;
										 memcpy(&sv->value, program_state, 8);
										 program_state +=8;
										 break;
										}
										case 2:{
										 debug("receiving f32\n");
										 sv->value_type = F32;
										 memcpy(&sv->value, program_state, 4);
										 program_state +=4;
										 break;
										}
										case 3:{
										 debug("receiving f64\n");
										 sv->value_type = F64;
										 memcpy(&sv->value, program_state, 8);
										 program_state +=8;
										 break;
										}
										default:{
										 debug("received unknown type %" PRIu8 "\n", valtype);
										 exit(55);
										}
								}
								format_constant_value(_value_str, sv);
								debug("idx: %d, %s\n", (m->sp - 1 ), _value_str);
						}
						break;
				}
				default:{
									debug("Reiceived unknown program state\n");
									exit(33);
								}
			 }
	}		
	uint8_t done =(uint8_t) *program_state;
	debug("done with receiving state %" PRIu8 "\n", done);
	return  done == (uint8_t) 1;
}

void dump_stack_values(Module *m) {
  fflush(stdout);
	m->warduino->printing.longPrint = true;
  m->warduino->printing.printo(DUMP_STACK_START);
	m->warduino->printing.printo(R"({"stack":[)");
  char _value_str[256];
  for (int i = 0; i <= m->sp; i++) {
    auto v = &m->stack[i];
    format_constant_value(_value_str, v);
		m->warduino->printing.printo(R"({"idx":%d, %s}%s)", i, _value_str, (i == m->sp) ? "" : ",");
  }
	m->warduino->printing.printo("]}\n");
	m->warduino->printing.longPrint = false;
	m->warduino->printing.printo(DUMP_STACK_END);
}

void doDumpLocals(Module *m) {
    fflush(stdout);
    printf("DUMP LOCALS!\n\n");
    fflush(stdout);
    int firstFunFramePtr = m->csp;
    while (m->callstack[firstFunFramePtr].block->block_type != 0) {
        firstFunFramePtr--;
        if (firstFunFramePtr < 0) {
            FATAL("Not in a function!");
        }
    }
    Frame *f = &m->callstack[firstFunFramePtr];
    printf(R"({"count":%u,"locals":[)", 0);
    fflush(stdout);  // FIXME: this is needed for ESP to propery print
    char _value_str[256];
    for (size_t i = 0; i < f->block->local_count; i++) {
        auto v = &m->stack[m->fp + i];
        switch (v->value_type) {
            case I32:
                snprintf(_value_str, 255, R"("type":"i32","value":%)" PRIi32,
                         v->value.uint32);
                break;
            case I64:
                snprintf(_value_str, 255, R"("type":"i64","value":%)" PRIi64,
                         v->value.uint64);
                break;
            case F32:
                snprintf(_value_str, 255, R"("type":"i64","value":%.7f)",
                         v->value.f32);
                break;
            case F64:
                snprintf(_value_str, 255, R"("type":"i64","value":%.7f)",
                         v->value.f64);
                break;
            default:
                snprintf(_value_str, 255,
                         R"("type":"%02x","value":"%)" PRIx64 "\"",
                         v->value_type, v->value.uint64);
        }

        printf("{%s}%s", _value_str, (i + 1 < f->block->local_count) ? "," : "");
    }
    printf("]}\n\n");
    fflush(stdout);
}

/**
 * Read the change in bytes array.
 *
 * The array should be of the form
 * [0x10, index, ... new function body 0x0b]
 * Where index is the index without imports
 */
bool readChange(Module *m, uint8_t *bytes) {
    // Check if this was a change request
    if (*bytes != interruptUPDATEFun) return false;

    // SKIP the first byte (0x10), type of change
    uint8_t *pos = bytes + 1;

    uint32_t b = read_LEB_32(&pos);  // read id

    Block *function = &m->functions[m->import_count + b];
    uint32_t body_size = read_LEB_32(&pos);
    uint8_t *payload_start = pos;
    uint32_t local_count = read_LEB_32(&pos);
    uint8_t *save_pos;
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
        function->local_value_type =
                (uint8_t *) acalloc(function->local_count, sizeof(uint8_t),
                                    "function->local_value_type");
    }

    // Restore position and read the locals
    pos = save_pos;
    lidx = 0;
    for (uint32_t l = 0; l < local_count; l++) {
        lecount = read_LEB_32(&pos);
        uint8_t vt = read_LEB(&pos, 7);
        for (uint32_t i = 0; i < lecount; i++) {
            function->local_value_type[lidx++] = vt;
        }
    }

    function->start_ptr = pos;
    function->end_ptr = payload_start + body_size - 1;
    function->br_ptr = function->end_ptr;
    ASSERT(*function->end_ptr == 0x0b, "Code section did not end with 0x0b\n");
    pos = function->end_ptr + 1;
    return true;
}


/**
 * Read change to local
 * @param m
 * @param bytes
 * @return
 */
bool readChangeLocal(Module *m, uint8_t *bytes) {

    if (*bytes != interruptUPDATELocal) return false;
    uint8_t *pos = bytes + 1;
    printf("Local updates: %x\n", *pos);
    uint32_t localId = read_LEB_32(&pos);

    printf("Local %u being cahnged\n", localId);
    auto v = &m->stack[m->fp + localId];
    switch (v->value_type) {
        case I32:
            v->value.uint32 = read_LEB_signed(&pos, 32);
            break;
        case I64:
            v->value.int64 = read_LEB_signed(&pos, 64);
            break;
        case F32:
            memcpy(&v->value.uint32, pos, 4);
            break;
        case F64:
            memcpy(&v->value.uint64, pos, 8);
            break;
    }
    printf("Local %u changed to %u\n", localId,v->value.uint32);
    return true;
}

/**
 * Validate if there are interrupts and execute them
 *
 * The various kinds of interrups are preceded by an identifier:
 *
 * - `0x01` : Continue running
 * - `0x02` : Halt the execution
 * - `0x03` : Pause execution
 * - `0x04` : Execute one operaion and then pause
 * - `0x06` : Add a breakpoint, the adress is specified as a pointer.
 *            The pointer should be specified as: 06[length][pointer]
 *            eg: 06 06 55a5994fa3d6
 * - `0x07` : Remove the breakpoint at the adress specified as a pointer if it
 *            exists (see `0x06`)
 * - `0x10` : Dump information about the program
 * - `0x11` :                  show locals
 * - `0x20` : Replace the content body of a function by a new function given
 *            as payload (immediately following `0x10`), see #readChange
 */
bool check_interrupts(Module *m, RunningState *program_state) {
    uint8_t *interruptData = nullptr;
    interruptData = m->warduino->getInterrupt();
    if (interruptData) {
        printf("Interupt: %x\n", *interruptData);
        switch (*interruptData) {
            case interruptRUN:
                // printf("GO!\n");
                debug("GO!\n");
								m->warduino->printing.printo("Go!\n");
                *program_state = WARDUINOrun;
                free(interruptData);
                break;
            case interruptHALT:
								m->warduino->printing.printo("STOP!\n");
                // printf("STOP!\n");
                debug("STOP!\n");
                free(interruptData);
                exit(0);
                break;
            case interruptPAUSE:
                *program_state = WARDUINOpause;
								m->warduino->printing.printo("PAUSE!\n");
                // printf("PAUSE!\n");
                debug("PAUSE!\n");
                free(interruptData);
                break;
            case interruptSTEP:
                debug("STEP!\n");
								m->warduino->printing.printo("STEP!\n");
                // printf("STEP!\n");
                *program_state = WARDUINOstep;
                free(interruptData);
                break;
            case interruptBPAdd:  // Breakpoint
            case interruptBPRem:  // Breakpoint remove
            {
                // TODO: segfault may happen here!
                uint8_t len = interruptData[1];
                uintptr_t bp = 0x0;
                for (size_t i = 0; i < len; i++) {
                    bp <<= sizeof(uint8_t) * 8;
                    bp |= interruptData[i + 2];
                }
                auto *bpt = (uint8_t *) bp;

                if (*interruptData == 0x06) {
										// printf("ADD BP %p!\n", static_cast<void *>(bpt));
										debug("ADD BP %p!\n", static_cast<void *>(bpt));
										m->warduino->printing.printo("ADD BP %p!\n", static_cast<void *>(bpt));
                    m->warduino->addBreakpoint(bpt);
                } else {
										// printf("RMV BP %p!\n", static_cast<void *>(bpt));
										debug("RMV BP %p!\n", static_cast<void *>(bpt));
										m->warduino->printing.printo("RMV BP %p!\n", static_cast<void *>(bpt));
                    m->warduino->delBreakpoint(bpt);
                }

                free(interruptData);

                break;
            }

            case interruptDUMP:
                *program_state = WARDUINOpause;
                free(interruptData);
                doDump(m);
                break;
            case interruptDUMPLocals:
                *program_state = WARDUINOpause;
                free(interruptData);
								dump_stack_values(m);
                break;
            case interruptUPDATEFun:
                // printf("CHANGE local!\n");
                debug("CHANGE local!\n");
                readChange(m, interruptData);
                //  do not free(interruptData);
                // we need it to run that code
                // TODO: free double replacements
                break;
            case interruptUPDATELocal:
                // printf("CHANGE local!\n");
                debug("CHANGE local!\n");
                readChangeLocal(m, interruptData);
                free(interruptData);
                break;
						case interruptConnect:
						{
								debug("attempting to connect\n");
								int port = 3030;
								// TODO provide port as interruptData
								// free(interruptData);
								if(port < 0){
										debug("Invalid port nr %d", port);
								}
								m->warduino->printing.setupConnection(port);
								break;
						}
						case interruptRecvState:
						{
								if(!receivingData){
										debug("paused program execution\n");
										*program_state = WARDUINOpause;
										receivingData = true;
										freeState(m, interruptData);
										free(interruptData);
										m->warduino->printing.printo("ack!\n");
								}
								else {
										debug("receiving state\n");
										receivingData = !saveState(m, interruptData);
										free(interruptData);
										debug("sending %s!\n", receivingData ? "ack" : "done");
										m->warduino->printing.printo("%s!\n", receivingData ? "ack" : "done");
								}
								break;
						}
						default:
                // handle later
                printf("COULD not parse interrupt data!\n");
                free(interruptData);
                break;
        }
        return true;
    }
    return false;
}

