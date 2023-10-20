#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <filesystem>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "state-mgr.h"

constexpr  uint64_t LAMBDA_PHYS_START = UINT64_C(0);

using symbol_type = std::array<char, 10>;
using token_type = std::array<char, 10>;
using currency_type = int;
using qty_type = int;
using trader_type = std::array<char, 10>;
using id_type = int; 
using request_id_type = int;
enum class side_type : char { buy = 'B',  sell = 'S'  };
enum class exec_type : char {
    new_order = 'N',  // ack new order
    cancel = 'C',     // ack cancel
    execution = 'E',  // trade execution (partial or full)
    rejection = 'R'   // order rejection
};
using contract_address_type = std::string;
using request_type = char;

struct new_order_request  {
    id_type id;
    trader_type trader;
    symbol_type symbol;
    side_type side;
    currency_type price;
    qty_type qty;
};

struct deposit_request   {
    id_type id;
    trader_type trader;
    token_type token;
    currency_type amount;
};

struct withdraw_request  {
    id_type id;
    trader_type trader;
    token_type token;
    currency_type amount;
};

// Trading instrument
struct instrument {
    token_type base;    // token being traded
    token_type quote;   // quote token - "price " of 1 base token
};

//  Token exchange order
struct order {
    id_type id;             // order id provided by the exchange
    trader_type trader;     // trader id
    symbol_type symbol;     // instrument's symbol (ticker)
    side_type side;              // buy or sell
    currency_type price;    // limit price in instrument.quote
    qty_type qty;           // remaining quantity
    
    bool matches(const order& other) {
        return (side == side_type::buy && price >= other.price) || (side == side_type::sell && price <= other.price);
    } 

    bool is_filled()  {
        return  qty == 0;
    } 

};

// comparator for sorting bid offers
struct best_bid {
    bool operator()(const order &a, const order &b) const {
        return a.price > b.price;
    }
};

// comparator for sortting ask offers
struct best_ask {
    bool operator()(const  order &a, const order &b) const {
        return a.price < b.price;
    }
};

 using bids_type = std::multiset<order, best_bid, arena_allocator<order>>;
 using asks_type = std::multiset<order, best_ask, arena_allocator<order>>;

struct book {
    symbol_type symbol;
    bids_type bids;
    asks_type asks;
};

// exchange notifications
struct execution_report {
    trader_type trader;   // trader id
    id_type order_id;     // order id
    exec_type type;       // execution type
    side_type side;       // side of executed order
    qty_type qty;         // qtd executed
    currency_type price;   // execution price
    std::string text;     // optional text
};
using execution_reports_type = std::vector<execution_report>;

using wallet_type = std::map<token_type, currency_type, std::less<token_type>, arena_allocator<std::pair<const token_type, currency_type>>>;
using books_type = std::map<symbol_type, book, std::less<symbol_type>, arena_allocator<std::pair<const symbol_type, book>>>;
using wallets_type = std::map<trader_type, wallet_type, std::less<trader_type>, arena_allocator<std::pair<const trader_type, wallet_type>>>;    

std::map<token_type, contract_address_type> tokens;
std::map<token_type, instrument>  instruments;

void init_instruments() {
    tokens[token_type{"ctsi"}] =  contract_address_type{"0xdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef"};
    tokens[token_type{"usdc"}] =  contract_address_type{"0xdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef"};
    instruments[symbol_type{"ctsi/usdc"}] = instrument{token_type{"ctsi"}, token_type{"usdc"}};
}

// exchange class to be "deserialized" from lambda state
class exchange {
    books_type books;
    wallets_type wallets;
    id_type next_id{0};

public:    
   
    bool new_order(order o, execution_reports_type &reports) {
        // validate order
        auto instrument = instruments.find(o.symbol);
        if (instrument == instruments.end()) {
            reports.push_back({o.trader, o.id, exec_type::rejection, o.side, o.qty, o.price, "Invalid symbol"});
            return false;
        }
        if (o.side == side_type::buy) {        
            auto size = o.qty * o.price;;
            auto  balance = get_balance(o.trader, instrument->second.quote);
            if (balance < size) {
                reports.push_back({o.trader, o.id, exec_type::rejection, o.side, o.qty, o.price, "Insufficient funds"});
                return false;
            }
            subtract_from_balance(o.trader, instrument->second.quote, size);
        } else {
            auto size = o.qty;
            auto  balance = get_balance(o.trader, instrument->second.base);
            if (balance < size) {
                reports.push_back({o.trader, o.id, exec_type::rejection, o.side, o.qty, o.price, "Insufficient funds"});
                return false;
            }
            subtract_from_balance(o.trader, instrument->second.base, size);
        }        
        
        // send report acknowledging new order
        reports.push_back({o.trader, o.id, exec_type::new_order, o.side, o.qty, o.price});
        // match against existing orders
        auto &book = find_or_create_book(o.symbol);
        if (o.side == side_type::buy) {
            match(o, book.asks, instrument->second, reports);    
            if (!o.is_filled()) {
                book.bids.insert(o);
            }
        } else {
            match(o, book.bids, instrument->second, reports);    
            if (!o.is_filled()) {
                book.asks.insert(o);
            }
        }
        return true;
    }

