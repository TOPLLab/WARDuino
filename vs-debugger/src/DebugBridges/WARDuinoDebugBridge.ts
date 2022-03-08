import {DebugBridge} from "./DebugBridge";
import {DebugBridgeListener} from "./DebugBridgeListener";
import * as net from 'net';
import {ChildProcess} from "child_process";

export class WARDuinoDebugBridge implements DebugBridge {

    private client?: net.Socket;
    private wasmPath: string;
    private cp?: ChildProcess;
    private listener: DebugBridgeListener;
    private pc: number = 0;

    constructor(wasmPath: string, listener: DebugBridgeListener) {
        this.wasmPath = wasmPath;
        this.listener = listener;
        this.connect();
    }

    connect(): void {
    }

    disconnect(): void {
    }

    getProgramCounter(): number {
        return 0;
    }

    step(): void {
    }
}