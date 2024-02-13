import {readdirSync, readFileSync, writeFileSync} from "fs";
import {extname} from "path";

// geometric mean
const mean = (list) => {
    let product = 1;
    list.forEach((item) => {
        product = product * item
    });

    return Math.pow(product, 1 / list.length);
}

const rows = [];

const esp = {}
const emu = {}

readdirSync(`../experiments/pc0/`).filter(file => extname(file).toLowerCase() === '.log').forEach(file => {
    const suite = JSON.parse(readFileSync(`../experiments/pc0/` + file).toString());

    if (suite['passes'] !== undefined && suite['passes'].length > 1) {
        const passes = suite['passes'][0];
        const match = passes.match(/ (?<name>[^\n _]*_??[^\n _]*)_?[0-9]*.wast/);
        const name = match.groups.name;
        const asserts = suite['passed actions'];

        esp[name] = {
            size: asserts,
            durations: []
        }

        emu[name] = {
            size: asserts,
            durations: []
        }
    }
});

for (let i = 0; i < 10; i++) {
    const hardware = `../experiments/esp${i}/`;
    readdirSync(hardware).filter(file => extname(file).toLowerCase() === '.log').forEach(file => {
        const suite = JSON.parse(readFileSync(hardware + file).toString());

        if (suite['passes'] !== undefined && suite['passes'].length > 1) {
            const passes = suite['passes'][0];
            const match = passes.match(/ (?<name>[^\n _]*_??[^\n _]*)_?[0-9]*.wast/);
            const name = match.groups.name;
            const duration = suite['duration (ms)'];

            esp[name].durations.push(duration);
        } else {
            console.log(suite);
        }
    });
}

const emulator = `../experiments/pc2/`;
readdirSync(emulator).filter(file => extname(file).toLowerCase() === '.log').forEach(file => {
    const suite = JSON.parse(readFileSync(emulator + file).toString());

    if (suite['passes'] !== undefined && suite['passes'].length > 1) {
        const passes = suite['passes'][0];
        const match = passes.match(/ (?<name>[^\n _]*_??[^\n _]*)_?[0-9]*.wast/);
        const name = match.groups.name;
        const duration = suite['duration (ms)'];

        emu[name].durations.push(duration);
    } else {
        console.log(suite);
    }
});

for (const point in esp) {
    const c = Math.round(mean(esp[point].durations));
    if (!isNaN(c)) rows.push([point, esp[point].size, c, Math.round(mean(emu[point].durations))]);
}

rows.push(["total",
    rows.map(x => +x[1]).reduce((acc, val) => acc + val, 0),
    rows.map(x => +x[2]).reduce((acc, val) => acc + val, 0),
    rows.map(x => +x[3]).reduce((acc, val) => acc + val, 0)])

rows.sort((a, b) => (a[1] === b[1]) ? 0 : ((a[1] < b[1]) ? -1 : 1));

rows.unshift(["name", "asserts", "hardware", "emulator"]);

writeFileSync('../experiments/runtime.csv', rows.map(e => e.join(",")).join("\n"));
