#include <emscripten/emscripten.h>

unsigned long EMSCRIPTEN_KEEPALIVE fac(int x)
{
    if(x <= 1){
        return  1;
    } else {
        return  (x * fac(x-1));
    }
}

int main() {
	return fac(12) % 251; // 228
}
