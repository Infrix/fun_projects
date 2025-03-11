#ifndef ORDERBOOKSIDE_HPP
#define ORDERBOOKSIDE_HPP

#include "order.hpp"

#include <set>
#include <chrono>

struct OrderPriorityComparator {
    OrderType side;
    OrderPriorityComparator() {};
    OrderPriorityComparator(OrderType side) : side(side) {};

    bool operator()(const Order& lhs, const Order& rhs) const {
        if (lhs.price != rhs.price) {
            return side == BUY ? (lhs.price > rhs.price) : (lhs.price < rhs.price);
        }
        return lhs.timestamp < rhs.timestamp;
    }
};

class OrderBookSide {
public:
    OrderBookSide(OrderType side) : side(side), resting_orders(OrderPriorityComparator(side)) {}
    void matchOrder(Order &active_order); 
    void addOrder(Order &order);
    void cancelOrder(unsigned int order_id);
private:
    OrderType side;
    std::set<Order, OrderPriorityComparator> resting_orders;

    inline std::chrono::microseconds::rep getCurrentTimestamp() noexcept
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
};

#endif

// Wrapper class to hold both sides of order books + single mutex
class InstrumentOrderBooks {
public:
    InstrumentOrderBooks() : buy_book(BUY), sell_book(SELL) {}
    inline void matchAndAddOrder(Order &active_order) {
        std::lock_guard<std::mutex> lock(mtx); // Single lock for match and add operations (to handle case of partial matches)

        if (active_order.side == BUY) {
            sell_book.matchOrder(active_order);
            if (active_order.count > 0) {
                buy_book.addOrder(active_order);
            }
        } else {
            buy_book.matchOrder(active_order);
            if (active_order.count > 0) {
                sell_book.addOrder(active_order);
            }
        }
    }
    OrderBookSide* getOrderBookSide(OrderType side) {
        return side == BUY ? &buy_book : &sell_book;
    }
    inline void cancelOrder(unsigned int order_id, OrderType side) {
        std::lock_guard<std::mutex> lock(mtx);
        getOrderBookSide(side)->cancelOrder(order_id);
    }
private:
    OrderBookSide buy_book;
    OrderBookSide sell_book;
    std::mutex mtx; // Single mutex for both buy & sell books combined
};