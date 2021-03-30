#include <Kuai/ConcurrentHashMap.hpp>
#include <utility>
#include <cassert>
#include <thread>
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
    seed = seed * 1236767 + 213141;
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

int main()
{
    ConHashMap<PolicyCanRemove, int, int> map(32 * 1024);
    std::atomic<int> reads = {0};
    std::atomic<int> writes = {0};
    auto runner = [&map, &reads, &writes](uint32_t seed) {
        for (int i = 0; i < 100000; i++)
        {
            auto idx = myrand(seed);
            auto action = idx / bufsize;
            idx = idx % bufsize;
            switch (action % 2)
            {
            case 0:
            {
                auto value = myrand_n0(seed);
                int newval = 0;
                if (vec[idx].compare_exchange_strong(newval, value))
                {
                    auto oldv = map.setIfAbsent(idx, value);
                    myassert(!oldv.hasData());
                    ++writes;
                    break;
                }
                // if the slot is set, fall through
            }
            case 1:
            {
                auto ret = map.get(idx);
                auto true_val = vec[idx].load();
                myassert(ret.hasData() == (true_val != 0));
                if (true_val)
                {
                    myassert(ret.get() == true_val);
                    auto oldv = map.setIfAbsent(idx, 2);
                    myassert(oldv.hasData());
                    myassert(oldv.get() == true_val);
                }
                ++reads;
            }
            break;

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
    printf("Done r=%d, w=%d\n", reads.load(), writes.load());
}