import 'mocha';
import { WASMCompilerBridge } from '../../CompilerBridges/WASMCompilerBridge';
import { WARDuinoDebugBridgeEmulator } from '../../DebugBridges/WARDuinoDebugBridgeEmulator';

let runPath = process.cwd();
let toolChainPath      = "";
let wasmDirectoryPath  = `${runPath}/src/test/UnitTests/TestSource/fac_ok.wasm`;

suite('WARDuinoDebuggerBridge Test Suite', () => {
      
   /* test('TestEstablishConnection', async () => {
        let bridge : WARDuinoDebugBridgeEmulator = new WARDuinoDebugBridgeEmulator(wasmDirectoryPath, 
            {
				notifyError(): void {
				
                },
				connected(): void {
				
                },
				disconnected(): void{

				},
				notifyProgress(message : string): void {
                    console.log(message);
				}
			}
        );
        bridge.connect();  

    });*/

});
