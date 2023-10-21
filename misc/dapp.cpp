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

extern "C" {
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
}

constexpr uint64_t LAMBDA_VIRTUAL_START = UINT64_C(0x1000000000); // something that worked

////////////////////////////////////////////////////////////////////////////////
// Auxiliary input/output types

using eth_address = std::array<uint8_t, 20>;
using token_type = eth_address;

struct input_metadata_type {
    eth_address sender;
    uint64_t block_number;
    uint64_t timestamp;
    uint64_t epoch_index;
    uint64_t input_index;
} __attribute__((packed));

static std::ostream &operator<<(std::ostream &out, const eth_address &s) {
    out << "0x";
    auto f = out.flags();
    for (const unsigned b : s) {
        out << std::hex << std::setfill('0') << std::setw(2) << b;
    }
    out.flags(f);
    return out;
}

using be256 = std::array<uint8_t, 32>;

static uint64_t to_uint64_t(const be256 &b) {
    uint64_t v = 0;
    for (unsigned i = sizeof(v); i >= 1; --i) {
        v <<= 8;
        v += *(b.end() - i);
    }
    return v;
}

static be256 to_be256(uint64_t v) {
    be256 b{};
    for (unsigned i = 1; i <= sizeof(v); ++i) {
        *(b.end() - i) = v & 0xff;
        v >>= 8;
    }
    return b;
}

static std::ostream &operator<<(std::ostream &out, const be256 &s) {
    out << to_uint64_t(s);
    return out;
}

static constexpr eth_address ERC20_PORTAL_ADDRESS{0x9C, 0x21, 0xAE, 0xb2, 0x09, 0x3C, 0x32, 0xDD, 0xbC, 0x53, 0xeE,
    0xF2, 0x4B, 0x87, 0x3B, 0xDC, 0xd1, 0xaD, 0xa1, 0xDB};

static constexpr eth_address CTSI_ADDRESS{0x49, 0x16, 0x04, 0xc0, 0xFD, 0xF0, 0x83, 0x47, 0xDd, 0x1f, 0xa4, 0xEe, 0x06,
    0x2a, 0x82, 0x2A, 0x5D, 0xD0, 0x6B, 0x5D};

static constexpr eth_address USDT_ADDRESS{0xdA, 0xC1, 0x7F, 0x95, 0x8D, 0x2e, 0xe5, 0x23, 0xa2, 0x20, 0x62, 0x06, 0x99,
    0x45, 0x97, 0xC1, 0x3D, 0x83, 0x1e, 0xc7};

static constexpr eth_address BNB_ADDRESS{0xB8, 0xc7, 0x74, 0x82, 0xe4, 0x5F, 0x1F, 0x44, 0xdE, 0x17, 0x45, 0xF5, 0x2C,
    0x74, 0x42, 0x6C, 0x63, 0x1b, 0xDD, 0x52};

enum class erc20_deposit_status : uint8_t {
    failed = 0,
    successful = 1,
};

static std::ostream &operator<<(std::ostream &out, const erc20_deposit_status &s) {
    switch (s) {
        case erc20_deposit_status::failed:
            out << "failed";
            break;
        case erc20_deposit_status::successful:
            out << "successful";
            break;
        default:
            out << "unknown";
            break;
    }
    return out;
}

// Payload encoding for ERC-20 deposits.
struct erc20_deposit_input_type {
    erc20_deposit_status status;
    token_type token;
    eth_address sender;
    be256 amount;
} __attribute__((packed));

static std::ostream &operator<<(std::ostream &out, const erc20_deposit_input_type &s) {
    out << "erc20_deposit_input{";
    out << "status:" << s.status << ',';
    out << "token:" << s.token << ',';
    out << "sender:" << s.sender << ',';
    out << "amount:" << s.amount;
    out << '}';
    return out;
}

// Payload encoding for ERC-20 transfers.
struct erc20_transfer_payload {
    std::array<uint8_t, 16> bytecode;
    eth_address destination;
    be256 amount;
} __attribute__((packed));

// Encodes a ERC-20 transfer of amount to destination address.
static erc20_transfer_payload encode_erc20_transfer(eth_address destination, be256 amount) {
    erc20_transfer_payload payload{};
    // Bytecode for solidity 'transfer(address,uint256)' in solidity.
    // The last 12 bytes in bytecode should be zeros.
    payload.bytecode = {0xa9, 0x05, 0x9c, 0xbb};
    payload.destination = destination;
    payload.amount = amount;
    return payload;
}

