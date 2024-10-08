import {WASM} from "latch";
import {readFileSync} from "fs";

interface Cursor {
    value: number;
}

function float32HexStr(x: number): string {
    const ab = new ArrayBuffer(4);
    const fb = new Float32Array(ab);
    fb[0] = x;
    const ui8 = new Uint8Array(ab);
    let res = '';
    for (let i = 3; i >= 0; i--) {
        res += ui8[i].toString(16).padStart(2, '0');
    }
    return res;
}

function float64HexStr(x: number): string {
    const ab = new ArrayBuffer(8);
    const fb = new Float64Array(ab);
    fb[0] = x;
    const ui8 = new Uint8Array(ab);
    let res = '';
    for (let i = 7; i >= 0; i--) {
        res += ui8[i].toString(16).padStart(2, '0');
    }
    return res;
}

function parseWasmFloat32(str: string) {
    const strBuf = str.replace(/_/gi, ''); // remove those damned underscores

    const flt = parseHexFloat(strBuf);
    const res = float32HexStr(flt);
    return res;
}

function parseWasmFloat64(str: string): string|undefined {
    const strBuf = str.replace(/_/gi, ''); // remove those damned underscores

    // just use hexFloat - it works for 64-bit floats
    const flt = parseHexFloat(strBuf);
    const res = float64HexStr(flt);
    return res;
}

function parseV128(type: string, args: string[]): string | undefined {
    const int_lambda = (bit_width: number, mask: bigint): string|undefined => {
        const elems = 128 / bit_width;
        const pad_len = bit_width / 4;

        if(args.length !== elems) return undefined;
        const res = args
            .map(str => str.replace(/_/gi, '')) // WASM allows _ in numbers, TS doesn't like those
            .map(str => {
                let start_idx = 0;
                let sign = 1;
                if(str.startsWith('-')) { sign = -1; start_idx = 1; }
                else if(str.startsWith('+')) { start_idx = 1; }
                return BigInt(sign) * BigInt(str.slice(start_idx))
            }) // parse to (big)-int
            .map(num => num & mask) // ensure correct bit width
            .map(num => num.toString(16).padStart(pad_len, '0')) // convert to hex
            .reduce((acc, val) => acc + val, ''); // concat
        return res;
    };

    const float_lambda = (elem_count: number, parser: (s: string) => string|undefined): string|undefined => {
        if(args.length !== elem_count) return undefined;
        const parsed = args.map(str => parser(str));
        if(parsed.some(str => str === undefined)) return undefined;
        return parsed.reduce((acc, val) => acc + (val as string), '');
    }

    switch(type) {
        case 'i8x16': return int_lambda(8,  0x00000000000000ffn);
        case 'i16x8': return int_lambda(16, 0x000000000000ffffn);
        case 'i32x4': return int_lambda(32, 0x00000000ffffffffn);
        case 'i64x2': return int_lambda(64, 0xffffffffffffffffn);

        case 'f32x4': return float_lambda(4, parseWasmFloat32);
        case 'f64x2': return float_lambda(2, parseWasmFloat64);

        default: return undefined;
    }
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
    } else if(type === WASM.Type.v128) {
        const slice = input.slice(cursor, cursor + delta).split(' ').filter(x => x.trim() !== ''); // [dim, arg1, ...]
        value = parseV128(slice[0], slice.slice(1));
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
        let maybe: number | bigint | undefined;
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

    if (input.includes('e')) {
        return parseFloat(input);
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

function parseInteger(hex: string, bytes: number = 4): bigint {
    if(hex.startsWith('-')) {
        return BigInt(-1) * BigInt(hex.slice(1));
    }
    return BigInt(hex);
}

export function find(regex: RegExp, input: string) {
    const match = regex.exec(input);
    if (match === null || match[1] === undefined) {
        return '';
    }
    return match[1];
}