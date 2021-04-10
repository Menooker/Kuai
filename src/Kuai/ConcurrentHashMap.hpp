#pragma once
#include "SpinLock.hpp"
#include "ListNode.hpp"
#include "LogicalClock.hpp"
#include <stdint.h>
#include <utility>

namespace Kuai
{

    struct PolicyNoRemove
    {
        static void updateLocalClock()
        {
        }
        struct DeletionFlag
        {
            constexpr bool isDeleted()
            {
                return false;
            }
        };
        struct DeletionQueue
        {
            typedef void (*Deleter)(DeletionFlag *);
            DeletionQueue(Deleter v) {}
        };
        static constexpr bool canRemove = false;
    };

    struct PolicyCanRemove
    {
        static constexpr bool canRemove = true;
        static void updateLocalClock()
        {
            // sync local clock with global clock, indicating this core has seen the events with logical tick
            // less than the current global clock
            ThreadClock::updateLocalClock();
        }

        struct DeletionFlag
        {
            std::atomic<uint64_t> deleteTick = {0};
            void markDeleted()
            {
                // push up the global clock, indicating there is a new event that may not be seen by other cores
                deleteTick.store(++GlobalClock::clock.logicalClock);
            }

            bool readyToDelete()
            {
                return GlobalClock::clock.get_min_lock() >= deleteTick.load();
            }
            bool isDeleted()
            {
                // It is safe if a thread checks isDeleted before another thread marks it deleted. Since the global clock is increased,
                // by node deletion and the thread has not yet updated the local lock, the thread's local lock will be less than the
                // deleteTick
                return deleteTick.load();
            }
        };

        struct DeletionQueue
        {
            std::mutex lock;
            std::vector<DeletionFlag *> queue;
            typedef void (*Deleter)(DeletionFlag *);
            Deleter deleter;
            DeletionQueue(Deleter deleter) : deleter(deleter) {}
            void enqueue(DeletionFlag *p)
            {
                std::lock_guard<std::mutex> guard(lock);
                queue.emplace_back(p);
            }

            void doGC()
            {
                std::lock_guard<std::mutex> guard(lock);
                for (auto itr = queue.begin(); itr != queue.end();)
                {
                    if ((*itr)->readyToDelete())
                    {
                        deleter(*itr);
                        itr = queue.erase(itr);
                    }
                    else
                    {
                        ++itr;
                    }
                }
            }
            ~DeletionQueue()
            {
                for (auto &p : queue)
                {
                    deleter(p);
                }
            }
        };
    };

    template <typename BucketPolicy, typename K, typename V, typename Hasher = std::hash<K>, typename Comparer = std::equal_to<K>>
    struct ConHashMap : private BucketPolicy::DeletionQueue
    {
        struct HashListNode : public BucketPolicy::DeletionFlag
        {
            K k;
            V v;
            HashListNode *next;
        };

        struct Bucket
        {
            std::atomic<HashListNode *> ptr = {nullptr};
            SpinLock bucketLock;
        };

        Bucket *buckets;
        unsigned bucketNum;
        Hasher hasher;
        Comparer cmper;

    private:
        Bucket &getBucket(const K &k)
        {
            BucketPolicy::updateLocalClock();
            uint32_t hashv = hasher(k);
            return buckets[hashv % bucketNum];
        }

        HashListNode *makeNewNode(const K &k, V &&v, HashListNode *next)
        {
            HashListNode *ret = new HashListNode();
            ret->next = next;
            ret->k = k;
            ret->v = std::move(v);
            return ret;
        }

        HashListNode *makeNewNode(const K &k, const V &v, HashListNode *next)
        {
            HashListNode *ret = new HashListNode();
            ret->next = next;
            ret->k = k;
            ret->v = v;
            return ret;
        }

        HashListNode *findNode(std::atomic<HashListNode *> &head, const K &k, HashListNode *&prevNode)
        {
            for (;;)
            {
                prevNode = nullptr;
                HashListNode *headNode = head.load(); // reload head node if we met a deleted node
                bool retry = false;
                while (headNode)
                {
                    if (headNode->isDeleted())
                    {
                        retry = true;
                        break;
                    }
                    if (cmper(headNode->k, k))
                    {
                        return headNode;
                    }
                    prevNode = headNode;
                    headNode = headNode->next;
                }
                if (!retry)
                    return nullptr;
            }
        }

        HashListNode *findNode(std::atomic<HashListNode *> &headNode, const K &k)
        {
            HashListNode *prevNode;
            return findNode(headNode, k, prevNode);
        }
        static void nodeDeleter(typename BucketPolicy::DeletionFlag *node)
        {
            delete static_cast<HashListNode *>(node);
        }

    public:
        ConHashMap(size_t numBuckets) : BucketPolicy::DeletionQueue(nodeDeleter)
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
            HashListNode *cur = findNode(buck.ptr, k);
            if (cur)
            {
                return &cur->v;
            }
            return nullptr;
        }

        template <typename VType>
        void set(const K &k, VType &&v)
        {
            Bucket &buck = getBucket(k);
            std::lock_guard<SpinLock> guard(buck.bucketLock);
            HashListNode *headNode = buck.ptr;
            HashListNode *cur = findNode(buck.ptr, k);
            if (cur)
            {
                cur->v = std::forward<VType>(v);
                return;
            }
            buck.ptr = makeNewNode(k, std::forward<VType>(v), headNode);
        }

        template <typename Dummy = BucketPolicy>
        typename std::enable_if<Dummy::canRemove>::type remove(const K &k)
        {
            Bucket &buck = getBucket(k);
            std::lock_guard<SpinLock> guard(buck.bucketLock);
            HashListNode *prevNode;
            HashListNode *cur = findNode(buck.ptr, k, prevNode);
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
                cur->markDeleted();
                this->enqueue(cur);
                return;
            }
            throw std::runtime_error("Cannot find the key!");
        }

        template <typename Dummy = BucketPolicy>
        typename std::enable_if<Dummy::canRemove>::type garbageCollect()
        {
            this->doGC();
        }

        template <typename VType>
        V *setIfAbsent(const K &k, VType &&v)
        {
            Bucket &buck = getBucket(k);
            std::lock_guard<SpinLock> guard(buck.bucketLock);
            HashListNode *headNode = buck.ptr;
            HashListNode *cur = findNode(buck.ptr, k);
            if (cur)
            {
                return &cur->v;
            }
            buck.ptr = makeNewNode(k, std::forward<VType>(v), headNode);
            return nullptr;
        }
    };
} // namespace Kuai