static std::ostream &operator<<(std::ostream &out, const erc20_transfer_payload &s) {
    out << "erc20_transfer_payload{";
    out << "destination:" << s.destination << ',';
    out << "amount:" << s.amount;
    out << '}';
    return out;
}

////////////////////////////////////////////////////////////////////////////////
// LambadeX application input/output types

using symbol_type = std::array<char, 10>;

std::ostream &operator<<(std::ostream &out, const symbol_type &s) {
    for (auto c : s) {
        if (c == 0)
            break;
        out << c;
    }
    return out;
}

using currency_type = uint64_t; // fixed point (* 100)

using quantity_type = uint64_t; // fixed point (* 100)

using trader_type = eth_address;

using id_type = uint64_t;

enum class side_what : char { buy = 'B', sell = 'S' };

static std::ostream &operator<<(std::ostream &out, const side_what &s) {
    switch (s) {
        case side_what::buy:
            out << "buy";
            break;
        case side_what::sell:
            out << "sell";
            break;
        default:
            out << "unknown";
            break;
    }
    return out;
}

enum class event_what : char {
    new_order = 'N',                    // ack new order
    cancel_order = 'C',                 // ack cancel
    execution = 'E',                    // trade execution (partial or full)
    rejection_invalid_symbol = 'r',     // order rejection
    rejection_insufficient_funds = 'R', // order rejection
};

static std::ostream &operator<<(std::ostream &out, const event_what &s) {
    switch (s) {
        case event_what::new_order:
            out << "new_order";
            break;
        case event_what::cancel_order:
            out << "cancel_order";
            break;
        case event_what::execution:
            out << "execution";
            break;
        case event_what::rejection_insufficient_funds:
            out << "rejection_insufficient_funds";
            break;
        case event_what::rejection_invalid_symbol:
            out << "rejection_invalid_symbol";
            break;
        default:
            out << "unknown";
            break;
    }
    return out;
}

struct new_order_input_type {
    symbol_type symbol;
    side_what side;
    quantity_type quantity;
    currency_type price;
} __attribute__((packed));

static std::ostream &operator<<(std::ostream &out, const new_order_input_type &s) {
    out << "new_order_input_type{";
    out << "symbol:" << s.symbol << ',';
    out << "side:" << s.side << ',';
    out << "quantity:" << s.quantity << ',';
    out << "price:" << s.price;
    out << "}";
    return out;
}

struct cancel_order_input_type {
    id_type id;
} __attribute__((packed));

static std::ostream &operator<<(std::ostream &out, const cancel_order_input_type &s) {
    out << "cancel_order{";
    out << "id:" << s.id;
    out << "}";
    return out;
}

struct withdraw_input_type {
    token_type token;
    quantity_type quantity;
} __attribute__((packed));

static std::ostream &operator<<(std::ostream &out, const withdraw_input_type &s) {
    out << "withdraw_input_type{";
    out << "token:" << s.token << ',';
    out << "quantity:" << s.quantity;
    out << "}";
    return out;
}

enum class user_input_what : char { new_order = 'N', cancel_order = 'C', withdraw = 'W' };

struct user_input_type {
    user_input_what what;
    union {
        new_order_input_type new_order;
        cancel_order_input_type cancel_order;
        withdraw_input_type withdraw;
    };
} __attribute__((packed));

// This structure holds a union of all types of inputs that can be processed by LambadeX
struct input_type {
    union {
        // This is an input coming from ERC20_PORTAL_ADDRESS and must be a deposit
        erc20_deposit_input_type erc20_deposit;
        // This is an input coming from random users, and can be a new_order, a cancel_order, or a withdraw
        user_input_type user;
    };
} __attribute__((packed));

// This is an execution notification
struct execution_notice_type {
    trader_type trader;
    event_what event;
    id_type id;
    symbol_type symbol;
    side_what side;
    quantity_type quantity;
    currency_type price;
} __attribute__((packed));

using execution_notices_type = std::vector<execution_notice_type>;

static std::ostream &operator<<(std::ostream &out, const execution_notice_type &s) {
    out << "execution_notice_type{";
    out << "trader:" << s.trader << ',';
    out << "event:" << s.event << ',';
    out << "id:" << s.id << ',';
    out << "symbol:" << s.symbol << ',';
    out << "quantity:" << s.quantity << ',';
    out << "price:" << s.price;
    out << "}";
    return out;
}

