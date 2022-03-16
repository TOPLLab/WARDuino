import {VariableInfo} from "../CompilerBridges/VariableInfo";
import {FunctionInfo} from "../CompilerBridges/FunctionInfo";

export interface DebugBridge {
    connect(): Promise<string>;
    getProgramCounter(): number;
    setProgramCounter(pc: number): void;
    getLocals(): VariableInfo[];
    setLocals(locals: VariableInfo[]): void;
    getCallstack(): number[];
    setCallstack(callstack: number[]): void;
    step(): void;
    refresh(): void;
    disconnect(): void;
}