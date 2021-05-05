# Kuai - A concurrent hash map

Kuai is a concurrent hash map implementation based on segmented lock. It is aimed at read-biased workloads for multi-threaded hash map accesses.

Read accesses on the map will not require locking, while write operations like `set` and `setIfAbsent` will acquire the bucket-wise lock. Thus, reading on the hash map is non-blocking and writing on the hash map is blocked only if there are writes on the same bucket at the same time.

## Usage

Kuai provides removable and non-removable concurrent hash map implementations. The non-removable hash map cannot remove any key-value pair in the map, but it has a better performance than removable hash map.

Declaration of the hash map template:

```C++
template <typename BucketPolicy, typename K, typename V, typename Hasher = std::hash<K>, typename Comparer = std::equal_to<K>>
struct ConHashMap;
```

Kuai provides two BucketPolicies: `PolicyNoRemove` and `PolicyCanRemove` which correspond to non-removable and removable hash maps respectively.

For example, to define a removable hash map for `int` as key and `float` as value, one can write:

```C++
using namespace Kuai;
ConHashMap<PolicyCanRemove, int, float> map(1024);
```

The constructor of `ConHashMap` requires an integer as the initial bucket number of the map.

Kuai provides the `get` and `set` methods to access the mapped values by the keys:

```C++
...
struct ConHashMap {
    V *get(const K &k);

    template <typename VType>
    void set(const K &k, VType &&v);
};
```

The `get` method accepts the key to find and returns the pointer of the value. If the key is not found in the map, a `nullptr` will be returned.

The `set` methods accepts the key and the value to set. Note that the value can be rvalue (e.g. value of `std::move()`) or a lvalue.

For example, in the `map` previously defined in the example, one can access the map by:

```C++
map.set(123, 1.23f);
float* value = map.get(123);
std::cout<<*value; // should be 1.23 if no other threads has access to the map
```

Kuai also provides the `setIfAbsent` method, which inserts a key-value pair only if the key is not yet found in the map.

```C++
V *setIfAbsent(const K &k, VType &&v);
```

For removable maps, Kuai provides the `remove` and `collectGarbage` methods. Note that in muti-threaded environments, it is much more complicated to remove key-value pair and free the memory buffer of it, because it may be the case that one thread destorys a key-value node while another thread is reading it. Kuai introduces a mechanism to ensure that the hash map frees and destroy a key-value pair only when other threads will no longer have access to it.

Thus, when `remove` is called, it will not immediately free the key-value pair. The pair destruction is conducted in the `collectGarbage` method. `collectGarbage` can be called in any time and any thread to safely free the key-value pairs that have been already marked `removed`.

## Performance

Tested on Intel(R) Core(TM) i7-7700HQ CPU @ 2.80GHz, WSL2 on Win10. Ubuntu 20.04 in Docker.
Number of threads = 4. gcc version 9.3.0 (Ubuntu 9.3.0-17ubuntu1~20.04). Optimized with `-O2`.

TBB (Intel Thread Building Blocks) version 2021.2.0-357. Testing int-int mapping.

| Test name | Removable   | NonRemovable  | tbb::concurrent_hash_map | std::unordered_map + RW lock| std::unordered_map (no lock)|
|  ----     |  ----       | ----          | ----  | ----  | ----  |
| 500000 gets/sets. 20% of total is `get`  | 110   | 102 | 109 | 1846| N/A |
| 500000 gets/sets. 80% of total is `get`  | 59   | 52 | 112 | 2226| N/A|
| 500000 gets  | 40   | 36 | 106 |220| 202|
| Reading the same key (5000000 times)  | 21   | 4 | N/A | 2031| 8 |

## Node removal implementation

Kuai adpots the Quiescent State Based Reclamation when removing a key-value from the map. A global logical clock is used to mark the number of deletions issued by `remove()`. In every thread, there is a thread-local logical clock to mark the deletion event it has already observed. The key-value pairs are stored in the nodes in linked lists of a hash map. Every node has also a `deletionTick` field to store the logical clock when it is removed. If a node has not been removed, the field should be zero.

When a key-value pair is removed from the map, the linked list node will be firstly detached from the hash map. Kuai will then atomically increase the global clock by 1 and set the `deletionTick` in the node to the new value of the global clock. This indicates that a node is removed by a thread and the deletion event may not be seen by other threads (because in other threads, thread-local clocks is less than the global clock that has just been updated by us).

When other threads access the map, they should first update the thread-local clocks by loading the global clock and saving the new clock value to the thread-local clocks. Note that we add memory barriers to the loads and stores of the global clock, so that we can ensure the memory ordering before and after the accesses to the clock - If a thread's local clock is greater than or equal to a node's `deletionTick`, the thread acknowledges that the node has been deleted. Since the detaching of the node was performed before the setting of the `deletionTick` and the update of global clock, the memory barrier when reading the global clock in another thread makes sure that the thread must be aware that the node is inaccessible from the linked list. There are happens-before relations:

Thread1: detach node from list -> Thread1: update global clock = 1234 -> Thread2: read global clock = 1234

Thus, if all thread's local clock is no less than a node's `deletionTick`, it can be free'd because no thread will have the access to it via the linked list in the hash map.

To conclude, when `remove()` is called, Kuai will mark the `deletionTick` of the node and move it to the deletion queue (Note that the deletion queue is mutex-protected for simplicity of implementation). `collectGarbage()` can be safely called in any threads any time to try to really free the nodes in the deletion queue by checking the `deletionTick`.