// This is a deposit or withdraw notification
struct wallet_notice_type {
    trader_type trader;
    token_type token;
    quantity_type quantity;
} __attribute__((packed));

static std::ostream &operator<<(std::ostream &out, const wallet_notice_type &s) {
    out << "wallet_notice_type{";
    out << "trader:" << s.trader << ',';
    out << "token:" << s.token << ',';
    out << "quantity:" << s.quantity;
    out << "}";
    return out;
}

enum class notice_what : char { execution = 'E', wallet_withdraw = 'W', wallet_deposit = 'D' };

struct notice_type {
    notice_what what;
    union {
        wallet_notice_type wallet;
        execution_notice_type execution;
    };
} __attribute__((packed));

enum class query_what : char {
    book = 'B',
    wallet = 'W',
};

struct book_query_type {
    symbol_type symbol;
    uint64_t depth;
} __attribute__((packed));

static std::ostream &operator<<(std::ostream &out, const book_query_type &s) {
    out << "book_query_type{";
    out << "symbol:" << s.symbol << ',';
    out << "depth:" << s.depth;
    out << "}";
    return out;
}

struct wallet_query_type {
    trader_type trader;
} __attribute__((packed));

static std::ostream &operator<<(std::ostream &out, const wallet_query_type &s) {
    out << "wallet_query{";
    out << s.trader;
    out << "}";
    return out;
}

struct query_type {
    query_what what;
    union {
        book_query_type book;
        wallet_query_type wallet;
    };
} __attribute__((packed));

// This is a report in answer to a book query
struct book_entry_type {
    trader_type trader;
    id_type id;
    side_what side;
    quantity_type quantity;
    currency_type price;
} __attribute__((packed));

static std::ostream &operator<<(std::ostream &out, const book_entry_type &s) {
    out << "book_entry_type{";
    out << "trader:" << s.trader << ',';
    out << "id:" << s.id << ',';
    out << "side:" << s.side << ',';
    out << "quantity:" << s.quantity << ',';
    out << "price:" << s.price;
    out << "}";
    return out;
}

constexpr uint64_t MAX_BOOK_ENTRY = 64;
struct book_report_type {
    symbol_type symbol;
    uint64_t entry_count;
    std::array<book_entry_type, MAX_BOOK_ENTRY> entries;
} __attribute__((packed));

static std::ostream &operator<<(std::ostream &out, const book_report_type &s) {
    out << "book_report_type{";
    out << "symbol:" << s.symbol << ',';
    out << "entry_count:" << s.entry_count << ',';
    out << "entries:{";
    for (unsigned i = 0; i < s.entry_count; ++i) {
        out << s.entries[i] << ',';
    }
    out << "}";
    out << "}";
    return out;
}

struct wallet_entry_type {
    token_type token;
    quantity_type quantity;
} __attribute__((packed));

static std::ostream &operator<<(std::ostream &out, const wallet_entry_type &s) {
    out << "wallet_entry_type{";
    out << "token:" << s.token << ',';
    out << "quantity:" << s.quantity;
    out << "}";
    return out;
}

// This is a report in answer to a wallet query
constexpr uint64_t MAX_WALLET_ENTRY = 16;
struct wallet_report_type {
    uint64_t entry_count;
    std::array<wallet_entry_type, MAX_WALLET_ENTRY> entries;
} __attribute__((packed));

static std::ostream &operator<<(std::ostream &out, const wallet_report_type &s) {
    out << "wallet_report_type{";
    out << "entriy_count:" << s.entry_count << ',';
    out << "entries:{";
    for (unsigned i = 0; i < s.entry_count; ++i) {
        out << s.entries[i] << ',';
    }
    out << "}";
    out << "}";
    return out;
}

using report_what = query_what;

struct report_type {
    report_what what;
    union {
        book_report_type book;
        wallet_report_type wallet;
    };
} __attribute__((packed));

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
        auto p = m_data + m_next_free;
        m_next_free += length;
        if (m_next_free > get_data_length()) {
            throw std::bad_alloc();
        }
        return p;
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
        instruments[symbol_type{"ctsi/usdt"}] = instrument_type{CTSI_ADDRESS, USDT_ADDRESS};
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
        reports.push_back({o.trader, event_what::new_order, o.id, o.symbol, o.side, o.quantity, o.price});
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

////////////////////////////////////////////////////////////////////////////////
// Rollup utilities for emulated execution

