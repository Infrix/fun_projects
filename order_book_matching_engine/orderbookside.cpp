#include "order.hpp"
#include "io.hpp"
#include "orderbookside.hpp"

void OrderBookSide::matchOrder(Order &active_order) {

    while (!resting_orders.empty() && active_order.count > 0) {
        auto best_resting_order_it = resting_orders.begin();
        if ((side == BUY && active_order.price > best_resting_order_it->price)
        || (side == SELL && active_order.price < best_resting_order_it->price)) {
            // for buy book this means that active sell is larger than highest buy
            // for sell book this means that active buy is smaller than lowest sell
            // hence, no match and break
            break;
        }
        // match possible
        unsigned int executed_qty = 0;
        Order best_resting_order = *best_resting_order_it;

        unsigned int trade_qty = std::min(active_order.count, best_resting_order.count);
        active_order.count -= trade_qty;
        executed_qty += trade_qty;

        // Order executed
        Output::OrderExecuted(best_resting_order.order_id, active_order.order_id, best_resting_order.execution_id, best_resting_order.price, executed_qty, getCurrentTimestamp());

        if (executed_qty == best_resting_order.count) {
            resting_orders.erase(best_resting_order_it);
        } else {
            // due to immutability, we cannot simply modify element
            // not allowed on standard iterator
            resting_orders.erase(best_resting_order_it);
            best_resting_order.count -= trade_qty;
            best_resting_order.execution_id++;
            resting_orders.insert(best_resting_order);
        }
    }
}

void OrderBookSide::addOrder(Order &order) {
    // fully matched order
    if (order.count == 0) {
        return;
    }

    order.timestamp = getCurrentTimestamp();
    resting_orders.insert(order);
    
    // Order added
    Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.count, side == SELL, getCurrentTimestamp());
}

void OrderBookSide::cancelOrder(unsigned int order_id) {
    for (auto it = resting_orders.begin(); it != resting_orders.end(); it++) {
        if (it->order_id == order_id) {
            resting_orders.erase(it);

            // Order deleted (accepted)
            Output::OrderDeleted(order_id, true, getCurrentTimestamp());

            return;
        }
    }
    // Order deleted (rejected)
    Output::OrderDeleted(order_id, false, getCurrentTimestamp());
}
