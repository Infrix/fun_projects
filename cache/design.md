# Cache Project
## Background
A cache is a set of keys and values that is used to store the values of previous computation to save computation ttime

### Requirements
- Needs to be able to store keys and their corresponding values, as well as read, write and delete
- Needs to be able to evict certain elements of the cache when space is limited, based on a certain type of policy
## Design
### Cache
There are many possible ways a key-value store can be implemented, depending on the type of storage and eviction policy we use, we could be able to support additional methods such as getting ordered set of values by key or by time added. So we should use an interface to allow plug and play of different type of storages and eviction policies into our cache class and still have the same functionality, making it more extensible and maintainable.

**Cache Interface**
```C++
cache(storage_interface storage, eviction_policy_interface eviction_policy, int capacity)
get(key)
put(key, value)
delete(key, value)
```
### Storage
- Possible storage class implementations:
- Hashmap --> O(1) accesses and finds
- Treemap --> O(logn) accesses and finds, allows ordering of keys via BST
- Linked Hashmap --> O(1) accesses and finds, more space required for doubly linked list of keys, allows ordering of keys based on when they were inserted

### Eviction Policies
Possible eviction policies implementation:
- Random --> keep a set of keys and just choose a random one to evict
- FIFO --> keep a queue of keys and a set containing the keys, evict first element in queue, likely need hashmap that point to nodes in the event that key is deleted to remove from middle of queue
- LIFO --> keep a stack of keys and a set containing the keys, evict top element in stack, likely need hashmap that point to nodes in the event that key is deleted to remove from middle of queue
- LFU --> keep a min heap (priority queue) of nodes containing the key and count of usage stats for the key, and a hashmap from key to node to update node when key is used
- LRU --> use a linked list to be able to evict from the head, we also need a hashmap from keys to linked list nodes so that we can shift nodes to most recently used when it has been accessed. We also need to make it a doubly linked list so that we can delete the node (we will need access to the previous ndoe to delete it) before shifting it to the most recently used.