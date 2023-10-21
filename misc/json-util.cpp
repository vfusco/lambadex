#include <exception>
#include <fstream>
#include <iostream>
#include <array>
#include <string>
#include <unordered_map>

#include "json-util.h"

std::string to_string(const std::string &s) {
    return s;
}

std::string to_string(const char *s) {
    return s;
}

static const unsigned char hex_dec[256] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 255, 255, 255,
    255, 255, 255, 255, 10, 11, 12, 13, 14, 15, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 10, 11, 12, 13, 14,
    15, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
};

static const unsigned char hex_enc[512] = {
    '0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0', '7', '0', '8', '0', '9',
    '0', 'a', '0', 'b', '0', 'c', '0', 'd', '0', 'e', '0', 'f', '1', '0', '1', '1', '1', '2', '1',
    '3', '1', '4', '1', '5', '1', '6', '1', '7', '1', '8', '1', '9', '1', 'a', '1', 'b', '1', 'c', '1', 'd', '1',
    'e', '1', 'f', '2', '0', '2', '1', '2', '2', '2', '3', '2', '4', '2', '5', '2', '6', '2', '7', '2', '8',
    '2', '9', '2', 'a', '2', 'b', '2', 'c', '2', 'd', '2', 'e', '2', 'f', '3', '0', '3', '1', '3', '2', '3', '3',
    '3', '4', '3', '5', '3', '6', '3', '7', '3', '8', '3', '9', '3', 'a', '3', 'b', '3', 'c', '3', 'd', '3', 'e',
    '3', 'f', '4', '0', '4', '1', '4', '2', '4', '3', '4', '4', '4', '5', '4', '6', '4', '7', '4', '8', '4', '9',
    '4', 'a', '4', 'b', '4', 'c', '4', 'd', '4', 'e', '4', 'f', '5', '0', '5', '1', '5', '2', '5', '3', '5', '4', '5',
    '5', '5', '6', '5', '7', '5', '8', '5', '9', '5', 'a', '5', 'b', '5', 'c', '5', 'd', '5', 'e', '5', 'f',
    '6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6', '7', '6', '8', '6', '9',
    '6', 'a', '6', 'b', '6', 'c', '6', 'd', '6', 'e', '6', 'f', '7', '0', '7', '1', '7', '2', '7', '3', '7', '4',
    '7', '5', '7', '6', '7', '7', '7', '8', '7', '9', '7', 'a', '7', 'b', '7', 'c', '7', 'd', '7', 'e', '7',
    'f', '8', '0', '8', '1', '8', '2', '8', '3', '8', '4', '8', '5', '8', '6', '8', '7', '8', '8', '8', '9', '8', 'a',
    '8', 'b', '8', 'c', '8', 'd', '8', 'e', '8', 'f', '9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5', '9',
    '6', '9', '7', '9', '8', '9', '9', '9', 'a', '9', 'b', '9', 'c', '9', 'd', '9', 'e', '9', 'f', 'a', '0', 'a', '1',
    'a', '2', 'a', '3', 'a', '4', 'a', '5', 'a', '6', 'a', '7', 'a', '8', 'a', '9', 'a', 'a', 'a', 'b', 'a', 'c',
    'a', 'd', 'a', 'e', 'a', 'f', 'b', '0', 'b', '1', 'b', '2', 'b', '3', 'b', '4', 'b', '5', 'b', '6', 'b', '7',
    'b', '8', 'b', '9', 'b', 'a', 'b', 'b', 'b', 'c', 'b', 'd', 'b', 'e', 'b', 'f', 'c', '0', 'c',
    '1', 'c', '2', 'c', '3', 'c', '4', 'c', '5', 'c', '6', 'c', '7', 'c', '8', 'c', '9', 'c', 'a', 'c', 'b',
    'c', 'c', 'c', 'd', 'c', 'e', 'c', 'f', 'd', '0', 'd', '1', 'd', '2', 'd', '3', 'd', '4', 'd', '5', 'd', '6', 'd',
    '7', 'd', '8', 'd', '9', 'd', 'a', 'd', 'b', 'd', 'c', 'd', 'd', 'd', 'e', 'd', 'f', 'e', '0', 'e', '1', 'e', '2',
    'e', '3', 'e', '4', 'e', '5', 'e', '6', 'e', '7', 'e', '8', 'e', '9', 'e', 'a', 'e', 'b', 'e', 'c', 'e', 'd', 'e',
    'e', 'e', 'f', 'f', '0', 'f', '1', 'f', '2', 'f', '3', 'f', '4', 'f', '5', 'f', '6', 'f', '7', 'f', '8', 'f', '9',
    'f', 'a', 'f', 'b', 'f', 'c', 'f', 'd', 'f', 'e', 'f', 'f',
};

