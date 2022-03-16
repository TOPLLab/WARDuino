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

        if (line.startsWith("{")) {
            let obj = JSON.parse(line);
            this.addressBeginning = parseInt(obj.start);
            bridge.setProgramCounter((parseInt(obj.pc) - this.addressBeginning));
            bridge.setStartAddress(this.addressBeginning);
            bridge.setLocals(DebugInfoParser.parseLocals(obj.locals.locals));
            bridge.setCallstack(this.parseCallstack(obj.callstack));
            console.log(bridge.getProgramCounter().toString(16));
        }
    }

    private static parseLocals(objs: any[]): VariableInfo[] {
        let locals: VariableInfo[] = [];
        objs.forEach((obj) => {
            locals.push({index: obj.index, name: obj.name, type: obj.type, value: obj.value, mutable: true});
        });
        console.log(locals);
        return locals;
    }

    private parseCallstack(objs: any[]): Frame[] {
        let functions: Frame[] = [];
        objs.filter((obj) => {
            return obj.type === 0;
        }).forEach((obj) => {
            functions.push({index: parseInt(obj.fidx), returnAddress: parseInt(obj.ra) - this.addressBeginning});
        });
        return functions;
    }
}
