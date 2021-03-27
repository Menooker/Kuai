#pragma once
#include <atomic>
namespace Kuai
{

    template <typename T>
    struct ListNode
    {
        using ptr_t = std::atomic<ListNode<T> *>;
        ptr_t next;
        T data;
    };
} // namespace Kuai