# Out-of-place debugging

Aside from traditional remote debugging, the WARDuino virtual machine also supports pull-push debugging.

## Pull-based OOP Debugging

With pull debugging the current application is debugged in a local emulated WARDuino instance, but with live actuator
and sensor values from a drone device. To get current values, the emulated debugger initiates proxy calls to the drone
device. The debugger will wait until this call has completed and the result is returned by the drone.

Communication happens through a minimal byte format.

## Push-based OOP Debugging

A drone device can also push live values to the emulated debugger. These will typically be asynchronous events such as
hardware interrupts, exceptions, ...

Those are represented by `Events` in WARDuino and send by the drone device as simple json:

```json
{
  "topic": "topic string",
  "payload": "payload as a string"
}
```

The supported interrupt messages:

1. `interruptDUMPAllEvents (0x70)` dumps all events.
2. `interruptDUMPEvents (0x71)` this message' code is followed by two bytes, `a` and `b`. The command dumps at most `b`
   events, starting in the event queue from index `a`.
3. `interruptPOPEvent (0x72)` tells the VM to remove the event at the front of the queue and process it.
4. `interruptPUSHEvent (0x73)` this messages' code is followed by a json representation of an event to be pushed on the
   stack.
5. `interruptDUMPCallbackmapping (0x74)` requests a dump of the current callback mapping as json.
6. `interruptRecvCallbackmapping (0x75)` sends a callback mapping as json to replace the current callback mapping.

