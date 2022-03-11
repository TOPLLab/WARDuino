import {VariableInfo} from "./VariableInfo";

export interface FunctionInfo {
    index: number;
    name: string;
    locals: VariableInfo[];
}