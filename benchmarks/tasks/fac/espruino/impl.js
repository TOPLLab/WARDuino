function fac(x) {
    if ( x <= 1 ){
        return 1;
    }
    else {
        return (x*fac(x-1));
    }

}



console.log("START")
for (let index = 0; index < 10; index++) {

    console.log(fac(12) % 251); // 228
}
console.log("DONE")