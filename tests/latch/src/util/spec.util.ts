import {WASM} from "latch";
import {readFileSync} from "fs";

interface Cursor {
    value: number;
}

// 8b  -> 16 elems; pad  2
// 16b ->  8 elems, pad  4
// 32b ->  4 elems, pad  8
// 64b ->  2 elems, pad 16
// -> 128 / b elems; pad b / 4

function parseV128(type: string, args: string[]): string | undefined {
    const int_lambda = (bit_width: number, mask: bigint): string|undefined => {
        const elems = 128 / bit_width;
        const pad_len = bit_width / 4;

        if(args.length !== elems) return undefined;
        return args
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
    };

    const float_lambda = (bit_width: number, mask: bigint, flt2bigint: (flt: number, ab: ArrayBuffer) => bigint): string|undefined => {
        const elems = 128 / bit_width;
        const pad_len = bit_width / 4;

        if(args.length !== elems) return undefined;
        return args
            .map(str => str.replace(/_/gi, '')) // WASM allows _ in numbers, TS doesn't like those
            .map(str => parseFloat(str)) // parse to float ~ might not accept all floats
            .map(flt => {
                const buf = new ArrayBuffer(bit_width / 8);
                return flt2bigint(flt, buf) & mask;
            })
            .map(num => num.toString(16).padStart(pad_len, '0')) // convert to hex
            .reduce((acc, str) => acc + str, ''); // concat
    };

    const flt32_lambda = (flt: number, ab: ArrayBuffer) => {
        const fltBuffer = new Float32Array(ab);
        fltBuffer[0] = flt;
        const intBuffer = new Int32Array(ab);
        return BigInt(intBuffer[0]);
    };

    const flt64_lambda = (flt: number, ab: ArrayBuffer) => {
        const fltBuffer = new Float64Array(ab);
        fltBuffer[0] = flt;
        const intBuffer = new BigInt64Array(ab);
        return intBuffer[0];
    };

    switch(type) {
        case 'i8x16': return int_lambda(8,  0x00000000000000ffn);
        case 'i16x8': return int_lambda(16, 0x000000000000ffffn);
        case 'i32x4': return int_lambda(32, 0x00000000ffffffffn);
        case 'i64x2': return int_lambda(64, 0xffffffffffffffffn);

        case 'f32x4': return float_lambda(32, 0x00000000ffffffffn, flt32_lambda);
        case 'f64x2': return float_lambda(64, 0xffffffffffffffffn, flt64_lambda);

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
        // console.log(`arg #${args.length}.type: ${input.slice(cursor + delta - 3, cursor + delta)}`);

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