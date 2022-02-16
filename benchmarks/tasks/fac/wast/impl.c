unsigned long __attribute__((noinline)) fac(int x) {
    if (x <= 1) {
        return 1;
    } else {
        return (x * fac(x - 1));
    }
}

int bench() {
    int sum = 0;
#pragma clang loop unroll(disable)
    for (int i = 0; i < 10000; i++) {
        sum += fac(i % 12);
        sum %= 97;
    }
    return sum;
}
