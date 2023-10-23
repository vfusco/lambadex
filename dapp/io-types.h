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

static constexpr eth_address ADA_ADDRESS{0xc6, 0xe7, 0xDF, 0x5E, 0x7b, 0x4f, 0x2A, 0x27, 0x89, 0x06, 0x86, 0x2b, 0x61, 0x20, 0x58, 0x50, 0x34, 0x4D, 0x4e, 0x7d};

static constexpr eth_address BNB_ADDRESS{0x59, 0xb6, 0x70, 0xe9, 0xfA, 0x9D, 0x0A, 0x42, 0x77, 0x51, 0xAf, 0x20, 0x1D, 0x67, 0x67, 0x19, 0xa9, 0x70, 0x85, 0x7b};

static constexpr eth_address BTC_ADDRESS{0x4e, 0xd7, 0xc7, 0x0F, 0x96, 0xB9, 0x9c, 0x77, 0x69, 0x95, 0xfB, 0x64, 0x37, 0x7f, 0x0d, 0x4a, 0xB3, 0xB0, 0xe1, 0xC1};

static constexpr eth_address CTSI_ADDRESS{0x32, 0x28, 0x13, 0xFd, 0x9A, 0x80, 0x1c, 0x55, 0x07, 0xc9, 0xde, 0x60, 0x5d, 0x63, 0xCE, 0xA4, 0xf2, 0xCE, 0x6c, 0x44};

static constexpr eth_address DAI_ADDRESS{0xa8, 0x52, 0x33, 0xC6, 0x3b, 0x9E, 0xe9, 0x64, 0xAd, 0xd6, 0xF2, 0xcf, 0xfe, 0x00, 0xFd, 0x84, 0xeb, 0x32, 0x33, 0x8f};

static constexpr eth_address DOGE_ADDRESS{0x4A, 0x67, 0x92, 0x53, 0x41, 0x02, 0x72, 0xdd, 0x52, 0x32, 0xB3, 0xFf, 0x7c, 0xF5, 0xdb, 0xB8, 0x8f, 0x29, 0x53, 0x19};

static constexpr eth_address SOL_ADDRESS{0x7a, 0x20, 0x88, 0xa1, 0xbF, 0xc9, 0xd8, 0x1c, 0x55, 0x36, 0x8A, 0xE1, 0x68, 0xC2, 0xC0, 0x25, 0x70, 0xcB, 0x81, 0x4F};

static constexpr eth_address TON_ADDRESS{0x09, 0x63, 0x5F, 0x64, 0x3e, 0x14, 0x00, 0x90, 0xA9, 0xA8, 0xDc, 0xd7, 0x12, 0xeD, 0x62, 0x85, 0x85, 0x8c, 0xeB, 0xef};

static constexpr eth_address USDT_ADDRESS{0xc5, 0xa5, 0xC4, 0x29, 0x92, 0xdE, 0xCb, 0xae, 0x36, 0x85, 0x13, 0x59, 0x34, 0x5F, 0xE2, 0x59, 0x97, 0xF5, 0xC4, 0x2d};

static constexpr eth_address XRP_ADDRESS{0x67, 0xd2, 0x69, 0x19, 0x1c, 0x92, 0xCa, 0xf3, 0xcD, 0x77, 0x23, 0xF1, 0x16, 0xc8, 0x5e, 0x6E, 0x9b, 0xf5, 0x59, 0x33};

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
