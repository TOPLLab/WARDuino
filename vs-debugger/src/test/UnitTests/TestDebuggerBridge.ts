import 'mocha';
import { WASMCompilerBridge } from '../../CompilerBridges/WASMCompilerBridge';
import { WARDuinoDebugBridge } from '../../DebugBridges/WARDuinoDebugBridge';

let runPath = process.cwd();
let toolChainPath      = "";
let wasmDirectoryPath  = `${runPath}/src/test/UnitTests/TestSource/fac_ok.wasm`;

suite('WARDuinoDebuggerBridge Test Suite', () => {
      
    test('TestEstablishConnection', async () => {
        let bridge : WARDuinoDebugBridge = new WARDuinoDebugBridge(wasmDirectoryPath, 
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

    });

});
