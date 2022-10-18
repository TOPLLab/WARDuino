# Interrupts

Warduino reacts to interrups sent via the serial port. To hook onto these changes, 
we need to connect the RX pin to the D1 pin. We then set an interrupt handler on
chnages of this pin.

The interrupt handler, reads the input and passes it on to WARDuino trough the 
`handleInterrupt()` method. This method read the incomming data as HEX and 
transformes it into binary data (`uint8_t[]`). A HEX sequence may be ended with
any non-hex character (`[^0-9A-F]`). A HEX sequence must not contain a non-hex 
character, it should match `([0-9A-F][0-9A-F])+`

The first two character of the HEX sequence (that is the first byte of the 
translated binary data) differentiates between the various interrup types.

See: [src/Debug/Debugger.cpp](../src/Debug/Debugger.cpp)
