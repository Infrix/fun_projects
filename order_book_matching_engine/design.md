# Order Book & Matching Engine Project
## Background
A matching engine is a component that allows the matching of buy and sell orders inside an exchange. When an exchange receives a new order, it is considered ‘active’, and it will first try to match the active order against existing orders, called ‘resting’ orders. In case the exchange cannot match the active order against any resting orders, it will store the active order in an order book, so that it can potentially match it later. When an order is added to the order book, it is no longer considered ‘active’ and is now considered ‘resting’.

The exchange will match orders using the price-time priority rule. Order matching only happens between active orders (new orders not added to the order book yet), and resting orders (orders that have already been added to the order book). This rule for matching an active order with a resting order on an exchange is expressed using the following conditions – which must all be true for the matching to happen:

- The side of the two orders must be different (i.e. a buy order must match against a sell orders or vice versa).
- The instrument of the two orders must be the same (i.e. an order for “GOOG” must match against another order for “GOOG”).
- The size of the two orders must be greater than zero.
- The price of the buy order must be greater or equal to the price of the sell order.

In case an active order can be matched with multiple resting orders, the resting order with the best price is matched first. Intuitively, as a buyer, you would want to buy for the cheapest amount; as a seller, you would want to sell for the most money. So, an active buy order matches with the lowest-priced resting sell order, while an active sell order matches with the highest-priced resting buy order.

If there are still multiple matchable orders with the best price, the resting order that was added to the order book the earliest (i.e. the order whose “added to order book” log has the earliest timestamp) will be matched first. This is to ensure fairness towards the orders that have waited the longest.

If an active order cannot match with any resting order, it may be added to the order book, at which point it becomes a resting order.

Orders can be partially matched in case the size of the other order is lower. Consequently, orders can be matched multiple times. Fully filled resting orders must be removed from the order book.

### Requirements
- Order ID: a unique ID across all orders
- Instrument: the instrument i.e. symbol of the order, up to 8 characters
- Price: the price of the order
- Count: the size of the order


Engine should handle these input commands:
- New buy or sell order with arguments: Order ID, Instrument, Price, Count
- Cancel order with argument: Order ID

In response to input commands, the following outputs should be generated.
- Order added to order book
```
output = {
  order_id,         // the order ID
  instrument,       // the instrument name
  price,            // the order's price
  count,            // the order's size
  side,             // the side (buy or sell)
  timestamp,        // the timestamp when the order was added to the book
};
```

- Order executed
```
output = {
  resting_order_id, // the order ID of the resting order
  new_order_id,     // the order ID of the new (incoming) order
  execution_id,     // the resting order's execution id (see below)
  price,            // the price that the orders executed at
  count,            // the number of units traded
  timestamp,        // the timestamp when the order was executed
};
```

The execution price is always the price of the resting order.

An order’s execution ID is a number starting from 1, incrementing serially each time that resting order is matched. E.g. if there is a resting order with ID 123, and it is matched three times, then there should be executions with (resting_order_id, execution_id) of (123, 1), (123, 2), and (123, 3).

- Order deleted
```
output = {
  order_id,         // the order ID of the (potentially) cancelled order
  cancel_state,     // the result of the cancel
                    // (either accepted or rejected)
  timestamp,        // the timestamp that the cancel
                    // was either accepted or rejected
};
```
If the order was successfully deleted, then the cancel state should be Accepted (A). Otherwise, if the order was already deleted or fully executed (remaining size 0), the state should be Rejected (R).

When a new order is created, the engine should first try to match it against one or more existing (resting) orders. When that happens, the exchange matching engine should first write the execution message for those matches, and if the active order is still not fully filled when it can no longer match any more orders, only then should the engine generate output for adding the order to the order book.

# Solution

## Description of Data Structures

OrderBookSide is a data structure that represents a side of an order book for each instrument, keeping track of a single side of resting orders for that instrument (e.g. all resting buy orders for GOOG). For each instrument, there are two OrderBookSide, one for buy orders and one for sell orders. OrderBookSide is implemented using a set that is sorted by price-time accordingly for a SELL or BUY book. Specifically, a std::set is used as it is implemented as a Red-Black Tree under the hood. This allows one to exploit the sorted property to efficiently retrieve the best respective BUY or SELL resting order as it will be at the beginning of the set. This also allows for efficient insertion of an order into the order book as a resting order.

ConcurrentHashMap<std::string, InstrumentOrderBooks*> instrument_order_books: 
This data structure is a global map of instrument name to a pointer to InstrumentOrderBooks, which holds the corresponding instrument’s pair of OrderBookSide (the buy and sell books). This allows the engine to retrieve the correct order book for matching and adding resting orders whenever an active order from a client comes in. This is implemented using a custom concurrent hashmap, which locks at a smaller granularity instead of locking the entire hashmap for each operation. In this case, each bucket of the hashmap has a corresponding mutex, which allows for concurrent operations to be executed on different buckets. The concurrent hashmap is implemented using  a std::vector<std::list<Node>> buckets which represents the buckets, where each Node is a key-value pair. The mutexes are stored in a std::vector<std::shared_mutex> mutexes, where each mutexes[i] corresponds to the mutex for  buckets[i].

ConcurrentHashMap<unsigned int,OrderMapping> order_id_map: 
Lastly, since we have to account for cancellation of orders from clients, we have to retrieve the respective order book that contains the given order id. To do this, we again maintain a global mapping from order id to the respective order book, where OrderMapping is just a tuple of a pointer to the corresponding InstrumentOrderBooks and the order side so we can retrieve the correct side of the instrument’s order book.. To implement such a mapping, the custom concurrent hashmap is reutilised, allowing one to efficiently retrieve the respective order book for a cancelled order to run the order cancellation logic. 

## Synchronisation Primitives
For each instrument’s order books (instrument_order_books), making it thread-safe was trivial. A std::mutex was simply used per instrument, so that each instrument would be locked when matching active orders, adding resting orders and cancelling orders.

The concurrent hashmap has a more non-trivial implementation as a more fine-grained locking approach is adopted. The mappings are partitioned into buckets, where each bucket is guarded by a std::shared_mutex. For retrievals, a shared_lock is used so that multiple readers can read a bucket concurrently. For insertions, a unique_lock is used for exclusive write access to update buckets. As each bucket has its own shared mutex, multiple threads can be writing to different buckets at the same time.

## Level of Concurrency
This implementation adopts Instrument-level concurrency. This means that orders from different instruments can execute concurrently. However, orders belonging to the same instrument would execute in serial order.

Since each instrument’s order book, which is represented by an InstrumentOrderBooks, has a single mutex, this means that the entire order book is locked for any operation (matching, adding, cancelling). As such, orders execute in serial order. 

On the otherhand, when adding a new instrument, we only occasionally lock existing instruments. This is because the concurrent hashmap implementation uses a shared mutex for each bucket. Hence, in the scenarios where there is a hash collision when a new instrument is hashed to an existing instrument’s bucket , some existing instruments have to be locked to insert a new instrument into the bucket to maintain our global instrument mapping. We can reduce the chance of this happening by utilising a higher number of buckets and an appropriate hash function to reduce hash collisions. In a scenario where all instruments were hashed to different buckets, then no existing instrument would have to be locked when adding a new instrument, which maximizes concurrency.

## Future Devleopment
Implement a more advanced level of concurrency, where for the same instrument, one buy and one sell order can execute concurrently.
