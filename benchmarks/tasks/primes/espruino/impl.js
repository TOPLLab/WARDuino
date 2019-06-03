
function is_prime(n) {
    if (n < 3) {
        return n > 1;
    } else {
        if ((n % 2 == 0) || (n % 3 == 0)) {
            return false;
        } else {
            let i = 5;
            while (i * i <= n) {
                if ((n % i == 0) || (n % (i + 2) == 0)) {
                    return false;
                }
                i += 6;
            }
            return true;

        }
    }
}

function main() {
    let sum = 0;
    let count = 0;
    for (let i = 1; sum < 133742; i++) {
        if (is_prime(i)){
            sum += i;
            count++;
        }
    }
    return count;
}



console.log("START")
for (let index = 0; index < 10; index++) {

    console.log(main())
}
console.log("DONE")