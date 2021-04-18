#include <Kuai/ConcurrentHashMap.hpp>
#include <Kuai/Globals.hpp>
#include <utility>
#include <cassert>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <pthread.h>
using namespace Kuai;

using RemovableMap = ConHashMap<PolicyCanRemove, int, int>;
using NonRemovableMap = ConHashMap<PolicyNoRemove, int, int>;

struct StdHashMap
{
    std::unordered_map<int, int> impl;
    StdHashMap(int size) : impl(size) {}
    void set(int k, int v)
    {
        impl[k] = v;
    }
    int *get(int k)
    {
        return &impl.find(k)->second;
    }
};

struct StdHashMapLocked
{
    std::unordered_map<int, int> impl;
    pthread_rwlock_t lk;
    StdHashMapLocked(int size) : impl(size) { pthread_rwlock_init(&lk, nullptr); }
    void set(int k, int v)
    {
        pthread_rwlock_wrlock(&lk);
        impl[k] = v;
        pthread_rwlock_unlock(&lk);
    }
    int *get(int k)
    {
        pthread_rwlock_rdlock(&lk);
        auto itr = impl.find(k);
        int *ret = (itr == impl.end()) ? nullptr : &itr->second;
        pthread_rwlock_unlock(&lk);
        return ret;
    }
};

template <typename T>
void same_entry_test(int num_iter, bool printit)
{
    T map(1024);
    map.set(0, 123);
    map.set(1, 123);
    map.set(2, 123);
    auto thread_func = [&map, num_iter, printit]() {
        int sum = 0;
        for (int i = 0; i < num_iter; i++)
        {
            sum += *map.get(2);
        }
    };
    std::thread threads[4];
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 4; i++)
    {
        threads[i] = std::thread(thread_func);
    }
    for (int i = 0; i < 4; i++)
    {
        threads[i].join();
    }
    auto endt = std::chrono::high_resolution_clock::now();
    if (printit)
        printf("TIME= %ld ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(endt - start).count());
}

void multi_thread_read_same_entry()
{
    printf("******************\nSame entry performance\n");
    int num_iter = 5000000;
    printf("====================\nRemovable\n");
    same_entry_test<RemovableMap>(1000, false);
    same_entry_test<RemovableMap>(num_iter, true);

    printf("====================\nNonRemovable\n");
    same_entry_test<NonRemovableMap>(1000, false);
    same_entry_test<NonRemovableMap>(num_iter, true);

    printf("====================\nstd::unordered_map\n");
    same_entry_test<StdHashMap>(1000, false);
    same_entry_test<StdHashMap>(num_iter, true);

    printf("====================\nstd::unordered_map with RWLock\n");
    same_entry_test<StdHashMapLocked>(1000, false);
    same_entry_test<StdHashMapLocked>(num_iter, true);
}

uint32_t myrand(uint32_t &seed)
{
    seed = seed * 53231 + 99991;
    return seed >> 4;
}

template <typename T>
void do_perf_test(int num_iter, int read_percent, bool printit)
{
    T map(1024 * 1024);
    std::atomic<bool> startflag = {{false}};
    auto thread_func = [&map, num_iter, printit, read_percent, &startflag](uint32_t seed) {
        while (!startflag)
            ;

        int sum = 0;
        for (int i = 0; i < num_iter; i++)
        {
            auto action = myrand(seed);
            if (action % 100 <= read_percent)
            {
                auto val = map.get(myrand(seed));
                if (val)
                {
                    sum += *val;
                }
            }
            else
            {
                map.set(myrand(seed), myrand(seed));
            }
        }
    };
    std::thread threads[4];
    for (int i = 0; i < 4; i++)
    {
        threads[i] = std::thread(thread_func, i);
    }
    startflag = true;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 4; i++)
    {
        threads[i].join();
    }
    auto endt = std::chrono::high_resolution_clock::now();
    if (printit)
        printf("TIME= %ld ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(endt - start).count());
}

void perf_test(int read_percent)
{

    printf("******************\nPerf test, read = %d%%\n", read_percent);
    int num_iter = 500000;
    printf("====================\nRemovable\n");
    do_perf_test<RemovableMap>(1000, read_percent, false);
    do_perf_test<RemovableMap>(num_iter, read_percent, true);

    printf("====================\nNonRemovable\n");
    do_perf_test<NonRemovableMap>(1000, read_percent, false);
    do_perf_test<NonRemovableMap>(num_iter, read_percent, true);

    printf("====================\nstd::unordered_map\n");
    do_perf_test<StdHashMapLocked>(1000, read_percent, false);
    do_perf_test<StdHashMapLocked>(num_iter, read_percent, true);

    if (read_percent == 100)
    {
        printf("====================\nstd::unordered_map\n");
        do_perf_test<StdHashMapLocked>(1000, read_percent, false);
        do_perf_test<StdHashMapLocked>(num_iter, read_percent, true);
    }
}

int main()
{
    perf_test(20);
    perf_test(80);
    perf_test(100);
    multi_thread_read_same_entry();
}