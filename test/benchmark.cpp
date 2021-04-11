#include <Kuai/ConcurrentHashMap.hpp>
#include <Kuai/Globals.hpp>
#include <utility>
#include <cassert>
#include <thread>
#include <chrono>
#include <unordered_map>
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
        if (printit)
            printf("CNT=%d\n", sum);
    };
    std::thread threads[8];
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 8; i++)
    {
        threads[i] = std::thread(thread_func);
    }
    for (int i = 0; i < 8; i++)
    {
        threads[i].join();
    }
    auto endt = std::chrono::high_resolution_clock::now();
    if (printit)
        printf("TIME= %ld ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(endt - start).count());
}

void multi_thread_read_same_entry()
{
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
}

int main()
{
    multi_thread_read_same_entry();
}