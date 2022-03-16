import 'mocha';
import {WARDuinoDebugBridge} from '../../DebugBridges/WARDuinoDebugBridge';
import exp = require("constants");
import {expect} from "chai";

const runPath = process.cwd();
const port = "/dev/ttyUSB0";
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
                disconnected(): void {

                },
                notifyProgress(message: string): void {
                    console.log(message);
                }
            },
            port,
            warduinoSDK
        );

        bridge.upload().catch(reason => {
            expect(reason.to.equal(`Could not connect to serial port: ${port}`));
        });
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
                notifyProgress(message: string): void {
                    console.log(message);
                }
            },
            port,
            warduinoSDK
        );

        bridge.upload().then(value => {
            expect(value).to.be.true;
        });
    });
});

suite('Hardware Test Suite', () => {
});