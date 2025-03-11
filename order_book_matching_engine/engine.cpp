#include <iostream>
#include <thread>

#include "io.hpp"
#include "engine.hpp"
#include "concurrenthashmap.hpp"

// maximum instruments --> 26^8 = 208827064576
// large prime number to reduce hash collisions
constexpr size_t NUM_BUCKETS_INSTRUMENT = 8'388'607;
// Global mapping: instrument -> pair of OrderBookSide (first: BUY, second: SELL)
static ConcurrentHashMap<std::string, InstrumentOrderBooks*> instrument_order_books(NUM_BUCKETS_INSTRUMENT);

// Global mapping: order_id -> order book that contains it
// maximum keys --> 2^32 = 4,294,967,296
// large prime number to reduce hash collisions
const size_t NUM_BUCKETS_ORDER = 67'108'863;
using OrderMapping = std::pair<InstrumentOrderBooks*, OrderType>;
static ConcurrentHashMap<unsigned int, OrderMapping> order_id_map(NUM_BUCKETS_ORDER);

InstrumentOrderBooks* getInstrumentOrderBooks(const std::string &instrument)
{
	auto opt_books = instrument_order_books.get(instrument);
	if (!opt_books.has_value()) {
		InstrumentOrderBooks* new_books = new InstrumentOrderBooks();
		InstrumentOrderBooks* safe_book = instrument_order_books.insert(instrument, new_books);
		return safe_book;
	}
	return opt_books.value();
}

void registerOrderInMap(const Order &order, InstrumentOrderBooks *books) {
	if (order.count == 0) return;
	OrderMapping mapping = std::make_pair(books, order.side);
	order_id_map.insert(order.order_id, mapping);
}

void Engine::accept(ClientConnection connection)
{
	auto thread = std::thread(&Engine::connection_thread, this, std::move(connection));
	thread.detach();
}


void Engine::connection_thread(ClientConnection connection)
{
	while(true)
	{
		ClientCommand input {};
		switch(connection.readInput(input))
		{
			case ReadResult::Error: SyncCerr {} << "Error reading input" << std::endl;
			case ReadResult::EndOfFile: return;
			case ReadResult::Success: break;
		}
		// Functions for printing output actions in the prescribed format are
		// provided in the Output class:
		switch(input.type)
		{
			case input_cancel: {
				auto optMapping = order_id_map.get(input.order_id);
				if (optMapping.has_value()) {
					OrderMapping mapping = optMapping.value();
					InstrumentOrderBooks* books = mapping.first;
					OrderType side = mapping.second;
					books->cancelOrder(input.order_id, side);
				} else {
					// example 5 actually leads to this branch, possible for a cancel to come earlier than the order id
					Output::OrderDeleted(input.order_id, false, getCurrentTimestamp());
				}
				break;
			}

			default: {
				OrderType order_type = (input.type == input_buy) ? BUY : SELL;
				Order active_order = Order(input.order_id, input.instrument, input.price, input.count, order_type);
				auto books = getInstrumentOrderBooks(input.instrument);
				books->matchAndAddOrder(active_order);
				registerOrderInMap(active_order, books);

				break;
			}
		}
	}
}