static size_t encode_hex(const unsigned char *in, size_t in_size, unsigned char *out, size_t out_size) {
	if (!in | !out) {
		throw std::invalid_argument{"need input and output buffers"};
	}
	if (out_size < 2) {
		throw std::invalid_argument{"output buffer is too small"};
	}
	out[0] = '0';
	out[1] = 'x';
	out += 2;
	out_size -= 2;
    if (out_size < 2 * in_size) {
		throw std::invalid_argument{"ouput buffer must be at least double of input buffer"};
	}
	size_t j = 0;
	for (size_t i = 0; i < in_size; ++i) {
		out[j] = hex_enc[2 * in[i]];
		++j;
		out[j] = hex_enc[2 * in[i] + 1];
		++j;
	}
    return j+2;
}

static size_t decode_hex(const unsigned char *in, size_t in_size, unsigned char *out, size_t out_size) {
	if (!in | !out) {
		throw std::invalid_argument{"need input and output buffers"};
	}
	if (in_size < 2) {
		throw std::invalid_argument{"input doesn't start with 0x"};
	}
	if (in[0] != '0' || (in[1] != 'x' && in[1] != 'X')) {
		throw std::invalid_argument{"input doesn't start with 0x"};
	}
	in += 2;
	in_size -= 2;
	if (in_size & 1) {
		throw std::invalid_argument{"input has odd length"};
	}
	size_t j = 0;
	for (size_t i = 0; i < in_size; i += 2) {
		unsigned char n0 = hex_dec[in[i]];
		if (n0 > 15) {
			throw std::invalid_argument{"input not hex encoded"};
		}
		unsigned char n1 = hex_dec[in[i + 1]];
		if (n1 > 15) {
			throw std::invalid_argument{"input not hex encoded"};
		}
		out[j] = (n0 << 4) + n1;
		++j;
	}
	return j;
}

template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, std::string &value, const std::string &path) {
    if (!contains(j, key)) {
        return;
    }
    const auto &jk = j[key];
    if (!jk.is_string()) {
        throw std::invalid_argument("field \""s + path + to_string(key) + "\" not a string");
    }
    value = jk.template get<std::string>();
}

template void ju_get_opt_field<uint64_t>(const nlohmann::json &j, const uint64_t &key, std::string &value,
    const std::string &path);
template void ju_get_opt_field<std::string>(const nlohmann::json &j, const std::string &key, std::string &value,
    const std::string &path);

template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, bool &value, const std::string &path) {
    if (!contains(j, key)) {
        return;
    }
    const auto &jk = j[key];
    if (!jk.is_boolean()) {
        throw std::invalid_argument("field \""s + path + to_string(key) + "\" not a Boolean");
    }
    value = jk.template get<bool>();
}

template void ju_get_opt_field<uint64_t>(const nlohmann::json &j, const uint64_t &key, bool &value,
    const std::string &path);

template void ju_get_opt_field<std::string>(const nlohmann::json &j, const std::string &key, bool &value,
    const std::string &path);

