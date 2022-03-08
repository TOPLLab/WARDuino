import 'mocha';
import {WASMCompilerBridge} from '../../CompilerBridges/WASMCompilerBridge';
import {expect} from 'chai';

let runPath = process.cwd();
let wasmDirectoryPath = `${runPath}/src/test/UnitTests/TestSource`;

suite('WARDuinoCompilerBridge Test Suite', () => {

    test('TestCompileOK', async () => {
        let compilerBridge = new WASMCompilerBridge(`${wasmDirectoryPath}/fac_ok.wast`);
        let result = await compilerBridge.compile().catch((reason) => {
                expect.fail(`Unexpected exception ${reason}`);
            }
        );
        expect(result[0].lineAddress).to.equal('000002e');
        expect(result[0].lineInfo.line).to.equal(13);
    });

    test('TestCompileBridgeSyntaxError', async () => {
        let compilerBridge = new WASMCompilerBridge(`${wasmDirectoryPath}/fac_syntax_error.wast`);
        let result = await compilerBridge.compile().catch((reason) => {
                expect(reason.lineInfo.line).to.equal(1);
                expect(reason.lineInfo.column).to.equal(2);
                expect(reason.message).to.contain('error: unexpected token "modul"');
            }
        );
        expect(result).to.be.undefined;
    });


})
