#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <array>
#include <map>
#include <set>

#define TEST_SYMBOL "ACME"

/// start of custom allocator stuff

unsigned char arena[8192]; // <<---- representing the lambda state that will  materialized in a known location
unsigned int arena_current = 0;
void* arena_allocate(std::size_t n) {
    auto p = arena+arena_current;
    arena_current += n;
    if (arena_current > sizeof(arena)) {
        throw std::bad_alloc();
    }
    return p;
}
void arena_deallocate(void* p, std::size_t n) noexcept {
    // no-op for now
}

template <typename T>
class arena_allocator {
public:
    arena_allocator() = default;
    using pointer = T*;
    using value_type = T;
    pointer allocate(std::size_t n) {
        return (pointer)arena_allocate(n * sizeof(T));
    }
    void deallocate(pointer p, std::size_t n) noexcept {
        arena_deallocate(p, n * sizeof(T));
    }
};

// start of exchange types

using symbol_type = std::array<char, 10>;
using price_type = int;
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

struct order {
    id_type id;
    trader_type trader;
    symbol_type symbol;
    side side;
    price_type price;    // limit price 
    qty_type qty;        // remaining quantity
};

bool order_matches(const order& o, const order& other) {
    return (o.side == side::buy && o.price >= other.price) || (o.side == side::sell && o.price <= other.price);
} 

bool order_is_filled(const order& o)  {
    return  o.qty == 0;
} 

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

// order book for one symbol
// CTSIxUSDC:  USDC for CTSI
// a buy order means sending CTSI and receiving USDC
// a sell order means sending  USDC and receiving CTSI
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
    price_type price;   // execution price
};

using execution_reports_type = std::vector<execution_report>;

// exchange class to be "deserialized" from lambda state
class exchange {
    using books_type = std::map<symbol_type, book, std::less<symbol_type>, arena_allocator<std::pair<const symbol_type, book>>>;
    books_type books;
    id_type next_id{0};

public:    
    void add_order(order o, execution_reports_type &reports) {
        // send report acknowledging new order
        o.id = get_next_id();
        reports.push_back({o.trader, o.id, exec_type::new_order, o.side, o.qty, o.price});
        // match against existing orders
        auto &book = find_or_create_book(o.symbol);
        if (o.side == side::buy) {
            match(o, book.asks, reports);    
            if (!order_is_filled(o)) {
                book.bids.insert(o);
            }
        } else {
            match(o, book.bids, reports);    
            if (!order_is_filled(o)) {
                book.asks.insert(o);
            }
        }
    }

    book *find_book(const symbol_type &symbol) {
        auto it = books.find(symbol);
        if (it != books.end())  {
            return &it->second;
        }
        return nullptr;
    }

private:

    book &find_or_create_book(const symbol_type &symbol) {
        auto it = books.find(symbol);
        if (it != books.end())  {
            return it->second;
        }
        return books.emplace(symbol, book{symbol, {}, {}}).first->second;;
    }   

    // match order against existing offers, executing trades and notifying both parties
    template <typename T>
    void match(order& o, T &offers, execution_reports_type& reports) {
        auto it = offers.begin();
        while(it != offers.end()) {
            // ok to drop const becasue the set is ordered by a custom comparator whose key(price) won't be changed
            auto &best_offer = const_cast<order&>(*it);
            if (order_is_filled(o) || !order_matches(o, best_offer))  {
                return;
            }
            // execute trade and notify both parties
            auto qty = std::min(o.qty, best_offer.qty);
            o.qty -= qty;
            best_offer.qty -= qty;
            auto price = (o.price + best_offer.price) /2;
            reports.push_back({o.trader, o.id, exec_type::execution, o.side, qty, price});
            reports.push_back({best_offer.trader, best_offer.id, exec_type::execution, o.side == side::buy ? side::sell : side::buy, qty, price});
            // remove offer from book if filled
            if (order_is_filled(best_offer)) {
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

// utility functions for testing
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
     ex.add_order(order{0, "perna", TEST_SYMBOL, side::buy, 100, 100}, r); 
    ex.add_order(order{0, "diego", TEST_SYMBOL, side::sell, 120, 100}, r); 
    ex.add_order(order{0, "perna", TEST_SYMBOL, side::buy, 110, 50}, r); 
    ex.add_order(order{0, "perna", TEST_SYMBOL, side::buy, 111, 40}, r); 
    ex.add_order(order{0, "diego", TEST_SYMBOL, side::sell, 112, 50}, r); 
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
    trader_type trader;
    price_type price;
    qty_type qty;
    execution_reports_type reports;
    memset(cmd, 0, sizeof(cmd));
    memset(symbol.data(), 0, sizeof(symbol));
    if (1 != fscanf(in, "%s", cmd)) {
        return print_error(out, "missing command");
    }
    if (0 == strcmp(cmd, "buy")) {
        if (4 != fscanf(in, "%s %s %d %d", symbol.data(), trader.data(), &price, &qty)) {
            return print_error(out, "insufficient arguments for buy command");
        }
        ex.add_order(order{0, trader, symbol, side::buy, price, qty}, reports);
    } else if (0 == strcmp(cmd, "sell")) {
        if (4 != fscanf(in, "%s %s %d %d", symbol.data(), trader.data(), &price, &qty)) {
            return print_error(out, "insufficient arguments for sell command");
        }
        ex.add_order(order{0, trader, symbol, side::sell, price, qty}, reports);
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
        process_input(ex, stdin, stdout, true);
    }
}

static void print_help_and_exit(const char* program_name) {
    printf("Usage: %s [--state-file=<file-name>] [--output-file=<file-name>] [--setup-test-fixture] [--help]\n", program_name);
    printf("Options:\n");
    printf("  --state-file=<file-name>   Load state from file\n");
    printf("  --output-file=<file-name>  Write output to file\n");
    printf("  --setup-test-fixture       Setup test fixture\n");
    printf("  --interactive              Interactive mode\n");
    printf("  --help                     Show help\n");
    exit(0);
}

int main(int argc, char** argv) {
    std::string opt_state_file;
    std::string opt_output_file;
    bool opt_setup_test_fixture = false;
    bool opt_interactive = false;
    bool opt_help = false;

    // parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--state-file") {
            if (i + 1 < argc) {
                opt_state_file = argv[++i];
            } else {
                std::cerr << "--state-file option requires a filename argument" << std::endl;
                return 1;
            }
        } else if (arg == "--output-file") {
            if (i + 1 < argc) {
                opt_output_file = argv[++i];
            } else {
                std::cerr << "--output-file option requires a filename argument" << std::endl;
                return 1;
            }
        } else if (arg == "--setup-test-fixture") {
            opt_setup_test_fixture = true;
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

    // initialize exchange
    // placement new to initialize exchange from the lambda state
    void *root = arena_allocate(sizeof(exchange));
    auto pex = new (root) exchange();
    auto &ex = *pex;

    if (opt_setup_test_fixture) {
        // setup test fixtureexchange ex;
        setup_test_fixture(ex);
    } 

    process_inputs(ex, opt_interactive);

    return 0;
}

