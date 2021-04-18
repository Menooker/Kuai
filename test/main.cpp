#include <Kuai/ConcurrentHashMap.hpp>
#include <Kuai/Globals.hpp>
#include <utility>
#include <cassert>
#include <thread>
#include <string.h>
using namespace Kuai;

constexpr int bufsize = 1024 * 1024 * 64;
std::atomic<int> vec[bufsize] = {{0}};

#define STR(t) #t
#define LINE_STR(v) STR(v)
#define _FILE_LINE_ "File : " #__FILE__ ", Line : " LINE_STR(__LINE__)

#define myassert(cond)                                                                       \
    if (!(cond))                                                                             \
    {                                                                                        \
        fprintf(stderr, "Assertion failed at %s:%d, line: %s\n", __FILE__, __LINE__, #cond); \
        std::abort();                                                                        \
    }

uint32_t myrand(uint32_t &seed)
{
    seed = seed * 53231 + 99991;
    return seed >> 4;
}

uint32_t myrand_n0(uint32_t &seed)
{
    auto ret = myrand(seed);
    if (ret == 0)
    {
        return 2;
    }
    return ret;
}

/**
 * Randomly read/set/remove to a map. It uses an int array as the reference to check the hash map result.
 * */
template <int numActions, typename Policy, typename GCFunc, typename DelFunc>
void randomTest(GCFunc &&dogc, DelFunc &&dodel)
{
    ConHashMap<Policy, int, int> map(1024);
    std::atomic<int> reads = {0};
    std::atomic<int> writes = {0};
    std::atomic<int> deletes = {0};
    auto runner = [&map, &reads, &writes, &deletes, &dogc, &dodel](uint32_t seed) {
        int tid = seed;
        for (int i = 0; i < 250000; i++)
        {
            if (tid == 0 && i % 1000 == 0)
            {
                dogc(map);
            }
            auto idx = myrand(seed);
            auto action = idx / bufsize;
            idx = idx % bufsize;
            auto doRead = [&]() {
                auto ret = map.get(idx);
                auto true_val = vec[idx].load();
                myassert(bool(ret) == (true_val != 0));
                if (true_val)
                {
                    myassert(*ret == true_val);
                    auto oldv = map.setIfAbsent(idx, 2);
                    myassert(oldv);
                    myassert(*oldv == true_val);
                }
                ++reads;
            };
            switch (action % numActions)
            {
            case 0:
            {
                auto value = myrand_n0(seed);
                int newval = 0;
                if (vec[idx].compare_exchange_strong(newval, value))
                {
                    auto oldv = map.setIfAbsent(idx, value);
                    myassert(!oldv);
                    ++writes;
                }
                else
                {
                    doRead();
                }
                break;
            }
            case 1:
                doRead();
                break;
            case 2:
            {
                int newval = 0;
                int oldv = vec[idx];
                if (oldv && vec[idx].compare_exchange_strong(oldv, newval))
                {
                    dodel(map, idx);
                    ++deletes;
                }
                else
                {
                    doRead();
                }
                break;
            }
            default:
                break;
            }
        }
    };
    std::thread threads[4];
    for (int i = 0; i < 4; i++)
    {
        threads[i] = std::thread(runner, i);
    }

    for (int i = 0; i < 4; i++)
    {
        threads[i].join();
    }
    printf("Done r=%d, w=%d, d=%d\n", reads.load(), writes.load(), deletes.load());
}

// tests the lifetime of the removed object. It should be alive before all threads update the local lock
void removalTest()
{
    struct Checker
    {
        volatile int *a;
        // when the object is freed, it will set the contents to the pointer
        ~Checker()
        {
            if (a)
            {
                *a = 123;
            }
        }
    };
    ConHashMap<PolicyCanRemove, int, Checker> map(1024);
    map.set(10, Checker{nullptr});
    printf("Insertion done\n");
    Checker *ptr = map.get(10);
    volatile int checkVal = 0;
    // connect the object instance to the local varibale. if the object is freed, checkVal will be set to 123
    ptr->a = &checkVal;
    std::atomic<bool> done = {{false}};
    // a GC thread calling GC of the map.
    std::thread gcThread([&]() {
        while (!done)
        {
            std::this_thread::yield();
            map.garbageCollect();
        }
    });

    // remove the object in another thread.  It should not free the object yet because the main thread has not acknowledged it
    std::thread removerThread([&]() {
        map.remove(10);
    });
    removerThread.join();
    printf("Removal done\n");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // checks that the object is not removed
    myassert(checkVal == 0);
    printf("Checked not removed\n");
    // this line will update the local clock in current thread. It indicates that the threads acknowledged the deletion
    map.set(30, Checker{nullptr});
    std::this_thread::sleep_for(std::chrono::seconds(1));
    myassert(checkVal == 123);
    printf("Successfully removed\n");
    done = true;
    gcThread.join();
}

int main()
{
    removalTest();
    using RemovableMap = ConHashMap<PolicyCanRemove, int, int>;
    using NonRemovableMap = ConHashMap<PolicyNoRemove, int, int>;
    printf("Testing NonRemovableMap\n");
    randomTest<2, PolicyNoRemove>([](NonRemovableMap &map) {}, [](NonRemovableMap &map, int idx) {});

    printf("Testing RemovableMap\n");
    for (int i = 0; i < bufsize; i++)
    {
        vec[i] = 0;
    }
    randomTest<3, PolicyCanRemove>([](RemovableMap &map) { map.garbageCollect(); }, [](RemovableMap &map, int idx) { map.remove(idx); });
}