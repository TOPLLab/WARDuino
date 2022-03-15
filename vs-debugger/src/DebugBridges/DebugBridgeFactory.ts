import {DebugBridge} from "./DebugBridge";
import {DebugBridgeListener} from "./DebugBridgeListener";
import {RunTimeTarget} from "./RunTimeTarget";
import {WARDuinoDebugBridgeEmulator} from "./WARDuinoDebugBridgeEmulator";
import {getFileExtension} from '../Parsers/ParseUtils';
import {WARDuinoDebugBridge} from "./WARDuinoDebugBridge";
import * as vscode from "vscode";


export class DebugBridgeFactory {
    static makeDebugBridge(file : string, target : RunTimeTarget, listener : DebugBridgeListener) : DebugBridge {
        let fileType = getFileExtension(file);
        switch (fileType)  {
            case "wast" :  
                switch (target)  {
                    case RunTimeTarget.emulator:
                         return new WARDuinoDebugBridgeEmulator(file,listener);
                    case RunTimeTarget.embedded:
                        let portAddress : string | undefined = vscode.workspace.getConfiguration().get("warduino.Port");
                        if (portAddress === undefined) {
                            throw new Error('Configuration error. No port address set.');
                        }
                        return new WARDuinoDebugBridge(file,listener, portAddress);
                }
        }
        throw new Error("Unsupported file type");
    }
}