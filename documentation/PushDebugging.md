# Pull-Push Debugging

Aside from traditional remote debugging, the WARDuino virtual machine also supports pull-push debugging.

## Pull Debugging

With pull debugging the current application is debugged in a local emulated WARDuino instance, but with live actuator and sensor values from a drone device.
To get current values, the emulated debugger initiates proxy calls to the drone device.
The debugger will wait until this call has completed and the result is returned by the drone.

Communication happens through a minimal byte format.

## Push Debugging

A drone device can also push live values to the emulated debugger.
These will typically be asynchronous events such as hardware interrupts, exceptions, ...

Those are represented by `Events` in WARDuino and send by the drone device as simple json:

```json
{
   "topic":"topic string",
   "payload":"payload as a string"
}
```

