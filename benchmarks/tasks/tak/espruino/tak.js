function tak(x, y, z) {
    if (!(y < x)) {
        return z;
    }
    else {
        return tak(tak(x - 1, y, z),
            tak(y - 1, z, x),
            tak(z - 1, x, y));
    }

}



console.log("START")
for (let index = 0; index < 10; index++) {

    console.log(tak(18, 12, 6))
}
console.log("DONE")