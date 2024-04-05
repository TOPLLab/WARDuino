#ifndef ARDUINO
#include <zephyr/kernel.h>

#include "warduino-thread.h"

void warduino::condition_variable::notify_one() { k_condvar_signal(&c); }

void warduino::condition_variable::wait(warduino::unique_lock &l) {
    k_condvar_wait(&c, &l.m.m, K_FOREVER);
}
#endif
