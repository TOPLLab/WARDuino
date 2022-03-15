import {DebugBridge} from "./DebugBridge";
import {DebugBridgeListener} from "./DebugBridgeListener";
import {ReadlineParser, SerialPort} from 'serialport';
import {DebugInfoParser} from "../Parsers/DebugInfoParser";
import {InterruptTypes} from "./InterruptTypes";


export class WARDuinoDebugBridge implements DebugBridge {
    private listener: DebugBridgeListener;
    private parser: DebugInfoParser;
    private wasmPath: string;
    private port: SerialPort | undefined;
    private pc: number = 0;
    private portAddress: string;


    constructor(wasmPath: string, listener: DebugBridgeListener, portAddress: string) {
        this.wasmPath = wasmPath;
        this.listener = listener;
        this.parser = new DebugInfoParser();
        this.portAddress = portAddress;
        this.connect().catch(reason => {
            console.log(reason);
        });
    }

    connect(): Promise<string> {
        let that = this;

        return new Promise((resolve, reject) => {
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
            parser.on('data', (line: any) => {
                that.parser.parse(that, line);
            });
        });
    }

    disconnect(): void {

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

    step(): void {
        this.sendInterrupt(InterruptTypes.interruptSTEP, function (err: any) {
            console.log("step");
            if (err) {
                return console.log('Error on write: ', err.message);
            }
        });
    }

    refresh(): void {
        this.sendInterrupt(InterruptTypes.interruptDUMPFull, function (err: any) {
            if (err) {
                return console.log('Error on write: ', err.message);
            }
            console.log('dbg');
        });
    }
}