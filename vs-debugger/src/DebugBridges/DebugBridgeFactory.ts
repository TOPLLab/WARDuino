import {DebugBridge} from "./DebugBridge";
import {DebugBridgeListener} from "./DebugBridgeListener";
import {RunTimeTarget} from "./RunTimeTarget";
import {WARDuinoDebugBridgeEmulator} from "./WARDuinoDebugBridgeEmulator";
import {getFileExtension} from '../Parsers/ParseUtils';
import {WARDuinoDebugBridge} from "./WARDuinoDebugBridge";

export class DebugBridgeFactory {
    static makeDebugBridge(file : string, target : RunTimeTarget, listener : DebugBridgeListener) : DebugBridge {
        let fileType = getFileExtension(file);
        switch (fileType)  {
            case "wast" :  
                switch (target)  {
                    case RunTimeTarget.emulator:
                         return new WARDuinoDebugBridgeEmulator(file,listener);
                    case RunTimeTarget.embedded:
                        return new WARDuinoDebugBridge(file,listener);
                }
        }
        throw new Error("Unsupported file type");
    }
}