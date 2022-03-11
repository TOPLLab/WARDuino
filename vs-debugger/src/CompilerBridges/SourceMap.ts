import {LineInfoPairs} from "./LineInfoPairs";
import {FunctionInfo} from "./FunctionInfo";
import {VariableInfo} from "./VariableInfo";

export interface SourceMap {
    lineInfoPairs: LineInfoPairs[];
    functionInfos: FunctionInfo[];
    globalInfos: VariableInfo[];
}