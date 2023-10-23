#include <algorithm>
#include <array>
#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstdio> // fprintf
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

constexpr uint64_t LAMBDA_VIRTUAL_START = UINT64_C(0x1000000000); // something that worked

////////////////////////////////////////////////////////////////////////////////
// Input/output types for advance/inspect state and voucher/notice/report
#include "io-types.h"

////////////////////////////////////////////////////////////////////////////////
// Perna's exchange

class memory_arena {
    uint64_t m_length;
    uint64_t m_next_free;
    unsigned char m_data[0];

public:
    memory_arena(uint64_t length) : m_length(length), m_next_free(0) {}
    void *get_data() {
        return m_data;
    }

    void *allocate(int length) {
        volatile void *p =  m_data + m_next_free;
        if ((m_next_free + length) > get_data_length()) {
            return nullptr;
        }
        m_next_free += length;
        return const_cast<void*>(p);
    }

    void deallocate(void *p, int length) {}

    uint64_t get_data_length() {
        return m_length - offsetof(memory_arena, m_data);
    }
};

// Global arena shared by all instances of arena_allocator
static memory_arena *g_arena;

template <typename T>
class arena_allocator {
public:
    arena_allocator() = default;
    using pointer = T *;
    using value_type = T;
    pointer allocate(std::size_t n) {
        return static_cast<pointer>(g_arena->allocate(n * sizeof(T)));
    }
    void deallocate(pointer p, std::size_t n) noexcept {
        g_arena->deallocate(p, n * sizeof(T));
    }
};

namespace perna {

// Trading instrument
struct instrument_type {
    token_type base;  // token being traded
    token_type quote; // quote token - "price " of 1 base token
};

//  Token exchange order
struct order_type {
    id_type id;          // order id provided by the exchange
    trader_type trader;  // trader id
    symbol_type symbol;  // instrument's symbol (ticker)
    side_what side;      // buy or sell
    currency_type price; // limit price in instrument.quote
    quantity_type quantity;   // remaining quantity

    bool matches(const order_type &other) {
        return (side == side_what::buy && price >= other.price) || (side == side_what::sell && price <= other.price);
    }

    bool is_filled() {
        return quantity == 0;
    }
};

// comparator for sorting bid offers
struct best_bid {
    bool operator()(const order_type &a, const order_type &b) const {
        return a.price > b.price;
    }
};

// comparator for sortting ask offers
struct best_ask {
    bool operator()(const order_type &a, const order_type &b) const {
        return a.price < b.price;
    }
};

using bids_type = std::multiset<order_type, best_bid, arena_allocator<order_type>>;
using asks_type = std::multiset<order_type, best_ask, arena_allocator<order_type>>;

struct book_type {
    symbol_type symbol;
    bids_type bids;
    asks_type asks;
};

using wallet_type = std::map<token_type, currency_type, std::less<token_type>,
    arena_allocator<std::pair<const token_type, currency_type>>>;
using books_type =
    std::map<symbol_type, book_type, std::less<symbol_type>, arena_allocator<std::pair<const symbol_type, book_type>>>;
using wallets_type = std::map<trader_type, wallet_type, std::less<trader_type>,
    arena_allocator<std::pair<const trader_type, wallet_type>>>;
using instruments_type = std::map<symbol_type, instrument_type, std::less<symbol_type>,
    arena_allocator<std::pair<const symbol_type, instrument_type>>>;

// exchange class to be "deserialized" from lambda state
class exchange {
    instruments_type instruments;
    books_type books;
    wallets_type wallets;
    id_type next_id{0};

public:
    exchange() {
        instruments[symbol_type{"ADA/USDT"}] = instrument_type{ADA_ADDRESS, USDT_ADDRESS};
        instruments[symbol_type{"BNB/USDT"}] = instrument_type{BNB_ADDRESS, USDT_ADDRESS};
        instruments[symbol_type{"BTC/USDT"}] = instrument_type{BTC_ADDRESS, USDT_ADDRESS};
        instruments[symbol_type{"CTSI/USDT"}] = instrument_type{CTSI_ADDRESS, USDT_ADDRESS};
        instruments[symbol_type{"DAI/USDT"}] = instrument_type{DAI_ADDRESS, USDT_ADDRESS};
        instruments[symbol_type{"DOGE/USDT"}] = instrument_type{DOGE_ADDRESS, USDT_ADDRESS};
        instruments[symbol_type{"SOL/USDT"}] = instrument_type{SOL_ADDRESS, USDT_ADDRESS};
        instruments[symbol_type{"TON/USDT"}] = instrument_type{TON_ADDRESS, USDT_ADDRESS};
        instruments[symbol_type{"XRP/USDT"}] = instrument_type{XRP_ADDRESS, USDT_ADDRESS};
        instruments[symbol_type{"ADA/BTC"}] = instrument_type{ADA_ADDRESS, BTC_ADDRESS };
        instruments[symbol_type{"BNB/BTC"}] = instrument_type{BNB_ADDRESS, BTC_ADDRESS };
        instruments[symbol_type{"CTSI/BTC"}] = instrument_type{CTSI_ADDRESS, BTC_ADDRESS };
        instruments[symbol_type{"XRP/BTC"}] = instrument_type{XRP_ADDRESS, BTC_ADDRESS };
    }

