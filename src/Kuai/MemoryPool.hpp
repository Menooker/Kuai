#pragma once
#include "ListNode.hpp"
#include <stdlib.h>
namespace Kuai
{

    struct MemoryPool
    {
        using node_t = ListNode<char[0]>;
        typename node_t::ptr_t head = {nullptr};
        uint64_t objSize;
        uint64_t allocSize;
        node_t *pageList = nullptr;
        constexpr static node_t *EMPTY_PTR = nullptr;
        constexpr static node_t *LOCK_SUCCESS = (node_t *)1;
        constexpr static node_t *ALLOCATING = (node_t *)2;

        void do_batch_alloc()
        {
            auto pageSize = allocSize + sizeof(node_t);
            auto objNodeSize = objSize + sizeof(node_t);
            node_t *newpage = (node_t *)malloc(pageSize);
            newpage->next = pageList;
            pageList = newpage;
            uintptr_t curObj = (uintptr_t)newpage->data;
            while (curObj + objNodeSize <= (uintptr_t)newpage->data + allocSize)
            {
                auto newhead = (node_t *)curObj;
                newhead->next = head.load();
                head = newhead;
                curObj += objNodeSize;
            }
        }

        void *alloc()
        {
            node_t *cur;
            for (;;)
            {
                cur = head.load();
                if (cur == EMPTY_PTR)
                {
                    cur = ALLOCATING;
                    if (head.compare_exchange_weak(cur, EMPTY_PTR))
                    {
                        do_batch_alloc();
                    }
                    continue;
                }
                else if (cur == ALLOCATING || cur == LOCK_SUCCESS)
                {
                    continue;
                }
                if (head.compare_exchange_weak(cur, LOCK_SUCCESS))
                {
                    break;
                }
            }
            void *ret = cur->data;
            head = cur->next.load();
            return ret;
        }

        void dealloc(void *ptr)
        {
            auto node = (node_t *)(uintptr_t(ptr) - sizeof(node_t::ptr_t));
            node_t *cur;
            do
            {
                cur = head.load();
                node->next = cur;
            } while (!head.compare_exchange_weak(node, cur));
        }
    };
} // namespace Kuai