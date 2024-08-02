#pragma once
#ifdef __ZEPHYR__
#include <pthread.h>
#include <zephyr/kernel.h>
#else
#include <condition_variable>
#include <mutex>
#include <thread>
#endif

namespace warduino {
#ifdef __ZEPHYR__
class mutex {
    friend class condition_variable;

   public:
    mutex() { k_mutex_init(&m); }

    inline void lock() { k_mutex_lock(&m, K_FOREVER); }

    inline int try_lock() { return k_mutex_lock(&m, K_MSEC(20)) == 0; }

    inline void unlock() { k_mutex_unlock(&m); }

   private:
    k_mutex m;
};

class lock_guard {
   public:
    explicit lock_guard(mutex m) : m(m) { m.lock(); }

    ~lock_guard() { m.unlock(); }

   private:
    mutex m;
};

class unique_lock {
    friend class condition_variable;

   public:
    explicit unique_lock(mutex &m) : m(&m) { m.lock(); }

    ~unique_lock() { m->unlock(); }

   private:
    mutex *m;
};

class thread {
   public:
    thread() {}

    thread(void *(*f)(void *)) { pthread_create(&thid, NULL, f, NULL); }

    template <class F, class T>
    thread(F &&f, T &&arg0) {
        pthread_create(&thid, NULL, reinterpret_cast<void *(*)(void *)>(f),
                       arg0);
    }

    void join() {
        void *ret;
        if (pthread_join(thid, &ret)) {
            printk("Failed to join thread!\n");
            k_fatal_halt(1);
        }
    }

   private:
    pthread_t thid;
};

class condition_variable {
   public:
    condition_variable() { k_condvar_init(&c); }

    inline void notify_one() { k_condvar_signal(&c); }

    inline void wait(warduino::unique_lock &l) {
        k_condvar_wait(&c, &l.m->m, K_FOREVER);
    }

    template <typename P>
    void wait(warduino::unique_lock &m, P pred) {
        while (!pred()) {
            wait(m);
        }
    }

   private:
    k_condvar c;
};
#else
typedef std::mutex mutex;
typedef std::lock_guard<std::mutex> lock_guard;
typedef std::unique_lock<std::mutex> unique_lock;
typedef std::thread thread;
typedef std::condition_variable condition_variable;
#endif
}  // namespace warduino
