import {WASM} from "latch";
import {readFileSync} from "fs";

interface Cursor {
    value: number;
}

export function parseResult(input: string): WASM.Value | undefined {
    let cursor = 0;
    let delta: number = consume(input, cursor, /\(/d);
    if (delta === 0) {
        return undefined;
    }
    cursor += delta;

    delta = consume(input, cursor, /^[^.)]*/d);
    const type: WASM.Type = WASM.typing.get(input.slice(cursor, cursor + delta)) ?? WASM.Type.i64;

    cursor += delta + consume(input, cursor + delta);

    let value;
    delta = consume(input, cursor, /^[^)]*/d);
    if (type === WASM.Type.f32 || type === WASM.Type.f64) {
        value = parseHexFloat(input.slice(cursor, cursor + delta));
    } else {
        value = parseInteger(input.slice(cursor, cursor + delta));
    }

    if (value === undefined) {
        return value;
    }

    return {type, value};
}

export function parseArguments(input: string, index: Cursor): WASM.Value[] {
    const args: WASM.Value[] = [];

    let cursor: number = consume(input, 0, /invoke "[^"]+"/d);
    while (cursor < input.length) {
        let delta: number = consume(input, cursor, /^[^)]*\(/d);
        if (delta === 0) {
            break;
        }
        cursor += delta;

        delta = consume(input, cursor, /^[^.)]*/d);
        const type: WASM.Type = WASM.typing.get(input.slice(cursor + delta - 3, cursor + delta)) ?? WASM.Type.i64;

        cursor += delta + consume(input, cursor + delta, /^[^)]*const /d);
        delta = consume(input, cursor, /^[^)]*/d);
        let maybe: number | undefined;
        if (type === WASM.Type.f32 || type === WASM.Type.f64) {
            maybe = parseHexFloat(input.slice(cursor, cursor + delta));
        } else {
            maybe = parseInteger(input.slice(cursor, cursor + delta));
        }

        if (maybe !== undefined) {
            args.push({type, value: maybe});
        }

        cursor += consume(input, cursor, /\)/d);
        if (input[cursor] === ')') {
            break;
        }
    }

    index.value = cursor;

    return args;
}

function consume(input: string, cursor: number, regex: RegExp = / /d): number {
    const match = regex.exec(input.slice(cursor));
    // @ts-ignore
    return (match?.indices[0][1]) ?? 0;
}

export function parseAsserts(file: string): string[] {
    const asserts: string[] = [];
    readFileSync(file).toString().split('\n').forEach((line) => {
        if (line.includes('(assert_return')) {
            asserts.push(line.replace(/.*\(assert_return\s*/, '('));
        }
    });
    return asserts;

}


// Sign function that returns non-zero values
function sign(integer: number): number {
    if (Object.is(integer, -0)) {
        return -1;
    }
    return Math.sign(integer) || 1;
}

function parseHexFloat(input: string): number {
    if (input.includes('-inf')) {
        return -Infinity;
    }

    if (input.includes('inf')) {
        return Infinity;
    }

    const radix: number = input.includes('0x') ? 16 : 10;
    let base: string = input, mantissa, exponent = 0;

    const splitIndex = input.indexOf('p');
    if (splitIndex !== -1) {
        base = input.slice(0, splitIndex);
        exponent = parseInt(input.slice(splitIndex + 1));
    }

    const dotIndex = base.indexOf('.');
    if (dotIndex !== -1) {
        const [integer, fractional] = base.split('.').map(hexStr => parseInt(hexStr, radix));
        const fraction = fractional / Math.pow(radix, base.length - dotIndex - 1);
        mantissa = sign(integer) * (Math.abs(integer) + fraction);
    } else {
        mantissa = parseInt(base, radix);
    }

    return mantissa * Math.pow(2, exponent);
}

function parseInteger(hex: string, bytes: number = 4): number {
    if (!hex.includes('0x')) {
        return parseInt(hex);
    }
    const mask = parseInt('0x80' + '00'.repeat(bytes - 1), 16);
    let integer = parseInt(hex, 16);
    if (integer >= mask) {
        integer = integer - mask * 2;
    }
    return integer;
}

export function find(regex: RegExp, input: string) {
    const match = regex.exec(input);
    if (match === null || match[1] === undefined) {
        return '';
    }
    return match[1];
}