template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, uint64_t &value, const std::string &path) {
    if (!contains(j, key)) {
        return;
    }
    const auto &jk = j[key];
    if (!jk.is_number_integer() && !jk.is_number_unsigned() && !jk.is_number_float()) {
        throw std::invalid_argument("field \""s + path + to_string(key) + "\" not an unsigned integer");
    }
    if (jk.is_number_float()) {
        auto f = jk.template get<nlohmann::json::number_float_t>();
        if (f < 0 || std::fmod(f, static_cast<nlohmann::json::number_float_t>(1.0)) != 0 ||
            f > static_cast<nlohmann::json::number_float_t>(UINT64_MAX)) {
            throw std::invalid_argument("field \""s + path + to_string(key) + "\" not an unsigned integer");
        }
        value = static_cast<uint64_t>(f);
        return;
    }
    if (jk.is_number_unsigned()) {
        value = jk.template get<uint64_t>();
        return;
    }
    auto i = jk.template get<nlohmann::json::number_integer_t>();
    if (i < 0) {
        throw std::invalid_argument("field \""s + path + to_string(key) + "\" not an unsigned integer");
    }
    value = static_cast<uint64_t>(i);
}

template void ju_get_opt_field<uint64_t>(const nlohmann::json &j, const uint64_t &key, uint64_t &value,
    const std::string &path);

template void ju_get_opt_field<std::string>(const nlohmann::json &j, const std::string &key, uint64_t &value,
    const std::string &path);

template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, eth_address &value, const std::string &path) {
    if (!contains(j, key)) {
        return;
    }
    const auto &jk = j[key];
    if (!jk.is_string()) {
        throw std::invalid_argument("field \""s + path + to_string(key) + "\" not a string");
    }
	const auto &address = jk.template get<std::string>();
	if (address.size() != 2*value.size()+2) {
        throw std::invalid_argument("field \""s + path + to_string(key) + "\" not a hex-encoded eth-address");
	}
	decode_hex(reinterpret_cast<const unsigned char *>(address.data()), address.size(), value.data(), value.size());
}

template void ju_get_opt_field<uint64_t>(const nlohmann::json &j, const uint64_t &key, eth_address &value,
    const std::string &path);

template void ju_get_opt_field<std::string>(const nlohmann::json &j, const std::string &key, eth_address &value,
    const std::string &path);

template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, symbol_type &value, const std::string &path) {
    if (!contains(j, key)) {
        return;
    }
    const auto &jk = j[key];
    if (!jk.is_string()) {
        throw std::invalid_argument("field \""s + path + to_string(key) + "\" not a string");
    }
	const auto &symbol = jk.template get<std::string>();
	if (symbol.size() > value.size()) {
        throw std::invalid_argument("field \""s + path + to_string(key) + "\" not a hex-encoded eth-address");
	}
    value = symbol_type{};
    std::copy(symbol.begin(), symbol.end(), value.begin());
}

template void ju_get_opt_field<uint64_t>(const nlohmann::json &j, const uint64_t &key, symbol_type &value,
    const std::string &path);

template void ju_get_opt_field<std::string>(const nlohmann::json &j, const std::string &key, symbol_type &value,
    const std::string &path);

template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, wallet_query_type &value, const std::string &path) {
    if (!contains(j, key)) {
        return;
    }
    const auto &wallet_query = j[key];
    const auto new_path = path + to_string(key) + "/";
    ju_get_field(wallet_query, "trader"s, value.trader, new_path);
}

template void ju_get_opt_field<uint64_t>(const nlohmann::json &j, const uint64_t &key, wallet_query_type &value,
    const std::string &path);

template void ju_get_opt_field<std::string>(const nlohmann::json &j, const std::string &key, wallet_query_type &value,
    const std::string &path);

template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, book_query_type &value, const std::string &path) {
    if (!contains(j, key)) {
        return;
    }
    const auto &book_query = j[key];
    const auto new_path = path + to_string(key) + "/";
    ju_get_field(book_query, "symbol"s, value.symbol, new_path);
    uint64_t depth;
    ju_get_field(book_query, "depth"s, depth, new_path);
    value.depth = depth;
}

template void ju_get_opt_field<uint64_t>(const nlohmann::json &j, const uint64_t &key, book_query_type &value,
    const std::string &path);

template void ju_get_opt_field<std::string>(const nlohmann::json &j, const std::string &key, book_query_type &value,
    const std::string &path);

