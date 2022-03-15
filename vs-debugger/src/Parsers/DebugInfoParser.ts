import {DebugBridge} from "../DebugBridges/DebugBridge";

export class DebugInfoParser {

    public parse(bridge: DebugBridge, line: any): void {
        console.log(line);

        if (line.includes("STEP")) {
            bridge.refresh();
        }

        if (line.startsWith("{")) {
            let obj = JSON.parse(line);
            bridge.setProgramCounter((parseInt(obj.pc) - parseInt(obj.start)));
            console.log(bridge.getProgramCounter().toString(16));
        }
    }

}
