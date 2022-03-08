import {getFileExtension} from "../Parsers/ParseUtils";
import {CompileBridge} from "./CompileBridge";
import {WASMCompilerBridge} from "./WASMCompilerBridge";

export class CompileBridgeFactory {
    static makeCompileBridge(file: string): CompileBridge {
        let fileType = getFileExtension(file);
        switch (fileType) {
            case "wast" :
                return new WASMCompilerBridge(file);
            // TODO add Rust
        }
        throw new Error("Unsupported file type");
    }
}