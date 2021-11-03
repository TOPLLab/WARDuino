int __attribute__((noinline)) tak(int x, int y, int z) {
    if (!(y < x)) {
        return z;
    } else {
        return tak(tak(x - 1, y, z), tak(y - 1, z, x), tak(z - 1, x, y));
    }
}

int bench() { return tak(18, 12, 6); }
