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
        SpinLock() = default;
        SpinLock(const SpinLock &) = delete;
        SpinLock(SpinLock &&) = delete;
    };

    struct SpinRWLock
    {
        std::atomic<int> readCount = {0};
        SpinRWLock() = default;
        SpinRWLock(const SpinRWLock &) = delete;
        SpinRWLock(SpinRWLock &&) = delete;

        struct ReadLock
        {
            SpinRWLock& lc;
            ReadLock(SpinRWLock &l) : lc(l) {}
            ReadLock(const ReadLock &) = delete;
            ReadLock(ReadLock &&) = default;
            void lock()
            {
                int oldv = lc.readCount.load();
                for (;;)
                {
                    // if read count >=0 (no write lock), count++
                    if (oldv >= 0)
                    {
                        if (lc.readCount.compare_exchange_weak(oldv, oldv + 1))
                        {
                            break;
                        }
                    }
                    else
                    {
                        oldv = lc.readCount.load();
                    }
                }
            }

            void unlock()
            {
                --lc.readCount;
            }
        };

        struct WriteLock
        {
            SpinRWLock &lc;
            WriteLock(SpinRWLock &l) : lc(l) {}
            WriteLock(const WriteLock &) = delete;
            WriteLock(WriteLock &&) = default;
            void lock()
            {
                int oldv = 0;
                while (!lc.readCount.compare_exchange_weak(oldv, -1))
                {
                    oldv = 0;
                }
            }

            void unlock()
            {
                lc.readCount.store(0);
            }
        };

        ReadLock read()
        {
            return ReadLock(*this);
        }
        WriteLock write()
        {
            return WriteLock(*this);
        }
    };
} // namespace Kuai