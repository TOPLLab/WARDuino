import "mocha";
import {WARDuinoDebugBridge} from "../../DebugBridges/WARDuinoDebugBridge";
import {expect} from "chai";
import * as fs from "fs";
import * as os from "os";
import * as path from "path";
import {before, beforeEach} from "mocha";
import ErrnoException = NodeJS.ErrnoException;

const runPath = process.cwd();

const port = "port does not exist";
const warduinoSDK = "$HOME/Arduino/libraries/WARDuino";
const wasmDirectoryPath = `${runPath}/src/test/UnitTests/TestSource/fac_ok.wasm`;

suite('Hardware-less Test Suite', () => {
    let tmpdir: string = "";
    let bridge: WARDuinoDebugBridge;

    before(async function () {
        await new Promise(resolve => {
            fs.mkdtemp(path.join(os.tmpdir(), 'warduino.'), (err: ErrnoException | null, dir: string) => {
                if (err === null) {
                    tmpdir = dir;
                    resolve(null);
                }
            });
        });
    });

    beforeEach(async function () {
        bridge = new WARDuinoDebugBridge("",
            undefined,
            tmpdir,
            {
                notifyError(): void {

                },
                connected(): void {

                },
                notifyPaused(): void {

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

    test('TestEstablishConnectionFailure', async () => {
        bridge.compileAndUpload().catch(reason => {
            expect(reason.to.equal(`Could not connect to serial port: ${port}`));
        });
    });

    test('TestNoLocalDevice', async () => {
        let result = await bridge.compileAndUpload();
        expect(result).to.be.false;
    });

});

suite('Hardware Test Suite', () => {
});