#ifndef BARE_METAL

extern "C" {
#include <linux/cartesi/rollup.h>
}

#define ROLLUP_DEVICE_NAME "/dev/rollup"

struct rollup_state_type {
    void *lambda;
    size_t lambda_length;
    int fd;
};

[[maybe_unused]] static bool rollup_throw_exception(rollup_state_type *rollup_state, const char *message) {
    rollup_exception exception{};
    exception.payload = {const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(message)), strlen(message)};
    if (ioctl(rollup_state->fd, IOCTL_ROLLUP_THROW_EXCEPTION, &exception) < 0) {
        (void) fprintf(stderr, "[dapp] unable to throw rollup exception: %s\n", strerror(errno));
        return false;
    }
    return true;
}

template <typename T>
[[nodiscard, maybe_unused]] static bool rollup_write_report(rollup_state_type *rollup_state, const T &payload) {
    rollup_report report{};
    report.payload = {const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(&payload)), sizeof(T)};
    if (ioctl(rollup_state->fd, IOCTL_ROLLUP_WRITE_REPORT, &report) < 0) {
        (void) fprintf(stderr, "[dapp] unable to write rollup report: %s\n", strerror(errno));
        return false;
    }
    return true;
}

template <typename T>
[[nodiscard, maybe_unused]] static bool rollup_write_notice(rollup_state_type *rollup_state, const T &payload) {
    rollup_notice notice{};
    notice.payload = {const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(&payload)), sizeof(T)};
    if (ioctl(rollup_state->fd, IOCTL_ROLLUP_WRITE_NOTICE, &notice) < 0) {
        (void) fprintf(stderr, "[dapp] unable to write rollup report: %s\n", strerror(errno));
        return false;
    }
    return true;
}

template <typename T>
[[nodiscard, maybe_unused]] static bool rollup_write_voucher(rollup_state_type *rollup_state,
    const eth_address &destination, const T &payload) {
    rollup_voucher voucher{};
    std::copy(destination.begin(), destination.end(), voucher.destination);
    voucher.payload = {const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(&payload)), sizeof(payload)};
    if (ioctl(rollup_state->fd, IOCTL_ROLLUP_WRITE_VOUCHER, &voucher) < 0) {
        (void) fprintf(stderr, "[dapp] unable to write rollup voucher: %s\n", strerror(errno));
        return false;
    }
    return true;
}

// Finish last rollup request, wait for next rollup request and process it.
// For every new request, reads an input POD and call backs its respective advance or inspect state handler.
template <typename LAMBDA, typename ADVANCE_INPUT, typename INSPECT_QUERY, typename ADVANCE_STATE,
    typename INSPECT_STATE>
[[nodiscard]] static bool rollup_process_next_request(rollup_state_type *rollup_state, bool accept_previous_request,
    ADVANCE_STATE advance_cb, INSPECT_STATE inspect_cb) {
    // Finish previous request and wait for the next request.
    rollup_finish finish_request{};
    finish_request.accept_previous_request = accept_previous_request;
    if (ioctl(rollup_state->fd, IOCTL_ROLLUP_FINISH, &finish_request) < 0) {
        (void) fprintf(stderr, "[dapp] unable to perform IOCTL_ROLLUP_FINISH: %s\n", strerror(errno));
        return false;
    }
    const uint64_t payload_length = static_cast<uint64_t>(finish_request.next_request_payload_length);
    if (finish_request.next_request_type == CARTESI_ROLLUP_ADVANCE_STATE) { // Advance state.
        // Check if input payload length is supported.
        if (payload_length > sizeof(ADVANCE_INPUT)) {
            (void) fprintf(stderr, "[dapp] advance request payload length (%u) is larger than max (%u)\n",
                static_cast<int>(payload_length), static_cast<int>(sizeof(ADVANCE_INPUT)));
            return false;
        }
        // Read the input.
        ADVANCE_INPUT input_payload{};
        rollup_advance_state request{};
        request.payload = {reinterpret_cast<uint8_t *>(&input_payload), sizeof(input_payload)};
        if (ioctl(rollup_state->fd, IOCTL_ROLLUP_READ_ADVANCE_STATE, &request) < 0) {
            (void) fprintf(stderr, "[dapp] unable to perform IOCTL_ROLLUP_READ_ADVANCE_STATE: %s\n", strerror(errno));
            return false;
        }
        input_metadata_type input_metadata{{}, request.metadata.block_number, request.metadata.timestamp,
            request.metadata.epoch_index, request.metadata.input_index};
        std::copy(std::begin(request.metadata.msg_sender), std::end(request.metadata.msg_sender),
            input_metadata.sender.begin());
        // Call advance state handler.
        return advance_cb(rollup_state, reinterpret_cast<LAMBDA *>(rollup_state->lambda), input_metadata, input_payload,
            payload_length);
    } else if (finish_request.next_request_type == CARTESI_ROLLUP_INSPECT_STATE) { // Inspect state.
        // Check if query payload length is supported.
        if (payload_length > sizeof(INSPECT_QUERY)) {
            (void) fprintf(stderr, "[dapp] inspect request payload length is too large\n");
            return false;
        }
        // Read the query.
        INSPECT_QUERY query_data{};
        rollup_inspect_state request{};
        request.payload = {reinterpret_cast<uint8_t *>(&query_data), sizeof(query_data)};
        if (ioctl(rollup_state->fd, IOCTL_ROLLUP_READ_INSPECT_STATE, &request) < 0) {
            (void) fprintf(stderr, "[dapp] unable to perform IOCTL_ROLLUP_READ_INSPECT_STATE: %s\n", strerror(errno));
            return false;
        }
        // Call inspect state handler.
        return inspect_cb(rollup_state, reinterpret_cast<LAMBDA *>(rollup_state->lambda), query_data, payload_length);
    } else {
        (void) fprintf(stderr, "[dapp] invalid request type\n");
        return false;
    }
}

