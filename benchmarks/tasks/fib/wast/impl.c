long __attribute__((noinline)) fib(int n) {
    unsigned long first = 0, second = 1, next = 0;
    for (unsigned c = 0; c < n; c++) {
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

int bench() {
    int sum = 0;
#pragma nounroll
    for (int i = 1000; i < 1050; i++) {
        sum += fib(i);
        sum %= 97;
    }
    return sum;
    // .       ..122583354898000
}
