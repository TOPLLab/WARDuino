#include "warduino-thread.h"

void warduino::condition_variable::notify_one() {
    c.notify_one();
}

void warduino::condition_variable::wait(warduino::unique_lock &l) {
    c.wait(l);
}