    bool new_order(order_type o, execution_notices_type &reports) {
        // validate order
        auto instrument = instruments.find(o.symbol);
        if (instrument == instruments.end()) {
            reports.push_back({o.trader, event_what::rejection_invalid_symbol, o.id, o.symbol, o.side, o.quantity, o.price});
            return false;
        }
        if (o.side == side_what::buy) {
            auto size = (o.quantity * o.price) / 100;
            auto balance = get_balance(o.trader, instrument->second.quote);
            if (balance < size) {
                reports.push_back(
                    {o.trader, event_what::rejection_insufficient_funds, o.id, o.symbol, o.side, o.quantity, o.price});
                return false;
            }
            subtract_from_balance(o.trader, instrument->second.quote, size);
        } else {
            auto size = o.quantity;
            auto balance = get_balance(o.trader, instrument->second.base);
            if (balance < size) {
                reports.push_back(
                    {o.trader, event_what::rejection_insufficient_funds, o.id, o.symbol, o.side, o.quantity, o.price});
                return false;
            }
            subtract_from_balance(o.trader, instrument->second.base, size);
        }
        // send report acknowledging new order
        reports.push_back({o.trader, event_what::new_order, get_next_id(), o.symbol, o.side, o.quantity, o.price});
        // match against existing orders
        auto &book = find_or_create_book(o.symbol);
        if (o.side == side_what::buy) {
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

    wallet_type *find_wallet(const trader_type &trader) {
        auto it = wallets.find(trader);
        if (it != wallets.end()) {
            return &it->second;
        }
        return nullptr;
    }

    book_type *find_book(const symbol_type &symbol) {
        auto it = books.find(symbol);
        if (it != books.end()) {
            return &it->second;
        }
        return nullptr;
    }

    void deposit(const trader_type &trader, const token_type &token, currency_type amount) {
        add_to_balance(trader, token, amount);
    }

    bool withdraw(const trader_type trader, const token_type token, currency_type amount) {
        if (get_balance(trader, token) < amount) {
            return false;
        }
        subtract_from_balance(trader, token, amount);
        return true;
    }

private:
    book_type &find_or_create_book(const symbol_type &symbol) {
        auto it = books.find(symbol);
        if (it != books.end()) {
            return it->second;
        }
        return books.emplace(symbol, book_type{symbol, {}, {}}).first->second;
        ;
    }

    wallet_type &find_or_create_wallet(const trader_type &trader) {
        auto it = wallets.find(trader);
        if (it != wallets.end()) {
            return it->second;
        }
        return wallets.emplace(trader, wallet_type{}).first->second;
        ;
    }

    void subtract_from_balance(const trader_type &trader, const token_type &token, currency_type amount) {
        auto &wallet = find_or_create_wallet(trader);
        wallet[token] -= amount;
    }

    void add_to_balance(const trader_type &trader, const token_type &token, currency_type amount) {
        auto &wallet = find_or_create_wallet(trader);
        wallet[token] += amount;
    }

    // match order against existing offers, executing trades and notifying both parties
    template <typename T>
    void match(order_type &o, T &offers, instrument_type &instr, execution_notices_type &reports) {
        auto it = offers.begin();
        while (it != offers.end()) {
            // ok to drop const becasue the set is ordered by a custom comparator whose key(price) won't be changed
            auto &best_offer = const_cast<order_type &>(*it);
            if (o.is_filled() || !o.matches(best_offer)) {
                return;
            }
            auto &buy_order = o.side == side_what::buy ? o : best_offer;
            auto &sell_order = o.side == side_what::buy ? best_offer : o;
            auto &buyer = buy_order.trader;
            auto &seller = sell_order.trader;
            // execute trade and notify both parties
            auto exec_quantity = std::min(o.quantity, best_offer.quantity);
            buy_order.quantity -= exec_quantity;
            sell_order.quantity -= exec_quantity;
            // exchange tokens
            auto exec_price = (o.price + best_offer.price) / 2;
            add_to_balance(buyer, instr.quote,
                (exec_quantity * buy_order.price) / 100); // add balance locked at the limit order price
            subtract_from_balance(buyer, instr.quote,
                (exec_quantity * exec_price) / 100);                  // subtract balance at the execution price
            add_to_balance(buyer, instr.base, exec_quantity);         // add bought tokens
            subtract_from_balance(seller, instr.base, exec_quantity); // subtract sold tokens
            add_to_balance(seller, instr.quote, (exec_quantity * exec_price) / 100); // add balance at the execution price
            // notify both parties
            reports.push_back(
                {buyer, event_what::execution, buy_order.id, o.symbol, side_what::buy, exec_quantity, exec_price});
            reports.push_back(
                {seller, event_what::execution, sell_order.id, o.symbol, side_what::sell, exec_quantity, exec_price});
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
        if (it == wallet->end()) {
            return 0;
        }
        return it->second;
    }

    id_type get_next_id() {
        return ++next_id;
    }
};

} // namespace perna

struct rollup_state_type;
struct lambda_type;
static bool inspect_state(rollup_state_type *rollup_state, lambda_type *state, const query_type &query,
    uint64_t query_length);

////////////////////////////////////////////////////////////////////////////////
// Rollup APIs

#ifdef EMULATOR
#include "rollup-emulator.hpp"
#endif

#ifdef BARE_METAL
#include "rollup-bare-metal.hpp"
#endif

#ifdef JSONRPC_SERVER
#include "rollup-jsonrpc-server.hpp"
#endif

////////////////////////////////////////////////////////////////////////////////
// Handlers for advance and inspect state

// Flush dapp state to disk.
[[maybe_unused]] static bool flush_lambda(rollup_state_type *rollup_state) {
    // Flushes state changes made into memory using mmap(2) back to the filesystem.
    if (msync(rollup_state->lambda, rollup_state->lambda_length, MS_SYNC) < 0) {
        (void) fprintf(stderr, "[dapp] unable to flush lambda state from memory to disk: %s\n", strerror(errno));
        return false;
    }
    return true;
}

// Dapp state.
struct lambda_type {
    perna::exchange ex;
    memory_arena arena;
};

static bool advance_state_deposit(rollup_state_type *rollup_state, lambda_type *state,
    const erc20_deposit_input_type &deposit) {
    std::cerr << "[dapp] " << deposit << '\n';
    // Consider only successful ERC-20 deposits.
    if (deposit.status != erc20_deposit_status::successful) {
        (void) fprintf(stderr, "[dapp] deposit erc20 transfer failed\n");
        return false;
    }
    auto quantity = to_uint64_t(deposit.amount);
    state->ex.deposit(deposit.sender, deposit.token, quantity);
    notice_type notice{.what = notice_what::wallet_deposit,
        .wallet = wallet_notice_type{.trader = deposit.sender, .token = deposit.token, .quantity = quantity}};
    std::cerr << "[dapp] " << notice.wallet << '\n';
    if (!rollup_write_notice(rollup_state, notice)) {
        (void) fprintf(stderr, "[dapp] unable to issue execution notice\n");
    }
    // Commit changes to rollup state
    (void) flush_lambda(rollup_state);
    return true;
}

static bool advance_state_new_order(rollup_state_type *rollup_state, lambda_type *state, const eth_address &sender,
    const new_order_input_type &new_order) {
    std::cerr << "[dapp] " << new_order << '\n';
    execution_notices_type notices;
    state->ex.new_order(perna::order_type{.id = 0,
                            .trader = sender,
                            .symbol = new_order.symbol,
                            .side = new_order.side,
                            .price = new_order.price,
                            .quantity = new_order.quantity},
        notices);
    // Loop over execution notices emitting
    for (const auto &execution : notices) {
        std::cerr << "[dapp] " << execution << '\n';
        if (!rollup_write_notice(rollup_state, notice_type{.what = notice_what::execution, .execution = execution})) {
            (void) fprintf(stderr, "[dapp] unable to issue execution notice\n");
        }
    }
    // Commit changes to rollup state
    (void) flush_lambda(rollup_state);
    return true;
}

static bool advance_state_cancel_order(rollup_state_type *rollup_state, lambda_type *state, const eth_address &sender,
    const cancel_order_input_type &cancel_order) {
    std::cerr << "[dapp] " << cancel_order << '\n';
    // Commit changes to rollup state
    (void) flush_lambda(rollup_state);
    return true;
}

static bool advance_state_withdraw(rollup_state_type *rollup_state, lambda_type *state, const eth_address &sender,
    const withdraw_input_type &withdraw) {
    std::cerr << "[dapp] " << withdraw << '\n';
    if (state->ex.withdraw(sender, withdraw.token, withdraw.quantity)) {
        be256 amount = to_be256(withdraw.quantity);
        erc20_transfer_payload payload = encode_erc20_transfer(sender, amount);
        if (!rollup_write_voucher(rollup_state, withdraw.token, payload)) {
            (void) fprintf(stderr, "[dapp] unable to issue withdraw voucher\n");
            return false;
        }
        std::cerr << "[dapp] " << payload << '\n';
        // Emit a notice marking the event
        notice_type notice{.what = notice_what::wallet_withdraw,
            .wallet = wallet_notice_type{.trader = sender, .token = withdraw.token, .quantity = withdraw.quantity}};
        std::cerr << "[dapp] " << notice.wallet << '\n';
        if (!rollup_write_notice(rollup_state, notice)) {
            (void) fprintf(stderr, "[dapp] unable to issue execution notice\n");
        }
    }
    // Commit changes to rollup state
    (void) flush_lambda(rollup_state);
    return true;
}

static bool advance_state(rollup_state_type *rollup_state, lambda_type *state,
    const input_metadata_type &input_metadata, const input_type &input, uint64_t input_length) {
    // If sender was ERC20_PORTAL_ADDRESS, this must be a deposit
    if (input_metadata.sender == ERC20_PORTAL_ADDRESS && input_length == sizeof(erc20_deposit_input_type)) {
        return advance_state_deposit(rollup_state, state, input.erc20_deposit);
    }
    // Otherwise, it must be an user input
    switch (input.user.what) {
        case user_input_what::new_order:
            return advance_state_new_order(rollup_state, state, input_metadata.sender, input.user.new_order);
        case user_input_what::cancel_order:
            return advance_state_cancel_order(rollup_state, state, input_metadata.sender, input.user.cancel_order);
        case user_input_what::withdraw:
            return advance_state_withdraw(rollup_state, state, input_metadata.sender, input.user.withdraw);
    }
    // Otherwise it is an invalid request
    (void) fprintf(stderr, "[dapp] invalid advance state request\n");
    return false;
}

static bool inspect_state_book(rollup_state_type *rollup_state, lambda_type *state, const book_query_type &query) {
    std::cerr << "[dapp] " << query << '\n';
    report_type report{.what = report_what::book, .book = { .symbol = query.symbol, .entry_count = 0 } };
    auto depth = std::min(query.depth, MAX_BOOK_ENTRY);
    auto *book = state->ex.find_book(query.symbol);
    if (book) {
        auto b = book->bids.begin();
        auto a = book->asks.begin();
        while ((b != book->bids.end() || a != book->asks.end())) {
            if (b != book->bids.end()) {
                report.book.entries[report.book.entry_count] = book_entry_type{
                    .trader = b->trader,
                    .id = b->id,
                    .side = side_what::buy,
                    .quantity = b->quantity,
                    .price = b->price
                };
                if (++report.book.entry_count >= depth) {
                    break;
                }
                ++b;
            }
            if (a != book->asks.end()) {
                report.book.entries[report.book.entry_count] = book_entry_type{
                    .trader = a->trader,
                    .id = a->id,
                    .side = side_what::sell,
                    .quantity = a->quantity,
                    .price = a->price
                };
                if (++report.book.entry_count >= depth) {
                    break;
                }
                ++a;
            }
        }
    }
    if (!rollup_write_report(rollup_state, report)) {
        (void) fprintf(stderr, "[dapp] unable to issue book query report\n");
    }
    std::cerr << "[dapp] " << report.book << '\n';
    return true;
}

static bool inspect_state_wallet(rollup_state_type *rollup_state, lambda_type *state, const wallet_query_type &query) {
    std::cerr << "[dapp] " << query << '\n';
    report_type report{.what = report_what::wallet, .wallet = { .entry_count = 0 } };
    auto *wallet = state->ex.find_wallet(query.trader);
    if (wallet) {
        for (auto& [token, amount]: *wallet) {
            if (report.wallet.entry_count >= MAX_WALLET_ENTRY) {
                break;
            }
            report.wallet.entries[report.wallet.entry_count++] = wallet_entry_type{token, amount};
        }
    }
    if (!rollup_write_report(rollup_state, report)) {
        (void) fprintf(stderr, "[dapp] unable to issue book query report\n");
    }
    std::cerr << "[dapp] " << report.wallet << '\n';
    return true;
}

static bool inspect_state(rollup_state_type *rollup_state, lambda_type *state, const query_type &query,
    uint64_t query_length) {
    switch (query.what) {
        case query_what::book:
            return inspect_state_book(rollup_state, state, query.book);
        case query_what::wallet:
            return inspect_state_wallet(rollup_state, state, query.wallet);
    }
    (void) fprintf(stderr, "[dapp] invalid inspect state request\n");
    return false;
}

#ifdef EMULATOR
int main(int argc, char *argv[]) {
    uint64_t lambda_virtual_start = LAMBDA_VIRTUAL_START;
    const char *lambda_drive_label = "lambda";
    bool initialize_lambda = false;
    int end = 0;
    for (int i = 1; i < argc; ++i) {
        end = 0;
        if (sscanf(argv[i], "--lambda-drive-label=%n", &end) == 0 && end != 0) {
            lambda_drive_label = argv[i] + end;
        } else if (sscanf(argv[i], "--lambda-virtual-start=0x%" SCNx64 "%n", &lambda_virtual_start, &end) == 1 &&
            argv[i][end] == 0) {
            ;
        } else if (sscanf(argv[i], "--lambda-virtual-start=%" SCNu64 "%n", &lambda_virtual_start, &end) == 1 &&
            argv[i][end] == 0) {
            ;
        } else if (strcmp(argv[i], "--initialize-lambda") == 0) {
            initialize_lambda = true;
        } else {
            (void) fprintf(stderr, "[dapp] invalid argument '%s'\n", argv[i]);
            return 1;
        }
    }
    auto *rollup_state = rollup_open(lambda_virtual_start, lambda_drive_label);
    if (!rollup_state) {
        (void) fprintf(stderr, "[dapp] unable to initialize rollup\n");
        return 1;
    }
    lambda_type *lambda = reinterpret_cast<lambda_type *>(rollup_state->lambda);
    g_arena = &lambda->arena;
    if (initialize_lambda) {
        memset(lambda, 0, rollup_state->lambda_length);
        new (&lambda->arena) memory_arena(rollup_state->lambda_length -
            (reinterpret_cast<char *>(&lambda->arena) - reinterpret_cast<char *>(lambda)));
        new (&lambda->ex) perna::exchange();
    }
    rollup_request_loop<lambda_type, input_type, query_type>(rollup_state, advance_state, inspect_state);
    // Unreachable code.
}
#endif

#ifdef BARE_METAL
int main(int argc, char *argv[]) {
    rollup_config_type config;
    config.lambda_virtual_start = LAMBDA_VIRTUAL_START;
    bool initialize_lambda = false;
    int end = 0;
    for (int i = 1; i < argc; ++i) {
        end = 0;
        if (sscanf(argv[i], "--image-filename=%n", &end) == 0 && end != 0) {
            config.image_filename = argv[i] + end;
        } else if (sscanf(argv[i], "--rollup-input-format=%n", &end) == 0 && end != 0) {
            config.input_format = argv[i] + end;
        } else if (sscanf(argv[i], "--rollup-input-metadata-format=%n", &end) == 0 && end != 0) {
            config.input_metadata_format = argv[i] + end;
        } else if (sscanf(argv[i], "--rollup-query-format=%n", &end) == 0 && end != 0) {
            config.query_format = argv[i] + end;
        } else if (sscanf(argv[i], "--lambda-virtual-start=0x%" SCNx64 "%n", &config.lambda_virtual_start, &end) == 1 &&
            argv[i][end] == 0) {
            ;
        } else if (sscanf(argv[i], "--lambda-virtual-start=%" SCNu64 "%n", &config.lambda_virtual_start, &end) == 1 &&
            argv[i][end] == 0) {
            ;
        } else if (sscanf(argv[i], "--rollup-input-begin=%d%n", &config.input_begin, &end) == 1 && argv[i][end] == 0) {
            ;
        } else if (sscanf(argv[i], "--rollup-input-end=%d%n", &config.input_end, &end) == 1 && argv[i][end] == 0) {
            ;
        } else if (sscanf(argv[i], "--rollup-query-begin=%d%n", &config.query_begin, &end) == 1 && argv[i][end] == 0) {
            ;
        } else if (sscanf(argv[i], "--rollup-query-end=%d%n", &config.query_end, &end) == 1 && argv[i][end] == 0) {
            ;
        } else if (strcmp(argv[i], "--initialize-lambda") == 0) {
            initialize_lambda = true;
        } else {
            (void) fprintf(stderr, "[dapp] invalid argument '%s'\n", argv[i]);
            return 1;
        }
    }
    rollup_state_type *rollup_state = rollup_open(config);
    if (!rollup_state) {
        (void) fprintf(stderr, "[dapp] unable to initialize rollup\n");
        return 1;
    }
    lambda_type *lambda = reinterpret_cast<lambda_type *>(rollup_state->lambda);
    g_arena = &lambda->arena;
    if (initialize_lambda) {
        memset(lambda, 0, rollup_state->lambda_length);
        new (&lambda->arena) memory_arena(rollup_state->lambda_length -
            (reinterpret_cast<char *>(&lambda->arena) - reinterpret_cast<char *>(lambda)));
        new (&lambda->ex) perna::exchange();
    }
    return rollup_request_loop<lambda_type, input_type, query_type>(rollup_state, advance_state, inspect_state);
}
#endif

#ifdef JSONRPC_SERVER
int main(int argc, char *argv[]) {
    rollup_config_type config;
    config.lambda_virtual_start = LAMBDA_VIRTUAL_START;
    bool initialize_lambda = false;
    int end = 0;
    for (int i = 1; i < argc; ++i) {
        end = 0;
        if (sscanf(argv[i], "--image-filename=%n", &end) == 0 && end != 0) {
            config.image_filename = argv[i] + end;
        } else if (sscanf(argv[i], "--server-address=%n", &end) == 0 && end != 0) {
            config.server_address = argv[i] + end;
        } else if (sscanf(argv[i], "--lambda-virtual-start=0x%" SCNx64 "%n", &config.lambda_virtual_start, &end) == 1 &&
            argv[i][end] == 0) {
            ;
        } else if (sscanf(argv[i], "--lambda-virtual-start=%" SCNu64 "%n", &config.lambda_virtual_start, &end) == 1 &&
            argv[i][end] == 0) {
            ;
        } else if (strcmp(argv[i], "--initialize-lambda") == 0) {
            initialize_lambda = true;
        } else {
            (void) fprintf(stderr, "[dapp] invalid argument '%s'\n", argv[i]);
            return 1;
        }
    }
    rollup_state_type *rollup_state = rollup_open(config);
    if (!rollup_state) {
        (void) fprintf(stderr, "[dapp] unable to initialize rollup\n");
        return 1;
    }
    lambda_type *lambda = reinterpret_cast<lambda_type *>(rollup_state->lambda);
    g_arena = &lambda->arena;
    if (initialize_lambda) {
        memset(lambda, 0, rollup_state->lambda_length);
        new (&lambda->arena) memory_arena(rollup_state->lambda_length -
            (reinterpret_cast<char *>(&lambda->arena) - reinterpret_cast<char *>(lambda)));
        new (&lambda->ex) perna::exchange();
    }
    return rollup_request_loop<lambda_type, input_type, query_type>(rollup_state, advance_state, inspect_state);
}
#endif
