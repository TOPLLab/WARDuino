import {VariableInfo} from "../CompilerBridges/VariableInfo";
import {FunctionInfo} from "../CompilerBridges/FunctionInfo";
import {Frame} from "../Parsers/Frame";

export interface DebugBridge {
    connect(): Promise<string>;
    getProgramCounter(): number;
    setProgramCounter(pc: number): void;
    getLocals(): VariableInfo[];
    setLocals(locals: VariableInfo[]): void;
    getCallstack(): Frame[];
    setCallstack(callstack: Frame[]): void;
    step(): void;
    refresh(): void;
    disconnect(): void;
}