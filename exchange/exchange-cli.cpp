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
#include "exchange.h"

#define TEST_SYMBOL "ctsi/usdc"

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
    fprintf(out, "Price: %d", r.price);
    fprintf(out, "Text: %s\n", r.text.c_str());
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
     
     ex.new_order(order{0, "perna", TEST_SYMBOL, side_type::buy, 100, 100}, r); 
    ex.new_order(order{0, "diego", TEST_SYMBOL, side_type::sell, 120, 100}, r); 
    ex.new_order(order{0, "perna", TEST_SYMBOL, side_type::buy, 110, 50}, r); 
    ex.new_order(order{0, "perna", TEST_SYMBOL, side_type::buy, 111, 40}, r); 
    ex.new_order(order{0, "diego", TEST_SYMBOL, side_type::sell, 112, 50}, r); 
    auto *book = ex.find_book(symbol_type{TEST_SYMBOL});
    print_book(stdout, *book);
}

static bool print_error(FILE *out, const char* message) {
    fprintf(out, "ERROR: %s\n", message);
    return false;
}

static bool process_input(exchange &ex, FILE *in, FILE *out, bool interactive = false) {
    char cmd[1024];
    std::string error_message;
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
        ex.new_order(order{0, trader, symbol, side_type::buy, price, qty}, reports);
    } else if (0 == strcmp(cmd, "sell")) {
        if (4 != fscanf(in, "%s %s %d %d", symbol.data(), trader.data(), &price, &qty)) {
            return print_error(out, "insufficient arguments for sell command");
        }
        ex.new_order(order{0, trader, symbol, side_type::sell, price, qty}, reports);
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
        if (!ex.withdraw(trader, token, qty, &error_message)) {
            printf("ERROR: %s\n", error_message.c_str());
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
    init_instruments();
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
        if (opt_setup_test_fixture) {
            // setup test fixtureexchange ex;
            setup_test_fixture(*pex);
        }
    } else {
        process_inputs(*pex, opt_interactive);
    }
    return 0;
}

