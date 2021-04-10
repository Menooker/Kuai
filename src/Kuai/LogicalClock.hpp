#pragma once
#include <atomic>
#include <utility>
#include "SpinLock.hpp"
#include <vector>
namespace Kuai
{
    struct ThreadClock;
    struct GlobalClock
    {
        std::atomic<uint64_t> logicalClock = {0};
        SpinRWLock lock;
        std::vector<ThreadClock *> threads;
        void add_thread(ThreadClock *th)
        {
            auto lk = lock.write();
            std::lock_guard<SpinRWLock::WriteLock> guard(lk);
            threads.push_back(th);
        }

        void remove_thread(ThreadClock *th)
        {
            auto lk = lock.write();
            std::lock_guard<SpinRWLock::WriteLock> guard(lk);
            for (auto itr = threads.begin(); itr != threads.end(); ++itr)
            {
                if (*itr == th)
                {
                    threads.erase(itr);
                    break;
                }
            }
        }

        uint64_t get_min_lock();
        static GlobalClock clock;
    };

    struct ThreadClock
    {
        std::atomic<uint64_t> logicalClock = {0};
        ThreadClock()
        {
            GlobalClock::clock.add_thread(this);
        }
        ~ThreadClock()
        {
            GlobalClock::clock.remove_thread(this);
        }

        static thread_local ThreadClock tls_clock;
        static void updateLocalClock()
        {
            tls_clock.logicalClock.store(GlobalClock::clock.logicalClock.load());
        }
    };

    inline uint64_t GlobalClock::get_min_lock()
    {
        auto lk = lock.read();
        std::lock_guard<SpinRWLock::ReadLock> guard(lk);
        uint64_t ret = std::numeric_limits<uint64_t>::max();
        for (auto itr = threads.begin(); itr != threads.end(); ++itr)
        {
            ret = std::min((*itr)->logicalClock.load(), ret);
        }
        return ret;
    }
} // namespace Kuai