import {DebugBridge} from "./DebugBridge";
import {DebugBridgeListener} from "./DebugBridgeListener";
import {ReadlineParser, SerialPort} from 'serialport';
import {DebugInfoParser} from "../Parsers/DebugInfoParser";
import {InterruptTypes} from "./InterruptTypes";
import {VariableInfo} from "../CompilerBridges/VariableInfo";
import {FunctionInfo} from "../CompilerBridges/FunctionInfo";
import {spawn, exec} from "child_process";
import {ThreadEvent} from "vscode-debugadapter";
import {resolve} from "path";
import {rejects} from "assert";
import {Frame} from "../Parsers/Frame";


export class WARDuinoDebugBridge implements DebugBridge {
    private listener: DebugBridgeListener;
    private parser: DebugInfoParser = new DebugInfoParser();
    private wasmPath: string;
    private port: SerialPort | undefined;
    private pc: number = 0;
    private locals: VariableInfo[] = [];
    private callstack: Frame[] = [];
    private portAddress: string;
    private sdk: string;


    constructor(wasmPath: string,
                listener: DebugBridgeListener,
                portAddress: string,
                warduinoSDK: string) {
        this.wasmPath = wasmPath;
        this.listener = listener;
        this.portAddress = portAddress;
        this.sdk = warduinoSDK;

        this.connect().catch(reason => {
            console.log(reason);
        });
    }

    async connect(): Promise<string> {
        let that = this;

        return new Promise(async (resolve, reject) => {
            await this.upload();

            that.port = new SerialPort({path: that.portAddress, baudRate: 115200},
                (error) => {
                    if (error) {
                        reject(`Could not connect to serial port: ${that.portAddress}`);
                    } else {
                        resolve(that.portAddress);
                    }
                }
            );

            const parser = new ReadlineParser();
            that.port?.pipe(parser);
            parser.on("data", (line: any) => {
                that.parser.parse(that, line);
            });
        });
    }

    disconnect(): void {

    }

    private uploadArduino(path: string, resolver: (value: boolean) => void): void {
        const upload = exec("make flash", {cwd: path}, (err, stdout, stderr) => {
                console.log(err);
            }
        );

        upload.on("data", (data) => {
            console.log(`stdout: ${data}`);
        });

        upload.on("close", (code) => {
            resolver(code === 0);
        });
    }

    compileArduino(path: string, resolver: (value: boolean) => void): void {
        const compile = exec("make compile", {
            cwd: path
        });

        compile.on("error", (err => {
            resolver(false);
        }));

        compile.on("close", (code) => {
            this.uploadArduino(path, resolver);
        });
    }

    upload(): Promise<boolean> {
        return new Promise<boolean>((resolve, reject) => {
            const path: string = this.sdk + "/platforms/Arduino/";
            const cp = exec("cp /tmp/warduino/upload.c " + path + "/upload.h");
            cp.on("error", err => {
                resolve(false);
            });
            cp.on("close", (code) => {
                this.compileArduino(path, resolve);
            });
        });
    }

    getProgramCounter(): number {
        return this.pc;
    }

    setProgramCounter(pc: number) {
        this.pc = pc;
    }

    private sendInterrupt(i: InterruptTypes, callback?: (error: Error | null | undefined) => void) {
        return this.port?.write(`${i} \n`, callback);
    }

    getLocals(): VariableInfo[] {
        return this.locals;
    }

    setLocals(locals: VariableInfo[]) {
        this.locals = locals;
    }

    getCallstack(): Frame[] {
        return this.callstack;
    }

    setCallstack(callstack: Frame[]): void {
        this.callstack = callstack;
    }

    step(): void {
        this.sendInterrupt(InterruptTypes.interruptSTEP, function (err: any) {
            console.log("step");
            if (err) {
                return console.log("Error on write: ", err.message);
            }
        });
    }

    refresh(): void {
        this.sendInterrupt(InterruptTypes.interruptDUMPFull, function (err: any) {
            if (err) {
                return console.log("Error on write: ", err.message);
            }
            console.log("dbg");
        });
    }
}