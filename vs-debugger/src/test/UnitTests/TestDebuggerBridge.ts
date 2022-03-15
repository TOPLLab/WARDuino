import 'mocha';
import {WARDuinoDebugBridge} from '../../DebugBridges/WARDuinoDebugBridge';
import exp = require("constants");
import {expect} from "chai";

let runPath = process.cwd();
let toolChainPath = "";
let wasmDirectoryPath = `${runPath}/src/test/UnitTests/TestSource/fac_ok.wasm`;

suite('WARDuinoDebuggerBridge Test Suite', () => {

    test('TestEstablishConnection', async () => {
        let bridge: WARDuinoDebugBridge = new WARDuinoDebugBridge(wasmDirectoryPath,
            {
                notifyError(): void {

                },
                connected(): void {

                },
                disconnected(): void {

                },
                notifyProgress(message: string): void {
                    console.log(message);
                }
            }, 
            '/dev/ttyUSB0', 
            '/Users/xtofs/Documents/Arduino/libraries/WARDuino'
        );

        bridge.upload();

        //await bridge.connect().then((data) => {
        //    expect(data).to.equal('/dev/ttyUSB0');
        //});
    });

});
