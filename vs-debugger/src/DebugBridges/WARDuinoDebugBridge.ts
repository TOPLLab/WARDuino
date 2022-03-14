import {DebugBridge} from "./DebugBridge";
import {DebugBridgeListener} from "./DebugBridgeListener";
import * as net from 'net';
import {ChildProcess} from "child_process";
import { SerialPort, ReadlineParser } from 'serialport'
import { Console } from "console";



export class WARDuinoDebugBridge implements DebugBridge {

    private listener: DebugBridgeListener;
    private wasmPath: string;
    private port: any;
    private pc: number = 0;


    constructor(wasmPath: string, listener: DebugBridgeListener) {
        this.wasmPath = wasmPath;
        this.listener = listener;
        this.connect();
    }

    compile() {
        this.wasmPath; 
        this.connect();
    }

    connect(): void {
        this.listener.notifyProgress('Started Emulator');
        this.port = new SerialPort({ path:  '/dev/cu.usbserial-1430', baudRate: 115200  }, 
        (error) => {
            if(error) {
                console.log('fatal' + error);
            }
        }
        );

        const parser = new ReadlineParser()
		this.port.pipe(parser)
		parser.on('data', (line:any) => {
            console.log(line);

				if (line.startsWith("{")) {
					var obj = JSON.parse(line);
					this.pc = (parseInt(obj.pc) - parseInt(obj.start));
					console.log(this.pc.toString(16));
				}
		});
   

    }

    disconnect(): void {

    }

    getProgramCounter(): number {
        return this.pc;
    }

    step(): void {
        this.port.write('04 \n\r', function(err:any) {
			if (err) {
			  return console.log('Error on write: ', err.message)
			}
			console.log('step')
		  });
          var that = this;

          setTimeout(function() {
            that.port.write('10 \r', function(err:any) {
                if (err) {
                  return console.log('Error on write: ', err.message)
                }
                console.log('dbg')
            })
        } , 500);  

    }
}