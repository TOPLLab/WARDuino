import {DebugBridge} from "../DebugBridges/DebugBridge";
import {VariableInfo} from "../CompilerBridges/VariableInfo";
import {FunctionInfo} from "../CompilerBridges/FunctionInfo";

export class DebugInfoParser {

    public parse(bridge: DebugBridge, line: any): void {
        console.log(line);

        if (line.includes("STEP")) {
            bridge.refresh();
        }

        if (line.startsWith("{")) {
            let obj = JSON.parse(line);
            bridge.setProgramCounter((parseInt(obj.pc) - parseInt(obj.start)));
            bridge.setLocals(DebugInfoParser.parseLocals(obj.locals.locals));
            bridge.setCurrentFunctionIndex(DebugInfoParser.findCurrentFunctionIndex(obj.callstack));
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

    private static findCurrentFunctionIndex(callstack: any[]): number {
        let i = callstack.length - 1;
        let obj;
        while (i >= 0) {
            obj = callstack[i--];
            if (obj.type === 0) {
                return parseInt(obj.fidx);
            }
        }
        return -1;
    }

}
