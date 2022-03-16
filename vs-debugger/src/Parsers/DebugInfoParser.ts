import {DebugBridge} from "../DebugBridges/DebugBridge";
import {VariableInfo} from "../CompilerBridges/VariableInfo";

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
            bridge.setCallstack(DebugInfoParser.parseCallstack(obj.callstack));
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

    private static parseCallstack(objs: any[]): number[] {
        let functions: number[] = [];
        objs.filter((obj) => {
            return obj.type === 0;
        }).forEach((obj) => {
            functions.push(parseInt(obj.fidx));
        });
        return functions;
    }

}