    wallet_type* find_wallet(const trader_type &trader) {
        auto it = wallets.find(trader);
        if (it != wallets.end())  {
            return &it->second;
        }
        return nullptr;
    }

    book *find_book(const symbol_type &symbol) {
        auto it = books.find(symbol);
        if (it != books.end())  {
            return &it->second;
        }
        return nullptr;
    }

    void deposit(const trader_type &trader, const token_type &token, currency_type amount) {
        add_to_balance(trader, token, amount);
    }

    bool  withdraw(const trader_type trader, const token_type token, currency_type amount, std::string *error_message=nullptr) {
        if (get_balance(trader, token) < amount) {
            if(error_message) {
                *error_message = "insufficient funds";
            }
            
            return false;
        }
        subtract_from_balance(trader, token, amount);
        return true;
    }
    
private:

    book &find_or_create_book(const symbol_type &symbol) {
        auto it = books.find(symbol);
        if (it != books.end())  {
            return it->second;
        }
        return books.emplace(symbol, book{symbol, {}, {}}).first->second;;
    }   

    wallet_type &find_or_create_wallet(const trader_type &trader) {
        auto it = wallets.find(trader);
        if (it != wallets.end())  {
            return it->second;
        }
        return wallets.emplace(trader, wallet_type{}).first->second;;
    }

    void subtract_from_balance(const trader_type &trader, const token_type &token, currency_type amount) {
        if (tokens.find(token) == tokens.end()) {
            throw std::runtime_error("invalid token");
        }
        auto &wallet = find_or_create_wallet(trader);
        wallet[token] -= amount;
    }
    
    void add_to_balance(const trader_type &trader, const token_type &token, currency_type amount) {
        if (tokens.find(token) == tokens.end()) {
            throw std::runtime_error("invalid token");
        }
        auto &wallet = find_or_create_wallet(trader);
        wallet[token] += amount;
    }

    // match order against existing offers, executing trades and notifying both parties
    template <typename T>
    void match(order& o, T &offers, instrument &instr, execution_reports_type& reports) {
        auto it = offers.begin();
        while(it != offers.end()) {
            // ok to drop const becasue the set is ordered by a custom comparator whose key(price) won't be changed
            auto &best_offer = const_cast<order&>(*it);
            if (o.is_filled() || !o.matches(best_offer))  {
                return;
            }
            auto &buy_order =  o.side == side_type::buy ? o : best_offer;
            auto &sell_order = o.side == side_type::buy ? best_offer : o;
            auto &buyer = buy_order.trader;
            auto &seller = sell_order.trader;
            // execute trade and notify both parties
            auto exec_qty = std::min(o.qty, best_offer.qty);
            buy_order.qty -= exec_qty;
            sell_order.qty -= exec_qty;
            // exchange tokens
            auto exec_price = (o.price + best_offer.price) /2;
            add_to_balance(buyer, instr.quote, exec_qty * buy_order.price);    // add balance locked at the limit order price
            subtract_from_balance(buyer, instr.quote, exec_qty * exec_price);  // subtract balance at the execution price
            add_to_balance(buyer, instr.base, exec_qty);                       // add bought tokens
            subtract_from_balance(seller, instr.base, exec_qty);               // subtract sold tokens
            add_to_balance(seller, instr.quote, exec_qty * exec_price);        // add balance at the execution price
            // notify both parties
            reports.push_back({buyer, buy_order.id, exec_type::execution, side_type::buy, exec_qty, exec_price});
            reports.push_back({seller, sell_order.id, exec_type::execution, side_type::sell, exec_qty, exec_price});
            // remove offer from book if filled
            if (best_offer.is_filled()) {
                offers.erase(it);
            }
            // find next best offer
            it = offers.begin();
        }
    }

    currency_type get_balance(const trader_type &trader, const token_type &token) {
        auto wallet = find_wallet(trader);
        if (!wallet) {
            return 0;
        }
        auto it = wallet->find(token);
        if (it == wallet->end())  {
            return 0;
        }
        return it->second;
    }


    id_type get_next_id() {
        return ++next_id;
    }
};


