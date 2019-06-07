function gcd(u, v) {
    return (v != 0) ? gcd(v, u % v) : u;
}

function main() {
    let sum = 0;
    for (let i = 40000; i < 50000; i++) {
        sum += gcd(i, 12345);
    }
    return sum;
}

console.log("START")
for (let index = 0; index < 10; index++) {

    console.log(main())
}
console.log("DONE")