export function jsonParse(obj: string) {
    return new Function(`return ${obj}`)();
}

export function extractAddressInformation( addressLine : string) : string {
    let regexpr = /^(?<address>([\da-f])+):/;
    let match = addressLine.match(regexpr);
    if(match?.groups) {
        return match.groups.address;
    }
    throw Error(`Could not parse address from line: ${addressLine}`);
}

export function getFileExtension(file: string) : string {
    let splitted =  file.split('.');
    if(splitted.length === 2){
        return splitted.pop()!;
    }
    throw Error("Could not determine file type");
}