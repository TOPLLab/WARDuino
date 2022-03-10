# WARDuino Debugger Dump Format

WARDuino sends its information dumps as json.

## Full dump (0x10) 

Lists the following items:

- program counter (pointer to instr in program buffer)
- start of program buffer (pointer)
- breakpoints (list of pointers to instrs in program buffer)
- functions (all declared functions)
- callstack (the current callstack, bottom to top)

### Callstack

The callstack is printed as a list of objects, with the first object the bottom of the stack.
Each object represents a block with a type:

- Function 0x00
- Init expression 0x01
- Block 0x02
- Loop 0x03
- If 0X04

When the block is a function it also holds the function index in the `fidx` field.

### Example output

```json
{
   "pc":"0x55affcc8e71d",
   "start":[
      "0x55affcc8e6b0"
   ],
   "breakpoints":[
      
   ],
   "functions":[ // All declared functions
      {
         "fidx":"0x0",
         "from":"0x55affcc8e718",
         "to":"0x55affcc8e718"
      },
      {
         "fidx":"0x1",
         "from":"0x55affcc8e71b",
         "to":"0x55affcc8e752"
      },
      {
         "fidx":"0x2",
         "from":"0x55affcc8e757",
         "to":"0x55affcc8e768"
      }
   ],
   "callstack":[
      {  // Bottom of stack
         "type":0,                  // block type 0 = FUNCTION
         "fidx":"0x2",              // index of function
         "sp":-1,                   // stack pointer before function call
         "fp":-1,                   // function pointer before function call
         "start":"0x55affcc8e757",  // pointer to start of program buffer
         "ra":"0x55affcc8e70f"      // pointer to return address
      },
      {
         "type":3,          // block type 3 = LOOP
         "fidx":"0x0",
         "sp":0,
         "fp":0,
         "start":"0x55affcc8e75b",
         "ra":"0x55affcc8e75d"
      },
      {  // Top of stack
         "type":0,          // block type 0 = FUNCTION
         "fidx":"0x1",
         "sp":0,
         "fp":0,
         "start":"0x55affcc8e71b",
         "ra":"0x55affcc8e763"
      }
   ]
}
```

## Dump locals (0x11)

