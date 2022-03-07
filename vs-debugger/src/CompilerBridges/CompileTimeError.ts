import { LineInfo } from "./LineInfo";


export class CompileTimeError extends Error {
    lineInfo : LineInfo;

    constructor(message: string, info: LineInfo) {
        super(message);
        this.lineInfo = info;
    }

}
