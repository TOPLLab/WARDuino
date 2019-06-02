#include <emscripten/emscripten.h>

long EMSCRIPTEN_KEEPALIVE fib(int n) {
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

int main()
{
    return fib(1501) % 100000000;
    // .       ..122583354898000
}
