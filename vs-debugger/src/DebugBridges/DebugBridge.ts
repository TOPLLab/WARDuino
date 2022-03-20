import {VariableInfo} from "../CompilerBridges/VariableInfo";
import {FunctionInfo} from "../CompilerBridges/FunctionInfo";
import {Frame} from "../Parsers/Frame";

export interface DebugBridge {
    setStartAddress(startAddress : number) : void;
    connect(): Promise<string>;
    getProgramCounter(): number;
    setProgramCounter(pc: number): void;
    getLocals(): VariableInfo[];
    setLocals(locals: VariableInfo[]): void;
    getCallstack(): Frame[];
    setCallstack(callstack: Frame[]): void;
    step(): void;
    run(): void;
    pause(): void;
    setBreakPoint(x:number): void;
    refresh(): void;
    disconnect(): void;
    setVariable(name:string,value:number):void;

    upload() : void;
}