// Process rollup requests forever.
template <typename LAMBDA, typename ADVANCE_INPUT, typename INSPECT_QUERY, typename ADVANCE_STATE,
    typename INSPECT_STATE>
[[noreturn]] static bool rollup_request_loop(rollup_state_type *rollup_state, ADVANCE_STATE advance_cb,
    INSPECT_STATE inspect_cb) {
    // Rollup device requires that we initialize the first previous request as accepted.
    bool accept_previous_request = true;
    // Request loop, should loop forever.
    while (true) {
        accept_previous_request = rollup_process_next_request<LAMBDA, ADVANCE_INPUT, INSPECT_QUERY>(rollup_state,
            accept_previous_request, advance_cb, inspect_cb);
    }
    // Unreachable code.
}

[[nodiscard]] rollup_state_type *rollup_open(uint64_t lambda_virtual_start, const char *lambda_drive_label) {
    static rollup_state_type rollup_state{};
    // Open rollup device.
    rollup_state.fd = open(ROLLUP_DEVICE_NAME, O_RDWR);
    if (rollup_state.fd < 0) {
        // This operation may fail only for machines where the rollup device is not configured correctly.
        (void) fprintf(stderr, "[dapp] unable to open rollup device: %s\n", strerror(errno));
        return nullptr;
    }
    char cmd[512];
    snprintf(cmd, std::size(cmd), "flashdrive \"%.256s\"", lambda_drive_label);
    auto *fin = popen(cmd, "r");
    if (fin == nullptr) {
        (void) fprintf(stderr, "[dapp] popen failed for command '%s' (%s)\n", cmd, strerror(errno));
        return nullptr;
    }
    char *lambda_device = nullptr;
    size_t dummy_n = 0;
    if (getline(&lambda_device, &dummy_n, fin) < 0) {
        (void) fprintf(stderr, "[dapp] unable to get lambda device from label '%s' (%s)\n", lambda_drive_label,
            strerror(errno));
        pclose(fin);
        free(lambda_device);
        return nullptr;
    }
    // Remove eol from return of command
    dummy_n = strlen(lambda_device);
    if (dummy_n > 0 && lambda_device[dummy_n - 1] == '\n') {
        lambda_device[dummy_n - 1] = '\0';
    }
    if (int ret = pclose(fin); ret != 0) {
        (void) fprintf(stderr, "[dapp] '%s' returned exit code %d\n", cmd, ret);
        free(lambda_device);
        return nullptr;
    }
    int memfd = open(lambda_device, O_RDWR);
    if (memfd < 0) {
        (void) fprintf(stderr, "[dapp] failed to open lambda device '%s' (%s)\n", lambda_device, strerror(errno));
        return nullptr;
    }
    free(lambda_device);
    auto off = lseek(memfd, 0, SEEK_END);
    if (off < 0) {
        (void) fprintf(stderr, "[dapp] unable to get length of lambda device (%s)\n", strerror(errno));
        close(memfd);
        return nullptr;
    }
    rollup_state.lambda_length = static_cast<size_t>(off);
    rollup_state.lambda = mmap(reinterpret_cast<void *>(lambda_virtual_start), rollup_state.lambda_length,
        PROT_WRITE | PROT_READ, MAP_SHARED | MAP_FIXED_NOREPLACE, memfd, 0);
    close(memfd);
    if (rollup_state.lambda == MAP_FAILED) {
        (void) fprintf(stderr, "mmap failed (%s)\n", strerror(errno));
        return nullptr;
    }
    if (rollup_state.lambda != reinterpret_cast<void *>(lambda_virtual_start)) {
        fprintf(stderr, "[dapp] mmap mapped wrong virtual address\n");
        munmap(rollup_state.lambda, rollup_state.lambda_length);
        return nullptr;
    }
    (void) fprintf(stderr, "[dapp] lambda virtual start: 0x%016" PRIx64 "\n", lambda_virtual_start);
    (void) fprintf(stderr, "[dapp] lambda length: 0x%016" PRIx64 "\n", rollup_state.lambda_length);
    return &rollup_state;
}

