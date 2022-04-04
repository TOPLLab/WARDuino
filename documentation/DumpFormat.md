# WARDuino Debugger Dump Format

WARDuino sends its information dumps as json.

## Full dump (0x12) 

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
   "pc":"0x3ffbdc00",
   "start":[
      "0x3ffbdb70"
   ],
   "breakpoints":[
      
   ],
   "functions":[
      {
         "fidx":"0x3",
         "from":"0x3ffbdbee",
         "to":"0x3ffbdbf4"
      },
      {
         "fidx":"0x4",
         "from":"0x3ffbdbf9",
         "to":"0x3ffbdc19"
      }
   ],
   "callstack":[
      {
         "type":0,
         "fidx":"0x4",
         "sp":-1,
         "fp":-1,
         "start":"0x3ffbdbf9",
         "ra":"0x3ffbdbdf",
         "callsite":"0x3ffbdbdd"
      }
   ],
   "locals":{
      "count":1,
      "locals":[
         {
            "type":"i32",
            "value":1000,
            "index":0
         }
      ]
   }
}


{
   "pc":"0x3ffbdbf0",
   "start":[
      "0x3ffbdb70"
   ],
   "breakpoints":[
      
   ],
   "functions":[
      {
         "fidx":"0x3",
         "from":"0x3ffbdbee",
         "to":"0x3ffbdbf4"
      },
      {
         "fidx":"0x4",
         "from":"0x3ffbdbf9",
         "to":"0x3ffbdc19"
      }
   ],
   "callstack":[
      {
         "type":0,
         "fidx":"0x4",
         "sp":-1,
         "fp":-1,
         "start":"0x3ffbdbf9",
         "ra":"0x3ffbdbdf",
         "callsite":"0x3ffbdbdd"
      },
      {
         "type":0,
         "fidx":"0x3",
         "sp":0,
         "fp":0,
         "start":"0x3ffbdbee",
         "ra":"0x3ffbdc00",
         "callsite":"0x3ffbdbfe"
      }
   ],
   "locals":{
      "count":0,
      "locals":[
         
      ]
   }
}
```

The WARDuino VM also supports a shorter version of the full dump (0x10), without the locals.

## Dump locals (0x11)

The locals can also be retreived on their own with the 0x11 byte.

```json
{
   "count":1,
   "locals":[
      {
         "type":"i32",
         "value":1000,
         "index":0
      }
   ]
}
```