template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, query_what &value, const std::string &path) {
    if (!contains(j, key)) {
        return;
    }
    const auto &jk = j[key];
    if (!jk.is_string()) {
        throw std::invalid_argument("field \""s + path + to_string(key) + "\" not a query_what");
    }
    const std::string &what = jk.template get<std::string>();
    if (what == "book") {
        value = query_what::book;
    } else if (what == "wallet") {
        value = query_what::wallet;
    } else {
        throw std::invalid_argument("field \""s + path + to_string(key) + "\" not a query_what");
    }
}

template void ju_get_opt_field<uint64_t>(const nlohmann::json &j, const uint64_t &key, query_what &value,
    const std::string &path);

template void ju_get_opt_field<std::string>(const nlohmann::json &j, const std::string &key, query_what &value,
    const std::string &path);

template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, query_type &value, const std::string &path) {
    if (!contains(j, key)) {
        return;
    }
    const auto &query = j[key];
    const auto new_path = path + to_string(key) + "/";
    ju_get_field(query, "what"s, value.what, new_path);
    if (value.what == query_what::book) {
        ju_get_field(query, "book"s, value.book, new_path);
    } else {
        ju_get_field(query, "wallet"s, value.wallet, new_path);
    }
}

template void ju_get_opt_field<uint64_t>(const nlohmann::json &j, const uint64_t &key, query_type &value,
    const std::string &path);

template void ju_get_opt_field<std::string>(const nlohmann::json &j, const std::string &key, query_type &value,
    const std::string &path);

std::string encode_eth_address(const eth_address &address) {
    std::array<unsigned char, 20*2+3> buf{};
    auto hex_size = encode_hex(address.data(), address.size(), buf.data(), buf.size());
    return std::string{reinterpret_cast<char *>(buf.data()), hex_size};
}

std::string encode_symbol(const symbol_type &symbol) {
    return std::string{symbol.data(), strnlen(symbol.data(), symbol.size())};
}

void to_json(nlohmann::json &j, const side_what &what) {
    switch (what) {
        case side_what::buy:
            j = "buy";
            break;
        case side_what::sell:
            j = "sell";
            break;
        default:
            j = "uknown";
            break;
    }
}

void to_json(nlohmann::json &j, const report_what &what) {
    switch (what) {
        case report_what::book:
            j = "book";
            break;
        case report_what::wallet:
            j = "wallet";
            break;
        default:
            j = "uknown";
            break;
    }
}

void to_json(nlohmann::json &j, const wallet_entry_type &entry) {
    j = nlohmann::json{{"token", encode_eth_address(entry.token)}, {"quantity", entry.quantity}};
}

void to_json(nlohmann::json &j, const wallet_report_type &wallet_report) {
    nlohmann::json entries = nlohmann::json::array();
    std::transform(&wallet_report.entries[0],
        &wallet_report.entries[std::min(MAX_WALLET_ENTRY, wallet_report.entry_count)],
        std::back_inserter(entries),
        [](const wallet_entry_type &e) -> nlohmann::json { return e; });
    j = nlohmann::json{{"entries", entries}};
}

void to_json(nlohmann::json &j, const book_entry_type &entry) {
    j = nlohmann::json{{"trader", encode_eth_address(entry.trader)}, {"id", entry.id}, {"side", entry.side}, {"price", entry.price}, {"quantity", entry.quantity}};
}

void to_json(nlohmann::json &j, const book_report_type &book_report) {
    nlohmann::json entries = nlohmann::json::array();
    std::transform(&book_report.entries[0], &book_report.entries[std::min(MAX_BOOK_ENTRY, book_report.entry_count)],
         std::back_inserter(entries), [](const book_entry_type &e) -> nlohmann::json { return e; });
    j = nlohmann::json{{"symbol", encode_symbol(book_report.symbol)}, {"entries", entries}};
}

void to_json(nlohmann::json &j, const report_type &report) {
    if (report.what == report_what::book) {
        j = nlohmann::json{{"what", report.what}, {"book", report.book}};
    } else {
        j = nlohmann::json{{"what", report.what}, {"wallet", report.wallet}};
    }
}