////////////////////////////////////////////////////////////////////////////////
// Rollup utilities for bare metal execution
#else

struct rollup_config_type {
    uint64_t lambda_virtual_start = LAMBDA_VIRTUAL_START;
    const char *image_filename = nullptr;
    int input_begin = 0;
    int input_end = 0;
    int query_begin = 0;
    int query_end = 0;
    const char *input_format = "input-%d.bin";
    const char *input_metadata_format = "input-%d-metadata.bin";
    const char *query_format = "query-%d.bin";
};

struct rollup_state_type {
    void *lambda;
    size_t lambda_length;
    int current_input;
    int current_query;
    int current_voucher;
    int current_report;
    int current_notice;
    rollup_config_type config;
};

rollup_state_type *rollup_open(const rollup_config_type &config) {
    static rollup_state_type rollup_state{.lambda = nullptr,
        .lambda_length = 0,
        .current_input = 0,
        .current_report = 0,
        .current_notice = 0};
    rollup_state.config = config;
    if (config.input_begin != config.input_end) {
        if (!config.input_format) {
            (void) fprintf(stderr, "[dapp] missing rollup input format\n");
            return nullptr;
        }
        if (!config.input_metadata_format) {
            (void) fprintf(stderr, "[dapp] missing rollup input metadata format\n");
            return nullptr;
        }
    }
    rollup_state.current_input = config.input_begin;
    if (config.query_begin != config.query_end) {
        if (!config.query_format) {
            (void) fprintf(stderr, "[dapp] missing rollup query format\n");
            return nullptr;
        }
    }
    rollup_state.current_query = config.query_begin;
    if (!config.image_filename) {
        (void) fprintf(stderr, "[dapp] missing image filename\n");
        return nullptr;
    }
    int memfd = open(config.image_filename, O_RDWR);
    if (memfd < 0) {
        fprintf(stderr, "[dapp] open failed for '%s' (%s)\n", config.image_filename, strerror(errno));
        return nullptr;
    }
    auto off = lseek(memfd, 0, SEEK_END);
    if (off < 0) {
        (void) fprintf(stderr, "[dapp] unable to get length of image file (%s)\n", config.image_filename);
        close(memfd);
        return nullptr;
    }
    rollup_state.lambda_length = static_cast<size_t>(off);
    rollup_state.lambda = mmap(reinterpret_cast<void *>(config.lambda_virtual_start), rollup_state.lambda_length,
        PROT_WRITE | PROT_READ, MAP_SHARED | MAP_FIXED_NOREPLACE, memfd, 0);
    close(memfd);
    if (rollup_state.lambda == MAP_FAILED) {
        (void) fprintf(stderr, "[dapp] mmap failed (%s)\n", strerror(errno));
        return nullptr;
    }
    if (rollup_state.lambda != reinterpret_cast<void *>(config.lambda_virtual_start)) {
        fprintf(stderr, "[dapp] mmap mapped wrong virtual address\n");
        munmap(rollup_state.lambda, rollup_state.lambda_length);
        return nullptr;
    }
    (void) fprintf(stderr, "[dapp] lambda virtual start: 0x%016" PRIx64 "\n", config.lambda_virtual_start);
    (void) fprintf(stderr, "[dapp] lambda length: 0x%016" PRIx64 "\n", rollup_state.lambda_length);
    return &rollup_state;
}

