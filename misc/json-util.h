#ifndef JSON_UTIL_H
#define JSON_UTIL_H

#include <array>
#include <string>
#include <type_traits>
#include <optional>

#include "nlohmann/json.hpp"

#include "io-types.h"

using namespace std::string_literals;

// Allow using to_string when the input type is already a string
using std::to_string;
std::string to_string(const std::string &s);
std::string to_string(const char *s);

// Generate a new optional-like type
template <int I, typename T>
struct new_optional : public std::optional<T> {
    using std::optional<T>::optional;
};

// Optional-like type used by parse_args function to identify an optional parameter
template <typename T>
using optional_param = new_optional<0, T>;

// Optional-like type that allows non-default-constructible types to be constructed by functions that
// receive them by reference
template <typename T>
using not_default_constructible = new_optional<1, T>;

// Forward declaration of generic ju_get_field
template <typename T, typename K>
void ju_get_field(const nlohmann::json &j, const K &key, T &value, const std::string &path = "params/");

// Allows use contains when the index is an integer and j contains an array
template <typename I>
inline std::enable_if_t<std::is_integral_v<I>, bool> contains(const nlohmann::json &j, I i) {
    if constexpr (std::is_signed_v<I>) {
        return j.is_array() && i >= 0 && i < static_cast<I>(j.size());
    } else {
        return j.is_array() && i < j.size();
    }
}

// Overload for case where index is a string and j contains an object
inline bool contains(const nlohmann::json &j, const std::string &s) {
    return j.contains(s);
}

/// \brief Attempts to load a string from a field in a JSON object
/// \tparam K Key type (explicit extern declarations for uint64_t and std::string are provided)
/// \param j JSON object to load from
/// \param key Key to load value from
/// \param value Object to store value
/// \param path Path to j
template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, std::string &value, const std::string &path = "params/");

/// \brief Attempts to load a bool from a field in a JSON object
/// \tparam K Key type (explicit extern declarations for uint64_t and std::string are provided)
/// \param j JSON object to load from
/// \param key Key to load value from
/// \param value Object to store value
/// \param path Path to j
template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, bool &value, const std::string &path = "params/");

/// \brief Attempts to load an uint64_t from a field in a JSON object
/// \tparam K Key type (explicit extern declarations for uint64_t and std::string are provided)
/// \param j JSON object to load from
/// \param key Key to load value from
/// \param value Object to store value
/// \param path Path to j
template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, uint64_t &value, const std::string &path = "params/");

/// \brief Attempts to load an symbol_type from a field in a JSON object
/// \tparam K Key type (explicit extern declarations for uint64_t and std::string are provided)
/// \param j JSON object to load from
/// \param key Key to load value from
/// \param value Object to store value
/// \param path Path to j
template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, symbol_type &value, const std::string &path = "params/");

/// \brief Attempts to load an eth_address from a field in a JSON object
/// \tparam K Key type (explicit extern declarations for uint64_t and std::string are provided)
/// \param j JSON object to load from
/// \param key Key to load value from
/// \param value Object to store value
/// \param path Path to j
template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, eth_address &value, const std::string &path = "params/");

/// \brief Attempts to load an wallet_query_type from a field in a JSON object
/// \tparam K Key type (explicit extern declarations for uint64_t and std::string are provided)
/// \param j JSON object to load from
/// \param key Key to load value from
/// \param value Object to store value
/// \param path Path to j
template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, wallet_query_type &value,
    const std::string &path = "params/");

/// \brief Attempts to load an book_query_type from a field in a JSON object
/// \tparam K Key type (explicit extern declarations for uint64_t and std::string are provided)
/// \param j JSON object to load from
/// \param key Key to load value from
/// \param value Object to store value
/// \param path Path to j
template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, book_query_type &value,
    const std::string &path = "params/");

/// \brief Attempts to load an query_what from a field in a JSON object
/// \tparam K Key type (explicit extern declarations for uint64_t and std::string are provided)
/// \param j JSON object to load from
/// \param key Key to load value from
/// \param value Object to store value
/// \param path Path to j
template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, query_what &value, const std::string &path = "params/");

/// \brief Attempts to load an query_type from a field in a JSON object
/// \tparam K Key type (explicit extern declarations for uint64_t and std::string are provided)
/// \param j JSON object to load from
/// \param key Key to load value from
/// \param value Object to store value
/// \param path Path to j
template <typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, query_type &value, const std::string &path = "params/");

