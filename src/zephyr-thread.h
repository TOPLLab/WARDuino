#include <zephyr/kernel.h>

namespace zephyr {
class mutex {
    friend class condition_variable;

   public:
    mutex() { k_mutex_init(&m); }

    inline void lock() { k_mutex_lock(&m, K_FOREVER); }

    inline void unlock() { k_mutex_unlock(&m); }

   private:
    k_mutex m;
};

class lock_guard {
   public:
    lock_guard(mutex m) : m(m) { m.lock(); }

    ~lock_guard() { m.unlock(); }

   private:
    mutex m;
};

class condition_variable {
   public:
    condition_variable() { k_condvar_init(&c); }

    void notify_one() { k_condvar_signal(&c); }

    void wait(mutex m) { k_condvar_wait(&c, &m.m, K_FOREVER); }

    template <typename P>
    void wait(mutex m, P pred) {
        while (!pred()) {
            wait(m);
        }
    }

   private:
    k_condvar c;
};
}  // namespace zephyr
