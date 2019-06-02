function fib(n) {
    let first = 0, second = 1, next = 0;
    for (let c = 0; c < n; c++) {
        if (c <= 1) {
            next = c;
        } else {
            next = (first + second) % 100000000;
            first = second;
            second = next;
        }
    }
    return next;
}

function main(){
    return fib(1501) % 100000000;
}

console.log("START")
for (let index = 0; index < 10; index++) {

    console.log(index,main())
}
console.log("DONE")