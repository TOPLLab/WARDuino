#include <emscripten/emscripten.h>


int  EMSCRIPTEN_KEEPALIVE gcd(int u, int v) {
    return (v != 0)?gcd(v, u%v):u;
}

int main() {
	return gcd(44486,12454);
}
