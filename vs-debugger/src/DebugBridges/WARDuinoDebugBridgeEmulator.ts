import {ChildProcess, spawn} from 'child_process';
import * as net from 'net';
import {DebugBridge} from './DebugBridge';
import {DebugBridgeListener} from './DebugBridgeListener';
import {InterruptTypes} from './InterruptTypes';
import {DebugInfoParser} from "../Parsers/DebugInfoParser";
import {VariableInfo} from "../CompilerBridges/VariableInfo";
import {Frame} from "../Parsers/Frame";
import {SourceMap} from "../CompilerBridges/SourceMap";

export class WARDuinoDebugBridgeEmulator implements DebugBridge {

    private client?: net.Socket;
    private wasmPath: string;
    private sourceMap: SourceMap | void;
    private cp?: ChildProcess;
    private listener: DebugBridgeListener;
    private parser: DebugInfoParser;
    private pc: number = 0;
    private locals: VariableInfo[] = [];
    private callstack: Frame[] = [];
    private startAddress: number = 0;

    constructor(wasmPath: string, sourceMap: SourceMap | void, listener: DebugBridgeListener) {
        this.wasmPath = wasmPath;
        this.sourceMap = sourceMap;
        this.listener = listener;
        this.parser = new DebugInfoParser();
        this.connect();
    }

    upload(): void {
        throw new Error('Method not implemented.');
    }

    setVariable(name: string, value: number): Promise<string> {
        console.log(`setting ${name} ${value}`);
        return new Promise<string>(resolve => resolve("Variable set."));
    }

    pause(): void {
        throw new Error('Method not implemented.');
    }

    setStartAddress(startAddress: number) {
        this.startAddress = startAddress;
    }

    run(): void {
        this.sendInterrupt(InterruptTypes.interruptRUN);
    }

    setBreakPoint(x: number): void {
        console.log(this.startAddress);
        throw new Error('Method not implemented.');
    }

    public connect(): Promise<string> {
        return new Promise((resolve, reject) => {
            this.startEmulator();
            resolve("127.0.0.1:8192");
        });
    }

    public getProgramCounter(): number {
        return this.pc;
    }

    setProgramCounter(pc: number) {
        this.pc = pc;
    }

    getLocals(fidx: number): VariableInfo[] {
        if (this.sourceMap === undefined || fidx >= this.sourceMap.functionInfos.length || fidx < 0) {
            return [];
        }
        return this.sourceMap.functionInfos[fidx].locals;
    }

    setLocals(fidx: number, locals: VariableInfo[]) {
        if (this.sourceMap === undefined) {
            return;
        }
        if (fidx >= this.sourceMap.functionInfos.length) {
            console.log(`warning setting locals for new function with index: ${fidx}`);
            this.sourceMap.functionInfos[fidx] = {index: fidx, name: "<anonymous>", locals: []};
        }
        this.sourceMap.functionInfos[fidx].locals = locals;
    }

    getCallstack(): Frame[] {
        return this.callstack;
    }

    setCallstack(callstack: Frame[]): void {
        this.callstack = callstack;
        this.listener.notifyStateUpdate();
    }

    getCurrentFunctionIndex(): number {
        if (this.callstack.length === 0) {
            return -1;
        }
        return this.callstack[this.callstack.length - 1].index;
    }

    private initClient() {
        let that = this;
        if (this.client === undefined) {
            this.client = new net.Socket();
            this.client.connect({port: 8192, host: '127.0.0.1'});  // TODO config
            this.listener.notifyProgress('Connected socket');

            this.client.on('error', err => {
                    this.listener.notifyError('Lost connection to the board');
                    console.log(err);
                }
            );

            this.client.on('data', data => {
                    console.log(`data: ${data}`);
                    that.parser.parse(that, data.toString());
                }
            );
        }
    }

    private sendInterrupt(i: InterruptTypes) {
        let command = `${i} \n`;
        this.client?.write(command);
    }

    public step() {
        this.sendInterrupt(InterruptTypes.interruptSTEP);
    }

    public refresh() {
        this.sendInterrupt(InterruptTypes.interruptDUMPFull);
    }

    private executeCommand(command: InterruptTypes) {
        console.log(command.toString());
        this.client?.write(command.toString + '\n');
    }

    private startEmulator() {
        this.cp = WARDuinoDebugBridgeEmulator.spawnEmulatorProcess();

        this.listener.notifyProgress('Started Emulator');
        while (this.cp.stdout === undefined) {
        }

        this.initClient();

        this.cp.stdout?.on('data', (data) => {  // Print debug and trace information
            console.log(`stdout: ${data}`);
        });

        this.cp.stderr?.on('data', (data) => {
            console.error(`stderr: ${data}`);
        });

        this.cp.on('error', (err) => {
            console.error('Failed to start subprocess.');
        });

        this.cp.on('close', (code) => {
            console.log('Something went wrong with the emulator stream');
            this.listener.notifyProgress('Disconnected from emulator');
        });

    }

    public disconnect(): void {
        this.cp?.kill();
        this.client?.destroy();
    }

    private static spawnEmulatorProcess(): ChildProcess {
        // TODO no absolute path. package extension with upload.wasm and compile warduino during installation.
        return spawn('/home/tolauwae/Arduino/libraries/WARDuino/vs-debugger/wdcli', ['--file', '/tmp/warduino/upload.wasm']);
    }

}