import {WASM} from "latch";
import {readFileSync} from "fs";
import Value = WASM.Value;

interface Cursor {
    value: number;
}

export function parseResult(input: string): WASM.Value<WASM.Type> | undefined {
    let cursor = 0;
    let delta: number = consume(input, cursor, /\(/d);
    if (delta === 0) {
        return undefined;
    }
    cursor += delta;

    delta = consume(input, cursor, /^[^.)]*/d);
    let type: WASM.Type = WASM.typing.get(input.slice(cursor, cursor + delta)) ?? WASM.Integer.i64;

    cursor += delta + consume(input, cursor + delta);

    delta = consume(input, cursor, /^[^)]*/d);
    return parseValue(input.slice(cursor, cursor + delta), type);
}

function parseValue(input: string, type: WASM.Type): Value<WASM.Type> | undefined {
    let value: number | bigint;
    switch (type) {
        case WASM.Float.f32:
        case WASM.Float.f64:
            value = parseHexFloat(input);
            break;
        case WASM.Integer.u32:
        case WASM.Integer.i32:
        case WASM.Integer.u64:
        case WASM.Integer.i64:
            value = parseInteger(input);
            break;
        default:
            return undefined;
    }
    type = typeof value !== 'bigint' && isNaN(<number>value) ? WASM.Special.nan
        : typeof value !== 'bigint' && <number>value === Infinity ? WASM.Special.infinity
            : type;

    return {type, value};
}

export function parseArguments(input: string, index: Cursor): WASM.Value<WASM.Type>[] {
    const args: WASM.Value<WASM.Type>[] = [];

    let cursor: number = consume(input, 0, /invoke "[^"]+"/d);
    while (cursor < input.length) {
        let delta: number = consume(input, cursor, /^[^)]*\(/d);
        if (delta === 0) {
            break;
        }
        cursor += delta;

        delta = consume(input, cursor, /^[^.)]*/d);
        const type: WASM.Type = WASM.typing.get(input.slice(cursor + delta - 3, cursor + delta)) ?? WASM.Integer.i64;

        cursor += delta + consume(input, cursor + delta, /^[^)]*const /d);
        delta = consume(input, cursor, /^[^)]*/d);
        let maybe = parseValue(input.slice(cursor, cursor + delta), type);

        if (maybe !== undefined) {
            args.push(maybe);
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

function shouldParseLine(input: string): boolean {
    return input.includes('(assert_return') && !input.replace(/\s+/g, '').startsWith(';;');
}

export function parseAsserts(file: string): string[] {
    const asserts: string[] = [];
    readFileSync(file).toString().split('\n').forEach((line) => {
        if (shouldParseLine(line)) {
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

function parseInteger(hex: string, bytes: number = 4): bigint | number {
    if (!hex.includes('0x')) {
        const n: number = parseInt(hex);
        return typeof n !== 'bigint' && isNaN(n) ? NaN : typeof n !== 'bigint' && n === Infinity ? Infinity : BigInt(n);
    }
    const mask = BigInt(parseInt('0x80' + '00'.repeat(bytes - 1), 16));
    let integer = BigInt(parseInt(hex, 16));
    if (integer >= mask) {
        integer = integer - mask * 2n;
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