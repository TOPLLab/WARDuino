import { DebugBridge } from "./DebugBridge";
import { DebugBridgeListener } from "./DebugBridgeListener";
import { RunTimeTarget } from "./RunTimeTarget";
import { WARDuinoDebugBridgeEmulator } from "./WARDuinoDebugBridge";
import { getFileExtension } from '../Parsers/ParseUtils';

export class DebugBridgeFactory {
    static makeDebugBridge(file : string, target : RunTimeTarget, listener : DebugBridgeListener) : DebugBridge {
        let fileType = getFileExtension(file);
        switch (fileType)  {
            case "wast" :  
                switch (target)  {
                    case RunTimeTarget.Emulator:
                         return new WARDuinoDebugBridgeEmulator(file,listener);
                }
        }
        throw new Error("Unsupported file type");
    }
}