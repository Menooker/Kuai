#pragma once
#include <atomic>
#include <mutex> //lock_guard
namespace Kuai
{

    struct SpinLock
    {
        std::atomic<int> v = {0};
        void lock()
        {
            int oldv = 0;
            while (!v.compare_exchange_weak(oldv, 1))
            {
                oldv = 0;
            }
        }

        void unlock()
        {
            v.store(0);
        }
    };
} // namespace Kuai