/// \brief Attempts to load an optional_param from a field in a JSON object
/// \tparam K Key type (explicit extern declarations for uint64_t and std::string are provided)
/// \param j JSON object to load from
/// \param key Key to load value from
/// \param value Object to store value
/// \param path Path to j
template <typename T, typename K>
void ju_get_opt_field(const nlohmann::json &j, const K &key, optional_param<T> &value,
    const std::string &path = "params/") {
    if (contains(j, key)) {
        value.emplace();
        ju_get_opt_field(j, key, value.value(), path);
    }
}

/// \brief Loads an object from a field in a JSON object.
/// \tparam K Key type (explicit extern declarations for uint64_t and std::string are provided)
/// \param j JSON object to load from
/// \param key Key to load value from
/// \param value Object to store value
/// \param path Path to j
/// \detail Throws error if field is missing
template <typename T, typename K>
void ju_get_field(const nlohmann::json &j, const K &key, T &value, const std::string &path) {
    if (!contains(j, key)) {
        throw std::invalid_argument("missing field \""s + path + to_string(key) + "\""s);
    }
    ju_get_opt_field(j, key, value, path);
}

// Automatic conversion functions from io-types to nlohmann::json
void to_json(nlohmann::json &j, const eth_address &address);
void to_json(nlohmann::json &j, const symbol_type &symbol);
void to_json(nlohmann::json &j, const side_what &what);
void to_json(nlohmann::json &j, const report_what &what);
void to_json(nlohmann::json &j, const book_report_type &book_report);
void to_json(nlohmann::json &j, const book_entry_type &entry);
void to_json(nlohmann::json &j, const wallet_report_type &wallet_report);
void to_json(nlohmann::json &j, const wallet_entry_type &entry);
void to_json(nlohmann::json &j, const report_type &report);

// Extern template declarations
extern template void ju_get_opt_field(const nlohmann::json &j, const std::string &key, std::string &value,
    const std::string &base = "params/");
extern template void ju_get_opt_field(const nlohmann::json &j, const uint64_t &key, std::string &value,
    const std::string &base = "params/");
extern template void ju_get_opt_field(const nlohmann::json &j, const std::string &key, bool &value,
    const std::string &base = "params/");
extern template void ju_get_opt_field(const nlohmann::json &j, const uint64_t &key, bool &value,
    const std::string &base = "params/");
extern template void ju_get_opt_field(const nlohmann::json &j, const std::string &key, uint64_t &value,
    const std::string &base = "params/");
extern template void ju_get_opt_field(const nlohmann::json &j, const uint64_t &key, uint64_t &value,
    const std::string &base = "params/");

extern template void ju_get_opt_field(const nlohmann::json &j, const std::string &key, symbol_type &value,
    const std::string &base = "params/");
extern template void ju_get_opt_field(const nlohmann::json &j, const uint64_t &key, symbol_type &value,
    const std::string &base = "params/");
extern template void ju_get_opt_field(const nlohmann::json &j, const std::string &key, eth_address &value,
    const std::string &base = "params/");
extern template void ju_get_opt_field(const nlohmann::json &j, const uint64_t &key, eth_address &value,
    const std::string &base = "params/");
extern template void ju_get_opt_field(const nlohmann::json &j, const std::string &key, wallet_query_type &value,
    const std::string &base = "params/");
extern template void ju_get_opt_field(const nlohmann::json &j, const uint64_t &key, wallet_query_type &value,
    const std::string &base = "params/");
extern template void ju_get_opt_field(const nlohmann::json &j, const std::string &key, book_query_type &value,
    const std::string &base = "params/");
extern template void ju_get_opt_field(const nlohmann::json &j, const uint64_t &key, book_query_type &value,
    const std::string &base = "params/");
extern template void ju_get_opt_field(const nlohmann::json &j, const std::string &key, query_what &value,
    const std::string &base = "params/");
extern template void ju_get_opt_field(const nlohmann::json &j, const uint64_t &key, query_what &value,
    const std::string &base = "params/");
extern template void ju_get_opt_field(const nlohmann::json &j, const std::string &key, query_type &value,
    const std::string &base = "params/");
extern template void ju_get_opt_field(const nlohmann::json &j, const uint64_t &key, query_type &value,
    const std::string &base = "params/");

#endif
