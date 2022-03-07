import  { exec, ExecException } from "child_process";
import * as parseUtils from "../Parsers/ParseUtils";
import { CompileTimeError } from "./CompileTimeError";
import { LineInfo } from "./LineInfo";
import { LineInfoPairs } from "./LineInfoPairs";

function checkCompileTimeError(errorMessage: string) {
    let regexpr = /:(?<line>(\d+)):(?<column>(\d+)): error: (?<message>(.*))/;
    let result  = errorMessage.match(regexpr);    
    if(result?.groups) {
        throw new CompileTimeError(errorMessage,
            { line    : parseInt(result.groups.line),
              column  : parseInt(result.groups.column),
              message : result.groups.message }
        );
    }
}

function checkErrorWat2Wasm(errorMessage : string) {
    if(errorMessage.match('wat2wasm')){
        throw new Error('Could not find wat2wasm in the path')
    }
}

function  extractLineInfo(lineString : string) : LineInfo {
    lineString = lineString.substring(1);
    return parseUtils.jsonParse(lineString);
}

function createLineInfoPairs(lines : string[]) : LineInfoPairs[] {
    let result = [];
    for(let i=0;i<lines.length;i++) {
        if(lines[i].match(/@/)){ 
            result.push({ lineInfo: extractLineInfo(lines[i]), 
                          lineAddress: parseUtils.extractAddressInformation(lines[i+1])});
        }
    }
    return result;
}

function makeSourceMap(sourceMapInput : String) :  LineInfoPairs[] {
    let lines = sourceMapInput.split('\n');
    let lineInfoPairs = createLineInfoPairs(lines); 
    return lineInfoPairs;
}

 
export class WASMCompilerBridge {
    wasmFilePath : String 
    runTimePath  : String 
    
    constructor(runtimePath : String, wasmFilePath : String) {
        this.wasmFilePath = wasmFilePath;
        this.runTimePath = runtimePath;
    }

    async CompileWast() {
        let compileCommand : string = this.compileToWasmCommand();
        return await this.executeCommand(compileCommand);
    }

    async compileHeader() {
        let compileCHeader : string = this.compileCHeaderFileCommand();
        return await this.executeCommand(compileCHeader);
    }

    private checkErrorMessage(errorString : string) {
        checkErrorWat2Wasm(errorString);
        checkCompileTimeError(errorString);
    }

    private handleStdError(stderr: string, reject: (x:any) => void) {
        if(stderr) {
            try {
                this.checkErrorMessage(stderr);
            } catch (e: any) {
                reject(e);
            }
        } 
    }

    private  handleError(error: ExecException|null, reject: (x:any)=> void) {
        if (error) {
            reject(error.message)
        }
    }

    private  executeCommand(command : string) : Promise<LineInfoPairs[]> {
        return new Promise( (resolve,reject) => {
            let sourceMap: LineInfoPairs[];
            let that = this;
            
            function handleCompilerStreams(error: ExecException|null, stdout: String, stderr: any) {
                that.handleStdError(stderr,reject);
                that.handleError(error,reject);
                sourceMap = makeSourceMap(stdout);  
            }
            
            let cp = exec(command, handleCompilerStreams); 

            cp.on('close', (code) => {
                if(sourceMap) {
                    resolve(sourceMap);
                }
            });
         })
    }

    private compileToWasmCommand(): string {
        return `wat2wasm -v -o /tmp/warduino/upload.wasm ` + this.wasmFilePath; 
    }

    private compileCHeaderFileCommand(): string {
        return `xxd -i upload.wasm > /tmp/warduino/upload.c`;  
    }

}