#include <emscripten/emscripten.h>

unsigned long __attribute__((noinline)) EMSCRIPTEN_KEEPALIVE fac(int x)
{
    if(x <= 1){
        return  1;
    } else {
        return  (x * fac(x-1));
    }
}

int main() {
    int sum = 0;
    #pragma nounroll
    for(int i = 0; i < 10000; i++){
        sum += fac(i % 12);
        sum %= 97;
    }
    return sum;
}
