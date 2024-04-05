#pragma once
#ifdef __ZEPHYR__
#include <zephyr/kernel.h>
#else
#include <mutex>
#include <condition_variable>
#include <thread>
#endif

namespace warduino {
#ifdef __ZEPHYR__
    class mutex {
        friend class condition_variable;
    public:
        mutex() {
            k_mutex_init(&m);
        }

        inline void lock() {
            k_mutex_lock(&m, K_FOREVER);
        }

        inline void unlock() {
            k_mutex_unlock(&m);
        }
    
    private:
        k_mutex m;
    };

    class lock_guard {
    public:
        explicit lock_guard(mutex m) : m(m) {
            m.lock();
        }

        ~lock_guard() {
            m.unlock();
        }

    private:
        mutex m;
    };


    class unique_lock {
        friend class condition_variable;
    public:
        explicit unique_lock(mutex m) : m(m) {
            m.lock();
        }

        ~unique_lock() {
            m.unlock();
        }

    private:
        mutex m;
    };
#else
    typedef std::mutex mutex;
    typedef std::lock_guard<std::mutex> lock_guard;
    typedef std::unique_lock<std::mutex> unique_lock;
    typedef std::thread thread; // TODO: Add Zephyr implementation for thread
#endif

    class condition_variable {
    public:
        condition_variable() {
#ifdef __ZEPHYR__
            k_condvar_init(&c);
#endif
        }

        void notify_one();

        void wait(warduino::unique_lock &l);

        template<typename P> void wait(warduino::unique_lock &m, P pred) {
            while (!pred()) {
                wait(m);
            }
        }

    private:
#ifdef __ZEPHYR__
        k_condvar c;
#else
        std::condition_variable c;
#endif
    };
}
