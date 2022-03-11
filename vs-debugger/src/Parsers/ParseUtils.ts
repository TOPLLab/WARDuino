import {FunctionInfo} from "../CompilerBridges/FunctionInfo";
import {VariableInfo} from "../CompilerBridges/VariableInfo";

export function jsonParse(obj: string) {
    return new Function(`return ${obj}`)();
}

export function extractAddressInformation(addressLine: string): string {
    let regexpr = /^(?<address>([\da-f])+):/;
    let match = addressLine.match(regexpr);
    if (match?.groups) {
        return match.groups.address;
    }
    throw Error(`Could not parse address from line: ${addressLine}`);
}

export function getFileExtension(file: string): string {
    let splitted = file.split('.');
    if (splitted.length === 2) {
        return splitted.pop()!;
    }
    throw Error("Could not determine file type");
}

function extractDetailedSection(section: string, input: String): String[] {
    let lines = input.split('\n');
    let i = 0;
    while (i < lines.length && !lines[i].startsWith(section)) {
        i++;
    }

    if (i >= lines.length) {
        return [];
    }

    let count: number = +(lines[i++].split(/[\[\]]+/)[1]);
    return lines.slice(i, ((isNaN(count)) ? lines.length : i + count));
}

function extractFunctionInfo(line: String): FunctionInfo {
    let index = +(line.split(/[\[\]]+/)[1]);
    let name = `func ${index}`;
    let splitOnName = line.split(/[<>]/);
    if (splitOnName.length >= 2) {
        name = splitOnName[1];
    }
    return {index: index, name: name, locals: []};
}

function fillInLocalInfos(functionInfos: FunctionInfo[], lines: String[]): FunctionInfo[] {
    lines = lines.filter((line) => line.includes("local"));
    for (let i = 0; i < lines.length; i++) {
        let components = lines[i].split(/[\[\]<>]+/);
        if (components.length >= 6) {
            functionInfos[+components[1]].locals.push({
                index: i,
                name: components[5],
                type: "undefined",
                mutable: true
            });  // TODO get type from disassembly
        }
    }
    return functionInfos;
}

function fillInExportInfos() {
    // TODO
}

function extractGlobalInfo(line: String): VariableInfo {
    let global = {} as VariableInfo;
    let match = line.match(/\[([0-9]+)]/);
    global.index = (match === null) ? NaN : +match[1];
    match = line.match(/<([a-zA-Z0-9 ]+)>/);
    global.name = (match === null) ? `global ${global.index}` : match[1];
    match = line.match(/ ([if][0-9][0-9]) /);
    global.type = (match === null) ? "undefined" : match[1];
    match = line.match(/mutable=([0-9])/);
    global.mutable = match !== null && +match[1] === 1;
    return global;
}

export function getFunctionInfos(input: String): FunctionInfo[] {
    let functionLines: String[] = extractDetailedSection("Function[", input);
    let customLines = extractDetailedSection("Custom:", input);

    if (functionLines.length === 0) {
        throw Error("Could not parse 'Function' section of objdump");
    }

    let functions: FunctionInfo[] = [];
    functionLines.forEach((line) => {
        functions.push(extractFunctionInfo(line));
    });

    return fillInLocalInfos(functions, customLines);
}

export function getGlobalInfos(input: String): VariableInfo[] {
    let lines: String[] = extractDetailedSection("Global[", input);
    let globals: VariableInfo[] = [];
    lines.forEach((line) => {
        globals.push(extractGlobalInfo(line));
    });
    return globals;
}