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

#define TEST_SYMBOL "ctsi/usdc"

constexpr  uint64_t LAMBDA_VIRTUAL_START = UINT64_C(0x1000000000);
constexpr  uint64_t LAMBDA_PHYS_START = UINT64_C(0);

class lambda_state {
    int m_fd;    
    uint64_t m_length = 0;    
    void *m_state;

public:
    lambda_state(const char *file_name) {
        m_length = std::filesystem::file_size(file_name);
        m_fd = open(file_name, O_RDWR);
        m_state = mmap((void*)LAMBDA_VIRTUAL_START, m_length,  PROT_WRITE | PROT_READ,  MAP_SHARED | MAP_NOEXTEND  | MAP_FIXED, m_fd, 0);
        if (m_state == MAP_FAILED) {
            throw std::runtime_error(std::string("mmap - ") + std::strerror(errno));
        }
        if (m_state != reinterpret_cast<void*>(LAMBDA_VIRTUAL_START)) {
            close(m_fd);
            throw std::runtime_error("mmap - unexpected address"); 
    }
    }

    lambda_state(uint64_t phys_start, uint64_t length) : m_length(length) {
        m_length = length;
        m_fd = open("/dev/mem", O_RDWR);
        m_state = mmap((void*)LAMBDA_VIRTUAL_START, m_length,  PROT_WRITE | PROT_READ,  MAP_SHARED | MAP_NOEXTEND  | MAP_FIXED, m_fd, phys_start);
        if (m_state == MAP_FAILED) {
            throw std::runtime_error(std::string("mmap - ") + std::strerror(errno));
        }
        if (m_state != reinterpret_cast<void*>(LAMBDA_VIRTUAL_START)) {
            close(m_fd);
            throw std::runtime_error("mmap - unexpected address"); 
        }
    }

    virtual ~lambda_state() {
        printf("munmap\n");
        munmap(m_state, m_length);
        close(m_fd);
    }

    uint64_t get_length() {
        return m_length;
    }
    void *get_state() {
        return m_state;
    }

};

/// start of custom allocator stuff

class memory_arena {
    uint64_t m_length;
    uint64_t m_next_free;
    unsigned char m_data[0];
public:
    memory_arena(uint64_t length) : m_length(length), m_next_free(0) { }
    void *get_data() {
        return m_data;
    }
    void *allocate(int length) {
        auto p = m_data + m_next_free;
        m_next_free += length;
        if (m_next_free > get_data_length()) {
            throw std::bad_alloc();
        }
        return p;
    }
    void deallocate(void *p, int length) {
    }
    uint64_t get_data_length() {
        return m_length - offsetof(memory_arena, m_data);
    }
    
};

static memory_arena *arena = nullptr;

template <typename T>
class arena_allocator {
public:
    arena_allocator() = default;
    using pointer = T*;
    using value_type = T;
    pointer allocate(std::size_t n) {
        return static_cast<pointer>(arena->allocate(n * sizeof(T)));    
    }
    void deallocate(pointer p, std::size_t n) noexcept {
        arena->deallocate(p, n * sizeof(T));
    }
};

// start of exchange types

using symbol_type = std::array<char, 10>;
using token_type = std::array<char, 10>;
using currency_type = int;
using qty_type = int;
using trader_type = std::array<char, 10>;
using id_type = int; 
enum class side : char { buy = 'B',  sell = 'S'  };
enum class exec_type : char {
    new_order = 'N',  // ack new order
    cancel = 'C',     // ack cancel
    execution = 'E',  // trade execution (partial or full)
    rejection = 'R'   // order rejection
};
using contract_address_type = std::string;

// Supported tokens
static std::map<token_type, contract_address_type> tokens {
    {{"brl"}, {"0x0000000000000000000"}},
    {{"usd"}, {"0x0000000000000000000"}},
    {{"ctsi"}, {"0x0000000000000000000"}},
    {{"usdc"}, {"0x0000000000000000000"}}
};