// Process rollup requests until there are no more inputs.
template <typename LAMBDA, typename ADVANCE_INPUT, typename INSPECT_QUERY, typename ADVANCE_STATE,
    typename INSPECT_STATE>
static int rollup_request_loop(rollup_state_type *rollup_state, ADVANCE_STATE advance_cb, INSPECT_STATE inspect_cb) {
    struct raw_input_type {
        be256 offset;
        be256 length;
        ADVANCE_INPUT payload;
    } __attribute__((packed));
    struct raw_input_metadata_type {
        std::array<char, 12> padding;
        input_metadata_type metadata;
    } __attribute__((packed));
    struct raw_query_type {
        be256 offset;
        be256 length;
        INSPECT_QUERY payload;
    } __attribute__((packed));
    for (int i = rollup_state->config.input_begin; i < rollup_state->config.input_end; ++i) {
        // Start report/notice/voucher counters anew
        rollup_state->current_notice = rollup_state->current_voucher = rollup_state->current_report = 0;
        char filename[FILENAME_MAX];
        // Load input metadata
        snprintf(filename, std::size(filename), rollup_state->config.input_metadata_format, i);
        (void) fprintf(stderr, "Loading %s\n", filename);
        auto *fin = fopen(filename, "r");
        if (!fin) {
            (void) fprintf(stderr, "Error opening %s (%s)\n", filename, strerror(errno));
            return 1;
        }
        raw_input_metadata_type raw_input_metadata{};
        auto read = fread(&raw_input_metadata, 1, sizeof(raw_input_metadata), fin);
        if (read < 0) {
            (void) fprintf(stderr, "Error reading from %s (%s)\n", filename, strerror(errno));
            fclose(fin);
            return 1;
        }
        if (read != sizeof(raw_input_metadata)) {
            (void) fprintf(stderr, "Missing metadata in %s\n", filename);
            fclose(fin);
            return 1;
        }
        fclose(fin);
        // Load input
        snprintf(filename, std::size(filename), rollup_state->config.input_format, i);
        (void) fprintf(stderr, "Loading %s\n", filename);
        fin = fopen(filename, "r");
        if (!fin) {
            (void) fprintf(stderr, "Error opening %s (%s)\n", filename, strerror(errno));
            return 1;
        }
        raw_input_type raw_input{};
        read = fread(&raw_input, 1, sizeof(raw_input), fin);
        if (read < 0) {
            (void) fprintf(stderr, "Error reading from %s (%s)\n", filename, strerror(errno));
            fclose(fin);
            return 1;
        }
        if (read < sizeof(raw_input) - sizeof(ADVANCE_INPUT)) {
            (void) fprintf(stderr, "Missing input data in %s\n", filename);
            fclose(fin);
            return 1;
        }
        fclose(fin);
        // Invoke callback
        bool accept = advance_cb(rollup_state, reinterpret_cast<LAMBDA *>(rollup_state->lambda),
            raw_input_metadata.metadata, raw_input.payload, read - (sizeof(raw_input) - sizeof(ADVANCE_INPUT)));
        if (accept) {
            (void) fprintf(stderr, "Accepted input %d\n", i);
        } else {
            (void) fprintf(stderr, "Rejected input %d\n", i);
        }
    }
    for (int i = rollup_state->config.query_begin; i < rollup_state->config.query_end; ++i) {
        // Start report/notice/voucher counters anew
        rollup_state->current_notice = rollup_state->current_voucher = rollup_state->current_report = 0;
        char filename[FILENAME_MAX];
        // Load input metadata
        snprintf(filename, std::size(filename), rollup_state->config.query_format, i);
        (void) fprintf(stderr, "Loading %s\n", filename);
        auto *fin = fopen(filename, "r");
        if (!fin) {
            (void) fprintf(stderr, "Error opening %s (%s)\n", filename, strerror(errno));
            return 1;
        }
        raw_query_type raw_query{};
        auto read = fread(&raw_query, 1, sizeof(raw_query), fin);
        if (read < 0) {
            (void) fprintf(stderr, "Error reading from %s (%s)\n", filename, strerror(errno));
            fclose(fin);
            return 1;
        }
        if (read < sizeof(raw_query) - sizeof(INSPECT_QUERY)) {
            (void) fprintf(stderr, "Missing query data in %s\n", filename);
            fclose(fin);
            return 1;
        }
        fclose(fin);
        // Invoke callback
        bool accept = inspect_cb(rollup_state, reinterpret_cast<LAMBDA *>(rollup_state->lambda), raw_query.payload,
            read - (sizeof(raw_query) - sizeof(INSPECT_QUERY)));
        if (accept) {
            (void) fprintf(stderr, "Accepted query %d\n", i);
        } else {
            (void) fprintf(stderr, "Rejected query %d\n", i);
        }
    }
    return 0;
}

