import 'mocha';
import {WARDuinoDebugBridge} from '../../DebugBridges/WARDuinoDebugBridge';
import exp = require("constants");
import {expect} from "chai";

const runPath = process.cwd();

const port = "/dev/cu.usbserial-0001";
const warduinoSDK = "/Users/xtofs/Documents/Arduino/libraries/WARDuino";
const wasmDirectoryPath = `${runPath}/src/test/UnitTests/TestSource/fac_ok.wasm`;

suite('Hardware-less Test Suite', () => {
    test('TestEstablishConnectionFailure', async () => {
        let bridge: WARDuinoDebugBridge = new WARDuinoDebugBridge(wasmDirectoryPath,
            {
                notifyError(): void {

                },
                connected(): void {

                },
                notifyPaused(): void 
                {
                    
                },
                disconnected(): void {

                },
                notifyProgress(message: string): void {
                    console.log(message);
                },
                notifyStateUpdate() {
                }
            },
            port,
            warduinoSDK
        );

        bridge.compileAndUpload().catch(reason => {
            expect(reason.to.equal(`Could not connect to serial port: ${port}`));
        });
    });

    test('TestWrongPath', async () => {

        let bridge: WARDuinoDebugBridge = new WARDuinoDebugBridge("",
            {
                notifyError(): void {

                },
                connected(): void {

                }, 
                notifyPaused() :void {

                },
                disconnected(): void {

                },
                notifyProgress(message: string): void {
                    console.log(message);
                },
                notifyStateUpdate() {
                }
            },
            port,
            warduinoSDK
        );


    });

    test('TestUpload', async () => {
        let bridge: WARDuinoDebugBridge = new WARDuinoDebugBridge(wasmDirectoryPath,
            {
                notifyError(): void {

                },
                connected(): void {

                },
                disconnected(): void {

                },
                notifyPaused() :void {

                },
                notifyProgress(message: string): void {
                    console.log(message);
                },
                notifyStateUpdate() {
                }
            },
            port,
            warduinoSDK
        );

        let result = await bridge.compileAndUpload();
        expect(result).to.be.true;
    });


});

suite('Hardware Test Suite', () => {
});