// Trading instrument
struct instrument {
    token_type base;    // token being traded
    token_type quote;   // quote token - "price " of 1 base token
};
 

 // Supported instruments
 static std::map<token_type, instrument>  instruments  {
     {symbol_type{"ctsi/usdc"}, {{"ctsi"}, {"usdc"}}},
     {symbol_type{"usdc/ctsi"}, {{"usdc"}, {"ctsi"}}},
     {symbol_type{"brl/usd"}, {{"blr"}, {"usd"}}},
     {symbol_type{"usd/brl"}, {{"usd"}, {"brl"}}},
     {symbol_type{"ctsi/brl"}, {{"ctsi"}, {"brl"}}},
     {symbol_type{"brl/ctsi"}, {{"brl"}, {"ctsi"}}},
 };


//  Token exchange order
struct order {
    id_type id;             // order id provided by the exchange
    trader_type trader;     // trader id
    symbol_type symbol;     // instrument's symbol (ticker)
    side side;              // buy or sell
    currency_type price;    // limit price in instrument.quote
    qty_type qty;           // remaining quantity
    
    bool matches(const order& other) {
        return (side == side::buy && price >= other.price) || (side == side::sell && price <= other.price);
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
    trader_type trader; // trader id
    id_type order_id;   // order id
    exec_type type;     // execution type
    side side;          // side of executed order
    qty_type qty;       // qtd executed
    currency_type price;   // execution price
};
using execution_reports_type = std::vector<execution_report>;

using wallet_type = std::map<token_type, currency_type, std::less<token_type>, arena_allocator<std::pair<const token_type, currency_type>>>;
using books_type = std::map<symbol_type, book, std::less<symbol_type>, arena_allocator<std::pair<const symbol_type, book>>>;
using wallets_type = std::map<trader_type, wallet_type, std::less<trader_type>, arena_allocator<std::pair<const trader_type, wallet_type>>>;    

// exchange class to be "deserialized" from lambda state
class exchange {
    books_type books;
    wallets_type wallets;
    id_type next_id{0};

public:    
    void new_order(order o, execution_reports_type &reports) {
        // validate order
        auto instrument = instruments.find(o.symbol);
        if (instrument == instruments.end()) {
            reports.push_back({o.trader, o.id, exec_type::rejection});
            return;
        }
        if (o.side == side::buy) {        
            auto size = o.qty * o.price;;
            auto  balance = get_balance(o.trader, instrument->second.quote);
            if (balance < size) {
                reports.push_back({o.trader, o.id, exec_type::rejection});
                return;
            }
            subtract_from_balance(o.trader, instrument->second.quote, size);
        } else {
            auto size = o.qty;
            auto  balance = get_balance(o.trader, instrument->second.base);
            if (balance < size) {
                reports.push_back({o.trader, o.id, exec_type::rejection});
                return;
            }
            subtract_from_balance(o.trader, instrument->second.base, size);
        }        
        
        // send report acknowledging new order
        reports.push_back({o.trader, o.id, exec_type::new_order, o.side, o.qty, o.price});
        // match against existing orders
        auto &book = find_or_create_book(o.symbol);
        if (o.side == side::buy) {
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
    }

    void cancel_order(id_type order_id) {
        // todo:
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
        // todo: notifications
    }

    void  withdraw(const trader_type &trader, const token_type &token, currency_type amount) {
        subtract_from_balance(trader, token, amount);
        // todo: notifications
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
            auto &buy_order =  o.side == side::buy ? o : best_offer;
            auto &sell_order = o.side == side::buy ? best_offer : o;
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
            reports.push_back({buyer, buy_order.id, exec_type::execution, side::buy, exec_qty, exec_price});
            reports.push_back({seller, sell_order.id, exec_type::execution, side::sell, exec_qty, exec_price});
            // remove offer from book if filled
            if (best_offer.is_filled()) {
                offers.erase(it);
            }
            // find next best offer
            it = offers.begin();
        }
    }


