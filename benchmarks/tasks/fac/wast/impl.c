#include <emscripten/emscripten.h>

unsigned long __attribute__((noinline)) EMSCRIPTEN_KEEPALIVE fac(int x)
{
    if(x <= 1){
        return  1;
    } else {
        return  (x * fac(x-1));
    }
}
#pragma GCC push_options
#pragma GCC optimize ("O0")
int main() {
	return fac(12) % 251; // 228
}
#pragma GCC pop_options
