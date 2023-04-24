#pragma once

enum RunningState {
    WARDUINOrun,
    WARDUINOpause,
    WARDUINOstep,
    PROXYrun,  // Running state used when executing a proxy call. During
    // this state the call is set up and executed by the main
    // loop.
};

enum ProxyMode { ProxyNotUsed = 0x01, ProxyRedirect = 0x02, ProxyCopy = 0x03 };

enum InterruptTypes {
    // Remote Debugging
    interruptRUN = 0x01,
    interruptHALT = 0x02,
    interruptPAUSE = 0x03,
    interruptSTEP = 0x04,
    interruptBPAdd = 0x06,
    interruptBPRem = 0x07,
    interruptDumpExecutionState = 0x09,
    interruptDUMP = 0x10,
    interruptDUMPLocals = 0x11,
    interruptDUMPFull = 0x12,
    interruptUPDATEStackValue = 0x14,
    interruptUPDATEGlobalValue = 0x15,
    interruptUPDATEFun = 0x20,
    interruptUPDATELocal = 0x21,
    interruptUPDATEModule = 0x22,
    // Remote REPL
    interruptINVOKE = 0x40,

    // Pull Debugging
    interruptWOODDUMP = 0x60,
    interruptRecvState = 0x62,
    interruptMonitorProxies = 0x63,
    interruptProxyCall = 0x64,
    interruptProxify = 0x65,  // wifi SSID \0 wifi PASS \0

    // Push Debugging
    interruptDUMPAllEvents = 0x70,
    interruptDUMPEvents = 0x71,
    interruptPOPEvent = 0x72,
    interruptPUSHEvent = 0x73,
    interruptDUMPCallbackmapping = 0x74,
    interruptRecvCallbackmapping = 0x75
};

enum ExecutionState {
    pcState = 0x01,
    breakpointsState = 0x02,
    callstackState = 0x03,
    globalsState = 0x04,
    tableState = 0x05,
    memState = 0x06,
    branchingTableState = 0x07,
    stackState = 0x08,
    errorState = 0x09,
    callbacksState = 0x0a,
    eventsState = 0x0b
};