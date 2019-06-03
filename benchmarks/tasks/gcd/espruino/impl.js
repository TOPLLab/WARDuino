function gcd(u, v) {
             return (v != 0)?gcd(v, u%v):u;
         }



console.log("START")
for (let index = 0; index < 10; index++) {

    console.log(gcd(44486,12454))
}
console.log("DONE")