template <typename T>
[[nodiscard, maybe_unused]] static bool rollup_write_data(rollup_state_type *rollup_state, const T &payload,
    const char *what, int &index) {
    struct raw_data_type {
        be256 offset;
        be256 length;
        T payload;
    };
    raw_data_type data{.offset = to_be256(32), .length = to_be256(sizeof(payload)), .payload = payload};
    char filename[FILENAME_MAX];
    const char *prefix = "input";
    if (rollup_state->current_input >= rollup_state->config.input_end) {
        prefix = "query";
    }
    snprintf(filename, std::size(filename), "%s-%d-%s-%d.bin", prefix, rollup_state->current_input, what, index);
    (void) fprintf(stderr, "Storing %s\n", filename);
    auto *fout = fopen(filename, "w");
    if (!fout) {
        (void) fprintf(stderr, "Unable open %s for writing (%s)\n", filename, strerror(errno));
        return false;
    }
    auto written = fwrite(&data, 1, sizeof(data), fout);
    if (written < sizeof(data)) {
        (void) fprintf(stderr, "Unable write to %s (%s)\n", filename, strerror(errno));
        return false;
    }
    fclose(fout);
    ++index;
    return true;
}

template <typename T>
[[nodiscard, maybe_unused]] static bool rollup_write_report(rollup_state_type *rollup_state, const T &payload) {
    return rollup_write_data(rollup_state, payload, "report", rollup_state->current_report);
}

template <typename T>
[[nodiscard, maybe_unused]] static bool rollup_write_notice(rollup_state_type *rollup_state, const T &payload) {
    return rollup_write_data(rollup_state, payload, "notice", rollup_state->current_notice);
}

template <typename T>
[[nodiscard, maybe_unused]] static bool rollup_write_voucher(rollup_state_type *rollup_state,
    const eth_address &destination, const T &payload) {
    struct raw_voucher_type {
        std::array<char, 12> padding;
        eth_address destination;
        be256 offset;
        be256 length;
        T payload;
    };
    raw_voucher_type voucher{.padding = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
        .destination = destination,
        .offset = to_be256(32),
        .length = to_be256(sizeof(payload)),
        .payload = payload};
    char filename[FILENAME_MAX];
    const char *prefix = "input";
    if (rollup_state->current_input >= rollup_state->config.input_end) {
        prefix = "query";
    }
    snprintf(filename, std::size(filename), "%s-%d-voucher-%d.bin", prefix, rollup_state->current_input,
        rollup_state->current_voucher);
    (void) fprintf(stderr, "Storing %s\n", filename);
    auto *fout = fopen(filename, "w");
    if (!fout) {
        (void) fprintf(stderr, "Unable open %s for writing (%s)\n", filename, strerror(errno));
        return false;
    }
    auto written = fwrite(&voucher, 1, sizeof(voucher), fout);
    if (written < sizeof(voucher)) {
        (void) fprintf(stderr, "Unable write to %s (%s)\n", filename, strerror(errno));
        return false;
    }
    fclose(fout);
    ++rollup_state->current_voucher;
    return true;
}

#endif

////////////////////////////////////////////////////////////////////////////////
// DApp state utilities.

// Flush dapp state to disk.
[[maybe_unused]] static bool flush_lambda(rollup_state_type *rollup_state) {
    // Flushes state changes made into memory using mmap(2) back to the filesystem.
    if (msync(rollup_state->lambda, rollup_state->lambda_length, MS_SYNC) < 0) {
        (void) fprintf(stderr, "[dapp] unable to flush lambda state from memory to disk: %s\n", strerror(errno));
        return false;
    }
    return true;
}

// POD for dapp state.
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

#ifndef BARE_METAL
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
#else
int main(int argc, char *argv[]) {
    uint64_t lambda_virtual_start = LAMBDA_VIRTUAL_START;
    const char *image_filename = nullptr;
    rollup_config_type config;
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
