function binomial(m, n){
    var r = 1, d = m - n;
    if (d > n) { n = d; d = m - n; }

    while (m > n) {
        r *= m--;
        while (d > 1 && ! (r%d) ) r /= d--;
    }

    return r;
}

function catalan(n) {
    return binomial(2 * n, n) / (1 + n);
}

function main(){
    var sum = 0;
    for (let i = 0; i < 10000; ++i) {
        sum += catalan((i + sum) % 18) % 100;
    }
	return sum % 256
}

console.log("START")
for (let index = 0; index < 10; index++) {

    console.log( main()); // 113
}
console.log("DONE")