    id_type get_next_id() {
        return ++next_id;
    }
};

void print_book(FILE *out,const book &book) {
    auto b = book.bids.begin();
    auto a = book.asks.begin();
    fprintf(out, "id  \ttradr\tqty\tbid | ask\tqty \ttradr\tid\n");
    while (b!=book.bids.end() || a!=book.asks.end()) {
        if (b!=book.bids.end()) {
            fprintf(out, "%d\t%s\t%d\t%d", b->id, b->trader.data(), b->qty, b->price);
        } else {
            fprintf(out, "    \t    \t    \t    ");
        }
        fprintf(out, " | ");
        if (a!=book.asks.end()) {
            fprintf(out, "%d\t%d\t%s\t%d", a->price, a->qty, a->trader.data(), a->id);
        } else {
            fprintf(out, "    \t    \t    \t    ");
        }
        fprintf(out, "\n");
        if (b!=book.bids.end()) ++b;
        if (a!=book.asks.end()) ++a;
    }
}

void print_wallet(FILE *out, const wallet_type &wallet) {
    for(auto &entry: wallet) {
        fprintf(out, "%s: %d\n", entry.first.data(), entry.second);
    }
}

static void print_execution_report(FILE *out, execution_report r) {
    fprintf(out, "Execution - Trader: %s ", r.trader.data());
    fprintf(out, "Order ID: %d ", r.order_id);
    fprintf(out, "Execution Type: %c ", (char)r.type);
    fprintf(out, "Side: %c ", (char)r.side);
    fprintf(out, "Quantity: %d ", r.qty);
    fprintf(out, "Price: %d\n", r.price);
}

static void setup_test_fixture(exchange &ex) {
    // // setup playground
     execution_reports_type r;
     ex.deposit(trader_type{"perna"}, token_type{"ctsi"}, 1000000);
     auto x = ex.find_wallet(trader_type{"perna"});
     print_wallet(stdout, *x);
     ex.deposit(trader_type{"perna"}, token_type{"usdc"}, 1000000);
     ex.deposit(trader_type{"diego"}, token_type{"ctsi"}, 1000000);
     ex.deposit(trader_type{"diego"}, token_type{"usdc"}, 1000000);
     
     ex.new_order(order{0, "perna", TEST_SYMBOL, side::buy, 100, 100}, r); 
    ex.new_order(order{0, "diego", TEST_SYMBOL, side::sell, 120, 100}, r); 
    ex.new_order(order{0, "perna", TEST_SYMBOL, side::buy, 110, 50}, r); 
    ex.new_order(order{0, "perna", TEST_SYMBOL, side::buy, 111, 40}, r); 
    ex.new_order(order{0, "diego", TEST_SYMBOL, side::sell, 112, 50}, r); 
    auto *book = ex.find_book(symbol_type{TEST_SYMBOL});
    print_book(stdout, *book);
}

static bool print_error(FILE *out, const char* message) {
    fprintf(out, "ERROR: %s\n", message);
    return false;
}

/*
    commands are in the format:
    buy   <symbol> <trader> <price> <qty>
    sell  <symbol> <trader>  <price> <qty>
    book <symbol>
*/
static bool process_input(exchange &ex, FILE *in, FILE *out, bool interactive = false) {
    char cmd[1024];
    symbol_type symbol;
    token_type token;
    trader_type trader;
    currency_type price;
    qty_type qty;
    execution_reports_type reports;
    memset(cmd, 0, sizeof(cmd));
    memset(symbol.data(), 0, sizeof(symbol));
    memset(token.data(), 0, sizeof(token));
    memset(trader.data(), 0, sizeof(trader));
    if (1 != fscanf(in, "%s", cmd)) {
        return print_error(out, "missing command");
    }
    if (0 == strcmp(cmd, "q")) {
        return false;
    }
    if (0 == strcmp(cmd, "buy")) {
        if (4 != fscanf(in, "%s %s %d %d", symbol.data(), trader.data(), &price, &qty)) {
            return print_error(out, "insufficient arguments for buy command");
        }
        ex.new_order(order{0, trader, symbol, side::buy, price, qty}, reports);
    } else if (0 == strcmp(cmd, "sell")) {
        if (4 != fscanf(in, "%s %s %d %d", symbol.data(), trader.data(), &price, &qty)) {
            return print_error(out, "insufficient arguments for sell command");
        }
        ex.new_order(order{0, trader, symbol, side::sell, price, qty}, reports);
    } else if (0 == strcmp(cmd, "book")) {
        if (1 != fscanf(in, "%s", symbol.data())) {
            return print_error(out, "insufficient arguments for book command");
        }
        auto *pbook = ex.find_book(symbol);
        // todo: return book in standard output format
        if (pbook) {
            print_book(out, *pbook);
        } else {
            fprintf(out, "No book for symbol %s\n", symbol.data());
        }
    } else if (0 == strcmp(cmd, "wallet")) {
        if (1 != fscanf(in, "%s", trader.data())) {
            return print_error(out, "insufficient arguments for book command");
        }
        auto *pwallet = ex.find_wallet(trader);
        if (pwallet) {
            print_wallet(out, *pwallet);
        } else {
            fprintf(out, "No wallet for trader %s\n", trader.data());
        }
    } else if (0 == strcmp(cmd, "deposit")) {
        if (3 != fscanf(in, "%s %s %d", trader.data(), token.data(), &qty)) {
            return print_error(out, "insufficient arguments for sell command");
        }
        ex.deposit(trader, token, qty);
    } else if (0 == strcmp(cmd, "withdraw")) {
        if (3 != fscanf(in, "%s %s %d", trader.data(), token.data(),  &qty)) {
            return print_error(out, "insufficient arguments for sell command");
        }
        ex.withdraw(trader, token, qty);
    } else {
        return print_error(out, "invalid command");
    }
    // TODO: convert the execution reports in standard output formant suitable for parsing
    for(auto &report: reports) {
        print_execution_report(out, report);
    }
    return true;
}

static void process_inputs(exchange &ex, bool interactive) {
    while(true) {
        if (interactive) {
            fprintf(stdout, "Enter command. Format is: (buy|sell|book) symbol trader price qty\n> ");            
        }
        if (!process_input(ex, stdin, stdout, true)) {
            return;
        }
    }
}

static void print_help_and_exit(const char* program_name) {
    printf("Usage: %s --lambda-state=<file-or-phys-addr> [options...] \n", program_name);
    printf("Options:\n");
    printf("  --lambda-state=<file-or-phys-addr>   Where to read the lambda state from\n");
    printf("  --setup-test-fixture                 Setup test fixture\n");
    printf("  --interactive                        Interactive mode\n");
    printf("  --help                               Show help\n");
    exit(0);
}

int main(int argc, char** argv) {
    //unique_ptr<lambda_state> state;
    std::unique_ptr<lambda_state> state;

    std::string opt_lambda_state;
    bool opt_init_state = false;
    bool opt_setup_test_fixture = false;
    bool opt_interactive = false;
    bool opt_help = false;

    // parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--lambda-state") {
            if (i + 1 < argc) {
                opt_lambda_state = argv[++i];
            } else {
                std::cerr << "--state-file option requires a filename argument" << std::endl;
                return 1;
            }
        } else if (arg == "--setup-test-fixture") {
            opt_setup_test_fixture = true;
        } else if (arg == "--init-state") {
            opt_init_state = true;
        } else if (arg == "--interactive") {
            opt_interactive = true;
        } else if (arg == "--help") {
            opt_help = true;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            return 1;
        }
    }

    // handle commands
    if (opt_help) {
        print_help_and_exit(argv[0]);
    } 

    // map lambda state
    if (opt_lambda_state.empty()) {
        std::cerr << "--lambda-state is required" << std::endl;
        return 1;
    }
    if (opt_lambda_state.find("0x") == 0) {
        auto length_index = opt_lambda_state.find(':');
        if (length_index == std::string::npos) {
            std::cerr << "--lambda-state must be in the format 0x<phys-addr>:<length>" << std::endl;
            return 1;
        }
        auto phys_addr = std::stoull(opt_lambda_state.substr(2, length_index-2), nullptr, 16);
        auto length  = std::stoull(opt_lambda_state.substr(length_index+1), nullptr, 16);
        state = std::unique_ptr<lambda_state>(new lambda_state(phys_addr, length));
    }  else {
        //  dd if=/dev/zero of=lambda-state  bs=1 count=8192
        state = std::unique_ptr<lambda_state>(new lambda_state(opt_lambda_state.c_str()));
    }

    // initialize arena
    arena = reinterpret_cast<memory_arena*>(state->get_state());
    exchange *pex = reinterpret_cast<exchange*>(arena->get_data());
    if (opt_init_state) {
        printf("init state....\n");
        // initialize arena
        arena = new (state->get_state()) memory_arena(state->get_length());
        memset(arena->get_data(), 0, arena->get_data_length());
        // create master object at the start of the arena
        void *root = arena->allocate(sizeof(exchange));
        pex = new (root) exchange();
    }
    auto &ex = *pex;

    if (opt_setup_test_fixture) {
        // setup test fixtureexchange ex;
        setup_test_fixture(ex);
    } 

    process_inputs(ex, opt_interactive);
    
    return 0;
}

