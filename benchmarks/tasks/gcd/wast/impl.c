int __attribute__((noinline)) gcd(int u, int v) {
    return (v != 0) ? gcd(v, u % v) : u;
}

int main() {
    int sum = 0;
    for (int i = 40000; i < 50000; i++) {
        sum += gcd(i, 12345);
    }
    return sum;
}
