#pragma once
#include <atomic>
namespace Kuai
{

    template <typename T>
    struct ListNode
    {
        ListNode* next;
        T data;
    };
} // namespace Kuai