#ifndef ORDER_HPP
#define ORDER_HPP

#include <string>
#include <chrono>


enum OrderType { BUY, SELL };

class Order {
public:
    unsigned int order_id;
    std::string instrument;
    unsigned int price;
    unsigned int count;
    OrderType side;
    std::chrono::microseconds::rep timestamp;
    unsigned int execution_id = 1;

    Order(unsigned int order_id, std::string instrument, unsigned int price, unsigned int count, OrderType side) : order_id(order_id), instrument(instrument), price(price), count(count), side(side) {}
};

#endif