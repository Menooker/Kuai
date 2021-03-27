#pragma once
#include "SpinLock.hpp"
#include "ListNode.hpp"
#include "Option.hpp"
#include <stdint.h>
#include <utility>

namespace Kuai
{
    template <typename K, typename V, typename Hasher = std::hash<K>, typename Comparer = std::equal_to<K>>
    struct ConHashMap
    {
        struct PairType
        {
            K k;
            V v;
        };
        typedef ListNode<PairType> HashListNode;
        struct Bucket
        {
            std::atomic<HashListNode *> ptr = {nullptr};
            SpinLock writeLock;
        };

        Bucket *buckets;
        unsigned bucketNum;
        Hasher hasher;
        Comparer cmper;

    private:
        Bucket &getBucket(const K &k)
        {
            uint32_t hashv = hasher(k);
            return buckets[hashv % bucketNum];
        }

        HashListNode *makeNewNode(const K &k, V &&v, HashListNode *next)
        {
            HashListNode *ret = new HashListNode();
            ret->next = next;
            ret->data.k = k;
            ret->data.v = std::move(v);
            return ret;
        }

        HashListNode *findNode(HashListNode *headNode, const K &k)
        {
            while (headNode)
            {
                if (cmper(headNode->data.k, k))
                {
                    return headNode;
                }
                headNode = headNode->next.load();
            }
            return nullptr;
        }

    public:
        ConHashMap(size_t numBuckets)
        {
            buckets = new Bucket[numBuckets];
            bucketNum = numBuckets;
        }

        ~ConHashMap()
        {
            for (size_t i = 0; i < bucketNum; i++)
            {
                HashListNode *cur = buckets[i].ptr.load();
                while (cur)
                {
                    auto next = cur->next.load();
                    delete cur;
                    cur = next;
                }
            }
            delete[] buckets;
        }

        Option<V> get(const K &k)
        {
            HashListNode *cur = findNode(getBucket(k).ptr.load(), k);
            if (cur)
            {
                return Option<V>(V(cur->data.v));
            }
            return Option<V>();
        }

        void set(const K &k, V &&v)
        {
            Bucket &buck = getBucket(k);
            std::lock_guard<SpinLock> guard(buck.writeLock);
            HashListNode *cur = buck.ptr.load();
            HashListNode *headNode = cur;
            cur = findNode(cur, k);
            if (cur)
            {
                cur->data.v = std::move(v);
                return;
            }
            buck.ptr.store(makeNewNode(k, std::move(v), headNode));
        }

        Option<V> setIfAbsent(const K &k, V &&v)
        {
            Bucket &buck = getBucket(k);
            std::lock_guard<SpinLock> guard(buck.writeLock);
            HashListNode *cur = buck.ptr.load();
            HashListNode *headNode = cur;
            cur = findNode(cur, k);
            if (cur)
            {
                return Option<V>(V(cur->data.v));
            }
            buck.ptr.store(makeNewNode(k, std::move(v), headNode));
            return Option<V>();
        }
    };
} // namespace Kuai