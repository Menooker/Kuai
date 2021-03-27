# Kuai - A concurrent hash map

Kuai is a concurrent hash map implementation based on segmented lock. It is aimed at read-biased workloads for multi-threaded hash map accesses.

Read accesses on the map will not require locking, while write operations like `set` and `setIfAbsent` will acquire the bucket-wise lock. Thus, reading on the hash map is non-blocking and writing on the hash map is blocked only if there are writes on the same bucket at the same time.