import {SourceMap} from "./SourceMap";

export interface CompileBridge {
    compile(): Promise<SourceMap>;
}