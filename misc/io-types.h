#ifndef IO_TYPES_H
#define IO_TYPES_H

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

static std::ostream &operator<<(std::ostream &out, const symbol_type &s) {
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
    out << "trader:" << s.trader;
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

static std::ostream &operator<<(std::ostream &out, const query_type &s) {
    out << "query{";
    if (s.what == query_what::wallet) {
        out << s.wallet;
    } else {
        out << s.book;
    }
    out << "}";
    return out;
}

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

#endif
