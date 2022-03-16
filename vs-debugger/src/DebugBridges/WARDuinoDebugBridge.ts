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

class Messages {
    public static UPLOADING : string = "Uploading to board";
    public static CONNECTING : string = "Connecting to board";
    public static CONNECTED : string = "Connected to board";
    public static DISCONNECTED : string = "Disconnected board";

    static COMPILING: string = "Compiling the code";
    public static COMPILED : string = "Compiled Code";
    static RESET: string = "Press reset button";
}


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
        return new Promise(async (resolve, reject) => {
            this.listener.notifyProgress(Messages.COMPILING);
            await this.compileAndUpload();
            this.listener.notifyProgress(Messages.CONNECTING);
            this.openSerialPort(reject, resolve);
            this.installInputStreamListener();
        });
    }

    private openSerialPort(reject: (reason?: any) => void, resolve: (value: string | PromiseLike<string>) => void) {
        this.port = new SerialPort({ path: this.portAddress, baudRate: 115200 },
            (error) => {
                if (error) {
                    reject(`Could not connect to serial port: ${this.portAddress}`);
                } else {
                    this.listener.notifyProgress(Messages.CONNECTED);
                    resolve(this.portAddress);
                }
            }
        );
    }

    private installInputStreamListener() {
        const parser = new ReadlineParser();
        this.port?.pipe(parser);
        parser.on("data", (line: any) => {
            this.parser.parse(this, line);
        });
    }

    public disconnect(): void {
        this.port?.close();
        this.listener.notifyProgress(Messages.DISCONNECTED);
    }

    private uploadArduino(path: string, resolver: (value: boolean) => void): void {
        this.listener.notifyProgress(Messages.RESET);

        const upload = exec(`sh upload ${this.portAddress}`, {cwd: path}, (err, stdout, stderr) => {
                console.log(err);
                console.log(stdout);
            }
        );

        upload.on("data", (data:string) => {
            console.log(`stdout: ${data}`);
            if( data.search('Uploading') ) {
                this.listener.notifyProgress(Messages.UPLOADING);
            }
        });

        upload.on("close", (code) => {
            resolver(code === 0);
        });
    }

    public compileArduino(path: string, resolver: (value: boolean) => void): void {
        const compile = exec("make compile", {
            cwd: path
        });

        compile.on("error", (err => {
            resolver(false);
        }));

        compile.on("close", (code) => {
            if(code === 0){ 
                this.listener.notifyProgress(Messages.COMPILED);
                this.uploadArduino(path, resolver);
            } 
        });
    }

    public compileAndUpload(): Promise<boolean> {
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