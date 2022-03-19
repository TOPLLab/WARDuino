import {DebugBridge} from "../DebugBridges/DebugBridge";
import {VariableInfo} from "../CompilerBridges/VariableInfo";
import {Frame} from "./Frame";

export class DebugInfoParser {

    private addressBeginning: number = 0;

    public parse(bridge: DebugBridge, line: any): void {
        console.log(line);

        if (line.includes("STEP")) {
            bridge.refresh();
        }

        if(line.includes("AT")) {
            let breakpointInfo = line.match(/AT (0x.*)!/);
            if(breakpointInfo.length > 1) {
                let pc = parseInt(breakpointInfo[1]);
                bridge.setProgramCounter(pc);
                bridge.pause();
            }
        }

        if (line.startsWith("{")) {
            let obj = JSON.parse(line);
            this.addressBeginning = parseInt(obj.start);
            bridge.setProgramCounter((parseInt(obj.pc) - this.addressBeginning));
            bridge.setStartAddress(this.addressBeginning);
            bridge.setCallstack(this.parseCallstack(obj.callstack));
            let fidx = bridge.getCurrentFunctionIndex();
            bridge.setLocals(fidx, this.parseLocals(bridge, obj.locals.locals));
            console.log(bridge.getProgramCounter().toString(16));
        }
    }

    private parseLocals(bridge: DebugBridge, objs: any[]): VariableInfo[] {
        let fidx = bridge.getCurrentFunctionIndex();
        let locals: VariableInfo[] = bridge.getLocals(fidx);
        objs.forEach((obj) => {
            let local = locals[obj.index];
            if (local) {
                local.type = obj.type;
                local.value = obj.value;
            }
        });
        console.log(locals);
        return locals;
    }

    private parseCallstack(objs: any[]): Frame[] {
        let functions: Frame[] = [];
        objs.filter((obj) => {
            return obj.type === 0;
        }).forEach((obj) => {
            functions.push({index: parseInt(obj.fidx), returnAddress: parseInt(obj.callsite) - this.addressBeginning});
        });
        return functions;
    }
}
