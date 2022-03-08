import {LineInfoPairs} from "./LineInfoPairs";

export interface CompileBridge {
    compile(): Promise<LineInfoPairs[]>;
}