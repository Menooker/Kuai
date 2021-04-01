#pragma once
#include "SpinLock.hpp"
#include "ListNode.hpp"
#include <stdint.h>
#include <utility>

namespace Kuai
{

    struct PolicyNoRemove
    {
        struct DummyLock
        {
            void lock() {}
            void unlock() {}
        };
        static constexpr bool canRemove = false;
        using BucketLock = SpinLock;
        using BucketReadLock = DummyLock;
        using BucketWriteLock = SpinLock;
        static DummyLock getReadLockRef(SpinLock &l) { return DummyLock(); }
        static SpinLock &getWriteLockRef(SpinLock &l) { return l; }
    };

    struct PolicyCanRemove
    {
        static constexpr bool canRemove = true;
        using BucketLock = SpinRWLock;
        using BucketReadLock = SpinRWLock::ReadLock;
        using BucketWriteLock = SpinRWLock::WriteLock;
        static BucketReadLock getReadLockRef(SpinRWLock &l) { return l.read(); }
        static BucketWriteLock getWriteLockRef(SpinRWLock &l) { return l.write(); }
    };

    template <typename BucketPolicy, typename K, typename V, typename Hasher = std::hash<K>, typename Comparer = std::equal_to<K>>
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
            typename BucketPolicy::BucketLock bucketLock;
        };

        Bucket *buckets;
        unsigned bucketNum;
        Hasher hasher;
        Comparer cmper;

    private:
        using BucketReadLock = typename BucketPolicy::BucketReadLock;
        using BucketWriteLock = typename BucketPolicy::BucketWriteLock;
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

        HashListNode *makeNewNode(const K &k, const V &v, HashListNode *next)
        {
            HashListNode *ret = new HashListNode();
            ret->next = next;
            ret->data.k = k;
            ret->data.v = v;
            return ret;
        }

        HashListNode *findNode(HashListNode *headNode, const K &k, HashListNode *&prevNode)
        {
            prevNode = nullptr;
            while (headNode)
            {
                if (cmper(headNode->data.k, k))
                {
                    return headNode;
                }
                prevNode = headNode;
                headNode = headNode->next;
            }
            return nullptr;
        }

        HashListNode *findNode(HashListNode *headNode, const K &k)
        {
            HashListNode *prevNode;
            return findNode(headNode, k, prevNode);
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
                HashListNode *cur = buckets[i].ptr;
                while (cur)
                {
                    auto next = cur->next;
                    delete cur;
                    cur = next;
                }
            }
            delete[] buckets;
        }

        V *get(const K &k)
        {
            Bucket &buck = getBucket(k);
            auto &&rlock = BucketPolicy::getReadLockRef(buck.bucketLock);
            std::lock_guard<BucketReadLock> guard(rlock);
            HashListNode *cur = findNode(buck.ptr, k);
            if (cur)
            {
                return &cur->data.v;
            }
            return nullptr;
        }

        template <typename VType>
        void set(const K &k, VType &&v)
        {
            Bucket &buck = getBucket(k);
            auto &&wlock = BucketPolicy::getWriteLockRef(buck.bucketLock);
            std::lock_guard<BucketWriteLock> guard(wlock);
            HashListNode *cur = buck.ptr;
            HashListNode *headNode = cur;
            cur = findNode(cur, k);
            if (cur)
            {
                cur->data.v = std::forward<VType>(v);
                return;
            }
            buck.ptr = makeNewNode(k, std::forward<VType>(v), headNode);
        }

        template <typename Dummy = BucketPolicy>
        typename std::enable_if<Dummy::canRemove>::type remove(const K &k)
        {
            Bucket &buck = getBucket(k);
            auto &&wlock = BucketPolicy::getWriteLockRef(buck.bucketLock);
            std::lock_guard<BucketWriteLock> guard(wlock);
            HashListNode *cur = buck.ptr;
            HashListNode *headNode = cur;
            HashListNode *prevNode;
            cur = findNode(cur, k, prevNode);
            if (cur)
            {
                if (prevNode)
                {
                    prevNode->next = cur->next;
                }
                else
                {
                    buck.ptr = cur->next;
                }
                delete cur;
                return;
            }
            throw std::runtime_error("Cannot find the key!");
        }

        template <typename VType>
        V *setIfAbsent(const K &k, VType &&v)
        {
            Bucket &buck = getBucket(k);
            auto &&wlock = BucketPolicy::getWriteLockRef(buck.bucketLock);
            std::lock_guard<BucketWriteLock> guard(wlock);
            HashListNode *cur = buck.ptr;
            HashListNode *headNode = cur;
            cur = findNode(cur, k);
            if (cur)
            {
                return &cur->data.v;
            }
            buck.ptr = makeNewNode(k, std::forward<VType>(v), headNode);
            return nullptr;
        }
    };
} // namespace Kuai