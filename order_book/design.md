# Limit Order Book Project
## Background
Contains prices and corresponding volumes with which people want to buy a given stock (one order book for each stock). Tracks all the open orders.
- Bid side represents open offers to buy
- Ask side represent open offers to sell
Efficiency is the most important, so we want to use extra space whenever possible to improve execution time

### Requirements
- If a client submits a buy or sell order that cannot be filled, it gets stored in order book
- Trades are made when highest bid >= lowest ask (spread has been crossed, where spread is the difference between the lowest ask and the highest bid) --> if one side has already been in the order book, and the other side initiates, the price at which the trade will execute at is the price in the book 
- Orders are executed at the best possible price first, and if many orders have the same price then the one that was submitted earliest is chosen

## OOP Design
### Order Book Interface
We use an interface as there are multiple possible implementations of an order book
```C++
placeOrder(order)
cancelOrder(orderId)
getVolumeAtPrice(price, buyingOrSelling)
```
place order --> takes an order object and either fills it or places it in the limit book, prints trades that have taken place
cancel order --> takes an order id and cancels it if it has not yet been filled, otherwise nothing happens
get volume at price --> gets the volume of open orders for either buying or selling side of order book at a particular price
Note: order book methods for now, attributes will come after we think about the algorithmic design

### Order Class
Encapsulate the state of an order
```C++
private int orderId
private datetime timestamp
private buyOrSellEnum side
private double price
private int volume
private string client
```
Along with corresponding getters and setters

## Algorithm Design
Data Structures for Limit Order
### Buy order
```C++
// pseudocode
    check lowest price of the sell side of order book
    while buyer still has volume left to fill
        if lowest price of sell side <= buy side
            execute trade
        else
            // no more sell side and unfilled volume still present
            add it to buyer heap
            break
```
#### Data structure for order book
Since the algorithm prioritises lowest selling price, we use a min heap to represent the sell side, so that we can quickly see the lowest price to buy shares at
Correspondingly, for the buy side, we use a max heap so that we can quickly see the highest price someone is willing to buy the share at
We use a heap for storing the order book for O(log n) inserts

However, since we can have multiple trades with the same price, our heap actually holds a bunch of queues for orders, so that we can track in order of timestamp. We use a queue instead of tracking both price and timestamp so that we have lesser nodes to track in the heap for efficiency

### Getting Volume
We need to be able to get volume, and it is important that this call happens as fast as possible. Without any extra memory, we would have to loop through every element of a queue to sum up its volume. Instead, we can maintain a hashmap that tracks the volume at each price, and increment / decrement the volume counter when orders are added and cancelled, giving us O(1) time to get volume

### Cancellations
We can actively cancel or lazily cancel
- Active way is to take the node representing an order and remove it from the queue
This will result in fewer nodes in the heap, but will incur time complexity whenever we do cancellation
Since we remove node from queue, it is usually O(n), but instead we can use doubly linked list with hashmap so that we can get the order node and remove it from the linked list in O(1) time, but removing queue from heap is still O(log n), assuming that we have a hashmap pointing to the queue in the heap
- Lazily mark order as cancelled
As we run through our queues, if we see an order is marked as cancelled, we just skip over it
Since we never remove anything from the middle of the heap, time complexity advantages
Also by not actively removing nodes, we have more nodes in the heap

## Order Book Implementation (again after reviewing algorithm design)
```C++
private minHeap bestAsk
private maxHeap bestBid
private hashmap<int (orderId), Order> orderMap
private hashmap<[double (price), buyOrSellEnum (side)], int (volume)> volumeMap
private hashmap<[double (price), buyOrSellEnum (side)], PriceQueue> queueMap

placeOrder(order)
cancelOrder(orderId)
getVolumeAtPrice(price, buyingOrSelling)
```

### Methods

#### placeOrder
```C++
// pseudocode
oppositeBook = order.side == buy ? bestAsk : bestBid
sameBook = order.side == buy ? bestBid : bestAsk
order = orderMap[order.orderId]

while order.volume > 0 && oppositeBook not empty && oppositeBook.peek().peek().price <= order.price
    otherOrder = oppositeBook.peek().peek()
    tradePrice = otherOrder.price, tradeVolume = min(order.volume, otherOrder.volume)
    otherOrder.volume -= tradeVolume
    orderVoume -= tradeVolume
    print("made by {otherOrder.client}, taken by {order.client}, {tradeVolume} shares @ {tradePrice}")

    if otherOrder.volume == 0
        cancel(otherOrder)

if order.volume > 0
    addOrderToBook(order, sameBook)
```

#### addOrderToBook(order, book)
```C++
// pseudocode
if not (order.price, order.side) in queueMap
    queueMap[(order.price, order.side)] = new DoublyLinkedList(order)
    volumeMap[(order.price, order.side)] = order.volume
else
    queueMap[(order.price, order.side)].push(order)
    volumeMap[(order.price, order.side)] += order.volume

```

#### cancelOrder(orderId)
```C++
// pseudocode
if orderId in orderMap
    order = orderMap[orderId]
    priceQueue = queueMap[(order.price, order.side)]
    priceQueue.remove(order) // O(1) time with doubly linked list and hashmap
    if priceQueue.size() == 0
        sameBook = order.side == buy ? bestBid : bestAsk
        sameBook.remove(priceQueue) // O(log n) removal
    volumeNode[(order.price, order.side)] -= order.volume
    orderMap.delete(orderId)
```

#### getVolumeAtPrice(price, side)
```C++
// pseudocode
return volumeMap[(price, side)] ? volumeMap[(price, side)] : 0
```

## Possible extensions to problem for future
Market orders --> Instead of limit orders, we have market orders, we do not specify a price, we just specify a volume to buy at
Trigger orders -->  Set to trigger once a trade executes above or below a certain price. We need to keep separate heaps for buy and sell trigger orders to determine if hte price threshold has been reached to trigger them, and if so start calling placeOrder with each triggered order

Run through algorithm as per normal, but when a trade is executed, we go through the heaps