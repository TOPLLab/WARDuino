function fac(x) {
    if ( x <= 1 ){
        return 1;
    }
    else {
        return (x*fac(x-1));
    }

}


function main() {
    let sum = 0;
    for(let i = 0; i < 10000; i++){
        sum += fac(i % 12);
        sum %= 97;
    }
    return sum;
}



console.log("START")
for (let index = 0; index < 10; index++) {

    console.log(main()); // 228
}
console.log("DONE")
