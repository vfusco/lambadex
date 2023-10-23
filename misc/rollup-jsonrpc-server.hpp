#ifndef ROLLUP_BARE_METAL_SERVER_H
#define ROLLUP_BARE_METAL_SERVER_H
////////////////////////////////////////////////////////////////////////////////
// Rollup utilities for bare metal server

#include <algorithm>
#include <array>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include "mongoose.h"
#include "nlohmann/json.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "json-util.h"

struct rollup_config_type {
    uint64_t lambda_virtual_start = 0;
    const char *image_filename = nullptr;
    const char *server_address = nullptr;
};

using namespace std::string_literals;
using json = nlohmann::json;

/// \brief Installs a signal handler
template <typename HANDLER>
static void install_signal_handler(int signum, HANDLER handler) {
    struct sigaction act {};
    if (sigemptyset(&act.sa_mask) < 0) {
        throw std::system_error{errno, std::generic_category(), "sigemptyset failed"};
    }
    act.sa_handler = handler; // NOLINT(cppcoreguidelines-pro-type-union-access)
    act.sa_flags = SA_RESTART;
    if (sigaction(signum, &act, nullptr) < 0) {
        throw std::system_error{errno, std::generic_category(), "sigaction failed"};
    }
}

/// \brief Installs all signal handlers
static void install_signal_handlers(void) {
    // Prevent this process from crashing on SIGPIPE when remote connection is closed
    install_signal_handler(SIGPIPE, SIG_IGN);
}

/// \brief Closes event manager without interfering with other processes
/// \param event_manager Pointer to event manager to close
/// \detail Mongoose's mg_mgr_free removes all sockets from the epoll_fd, which affects other processes.
/// We close the epoll_fd first to prevent this problem
static void mg_mgr_free_ours(mg_mgr *event_manager) {
#ifdef MG_ENABLE_EPOLL
    // Prevent destruction of manager from affecting the epoll state of parent
    close(event_manager->epoll_fd);
    event_manager->epoll_fd = -1;
#endif
    mg_mgr_free(event_manager);
}

/// \brief HTTP handler status
enum class http_handler_status {
    ready_for_next, ///< Ready for next request in loop
    shutdown        ///< Previous request was for shutdown
};

struct rollup_state_type {
    void *lambda;
    size_t lambda_length;
    rollup_config_type config;
    http_handler_status status;                ///< Status of last request
    mg_mgr event_manager;                      ///< Mongoose event manager
    json reports;
};

/// \brief Forward declaration of http handler
static void http_handler(mg_connection *con, int ev, void *ev_data, void *h_data);

/// \brief Names for JSONRPC error codes
enum jsonrpc_error_code : int {
    parse_error = -32700,      ///< When the request failed to parse
    invalid_request = -32600,  ///< When the request was invalid (missing fields, wrong types etc)
    method_not_found = -32601, ///< When the method was not found
    invalid_params = -32602,   ///< When the parameters provided don't meet the method's needs
    internal_error = -32603,   ///< When there was an internal error (runtime etc)
    server_error = -32000      ///< When there was some problem with the server itself
};

/// \brief Returns a successful JSONRPC response as a JSON object
/// \param j JSON request, from which an id is obtained
/// \param result Result to send in response (defaults to true)
/// \returns JSON object with response
static json jsonrpc_response_ok(const json &j, const json &result = true) {
    return {{"jsonrpc", "2.0"}, {"id", j.contains("id") ? j["id"] : json{nullptr}}, {"result", result}};
}

/// \brief Returns a failed JSONRPC response as a JSON object
/// \param j JSON request, from which an id is obtained
/// \param code JSONRPC Error code
/// \param message Error message
/// \returns JSON object with response
static json jsonrpc_response_error(const json &j, jsonrpc_error_code code, const std::string &message) {
    return {{"jsonrpc", "2.0"}, {"id", j.contains("id") ? j["id"] : json{nullptr}},
        {"error", {{"code", code}, {"message", message}}}};
}

/// \brief Returns a parse error JSONRPC response as a JSON object
/// \param message Error message
/// \returns JSON object with response
static json jsonrpc_response_parse_error(const std::string &message) {
    return jsonrpc_response_error(nullptr, jsonrpc_error_code::parse_error, message);
}

/// \brief Returns an invalid request JSONRPC response as a JSON object
/// \param j JSON request, from which an id is obtained
/// \param message Error message
/// \returns JSON object with response
static json jsonrpc_response_invalid_request(const json &j, const std::string &message) {
    return jsonrpc_response_error(j, jsonrpc_error_code::invalid_request, message);
}

/// \brief Returns an internal error JSONRPC response as a JSON object
/// \param j JSON request, from which an id is obtained
/// \param message Error message
/// \returns JSON object with response
static json jsonrpc_response_internal_error(const json &j, const std::string &message) {
    return jsonrpc_response_error(j, jsonrpc_error_code::internal_error, message);
}

/// \brief Returns a server error JSONRPC response as a JSON object
/// \param j JSON request, from which an id is obtained
/// \param message Error message
/// \returns JSON object with response
static json jsonrpc_response_server_error(const json &j, const std::string &message) {
    return jsonrpc_response_error(j, jsonrpc_error_code::server_error, message);
}

/// \brief Returns a method not found JSONRPC response as a JSON object
/// \param j JSON request, from which an id is obtained
/// \param message Error message
/// \returns JSON object with response
static json jsonrpc_response_method_not_found(const json &j, const std::string &message) {
    return jsonrpc_response_error(j, jsonrpc_error_code::method_not_found, message);
}

/// \brief Returns a invalid params JSONRPC response as a JSON object
/// \param j JSON request, from which an id is obtained
/// \param message Error message
/// \returns JSON object with response
static json jsonrpc_response_invalid_params(const json &j, const std::string &message) {
    return jsonrpc_response_error(j, jsonrpc_error_code::invalid_params, message);
}

/// \brief Checks that a JSON object contains only fields with allowed keys
/// \param j JSON object to test
/// \param keys Set of allowed keys
static void jsonrpc_check_allowed_fields(const json &j, const std::unordered_set<std::string> &keys,
    const std::string &base = "params/") {
    for (const auto &[key, val] : j.items()) {
        if (keys.find(key) == keys.end()) {
            // NOLINTNEXTLINE(performance-inefficient-string-concatenation)
            throw std::invalid_argument("unexpected field \"/"s + base + key + "\""s);
        }
    }
}

/// \brief Checks that a JSON object all fields with given keys
/// \param j JSON object to test
/// \param keys Set of mandatory keys
static void jsonrpc_check_mandatory_fields(const json &j, const std::unordered_set<std::string> &keys,
    const std::string &base = "params/") {
    for (const auto &key : keys) {
        if (!j.contains(key)) {
            // NOLINTNEXTLINE(performance-inefficient-string-concatenation)
            throw std::invalid_argument("missing field \"/"s + base + key + "\""s);
        }
    }
}

/// \brief Checks that a JSON object contains no fields
/// \param j JSON object to test
static void jsonrpc_check_no_params(const json &j) {
    if (j.contains("params") && !j["params"].empty()) {
        throw std::invalid_argument("unexpected \"params\" field");
    }
}

/// \brief Type trait to test if a type has been encapsulated in an optional_param
/// \tparam T type to test
/// \details This is the default case
template <typename T>
struct is_optional_param : std::false_type {};

/// \brief Type trait to test if a type has been encapsulated in an optional_param
/// \tparam T type to test
/// \details This is the encapsulated case
template <typename T>
struct is_optional_param<optional_param<T>> : std::true_type {};

/// \brief Shortcut to the type trait to test if a type has been encapsulated in an optional_param
/// \tparam T type to test
template <typename T>
inline constexpr bool is_optional_param_v = is_optional_param<T>::value;

/// \brief Counts the number of parameters that are mandatory (i.e., not wrapped in optional_param)
/// \tparam ARGS Parameter pack to test
/// \returns Number of parameters wrapped in optional_param
template <typename... ARGS>
constexpr size_t count_mandatory_params(void) {
    return ((is_optional_param_v<ARGS> ? 0 : 1) + ... + 0);
}

/// \brief Returns index of the first parameter that is optional (i.e., wrapped in optional_param)
/// \tparam ARGS Parameter pack to test
/// \tparam I Parameter pack with indices of each parameter
/// \returns Index of first parameter that is optional
template <typename... ARGS, size_t... I>
size_t first_optional_param(const std::tuple<ARGS...> &, std::index_sequence<I...>) {
    if constexpr (sizeof...(ARGS) > 0) {
        return std::min({(is_optional_param_v<ARGS> ? I + 1 : sizeof...(ARGS) + 1)...});
    } else {
        return sizeof...(ARGS) + 1;
    }
}

/// \brief Returns index of the first parameter that is optional (i.e., wrapped in optional_param)
/// \tparam ARGS Parameter pack to test
/// \tparam I Parameter pack with indices of each parameter
/// \returns Index of first parameter that is optional
template <typename... ARGS, size_t... I>
size_t last_mandatory_param(const std::tuple<ARGS...> &, std::index_sequence<I...>) {
    if constexpr (sizeof...(ARGS) > 0) {
        return std::max({(!is_optional_param_v<ARGS> ? I + 1 : 0)...});
    } else {
        return 0;
    }
}

/// \brief Checks if an argument has a value
/// \tparam T Type of argument
/// \param t Argument
/// \returns True if it has a value
/// \details This is the default overload
template <typename T>
bool has_arg(const T &t) {
    (void) t;
    return true;
}

/// \brief Checks if an argument has a value
/// \tparam T Type of argument
/// \param t Argument
/// \returns True if it has a value
/// \details This is the overload for optional paramenters (i.e., wrapped in optional_param)
template <typename T>
bool has_arg(const optional_param<T> &t) {
    return t.has_value();
}

/// \brief Finds the index of the first missing optional argument (i.e., wrapped in optional_param)
/// \tparam ARGS Parameter pack with parameter types
/// \tparam I Parameter pack with indices of each parameter
/// \returns Index of first optional argument that is missing
/// \details The function returns the index + 1, so that sizeof...(ARGS)+1 means no missing optional arguments
template <typename... ARGS, size_t... I>
size_t first_missing_optional_arg(const std::tuple<ARGS...> &tup, std::index_sequence<I...>) {
    if constexpr (sizeof...(ARGS) > 0) {
        return std::min({(!has_arg(std::get<I>(tup)) ? I + 1 : sizeof...(ARGS) + 1)...});
    } else {
        return 1;
    }
}

/// \brief Finds the index of the last argument that is present
/// \tparam ARGS Parameter pack with parameter types
/// \tparam I Parameter pack with indices of each parameter
/// \returns Index of last argument that is present
/// \details The function returns the index + 1, so that 0 means no arguments are present
template <typename... ARGS, size_t... I>
size_t last_present_arg(const std::tuple<ARGS...> &tup, std::index_sequence<I...>) {
    if constexpr (sizeof...(ARGS) > 0) {
        return std::max({(has_arg(std::get<I>(tup)) ? I + 1 : 0)...});
    } else {
        return 0;
    }
}

/// \brief Counts the number of arguments provided
/// \tparam ARGS Parameter pack with parameter types
/// \tparam I Parameter pack with indices of each parameter
/// \param tup Tupple with all arguments
/// \param i Index sequence
/// \returns Number of arguments provided
template <typename... ARGS, size_t... I>
size_t count_args(const std::tuple<ARGS...> &tup, const std::index_sequence<I...> &i) {
    // check first optional parameter happens after last mandatory parameter
    auto fop = first_optional_param(tup, i);
    auto lmp = last_mandatory_param(tup, i);
    if (fop <= lmp) {
        throw std::invalid_argument{"first optional parameter must come after last mandatory parameter"};
    }
    // make sure last present optional argument comes before first missing optional argument
    auto fmoa = first_missing_optional_arg(tup, i);
    auto lpa = last_present_arg(tup, i);
    if (lpa >= fmoa) {
        throw std::invalid_argument{"first missing optional argument must come after last present argument"};
    }
    return std::max(lmp, lpa);
}

/// \brief Counts the number of arguments provided
/// \tparam ARGS Parameter pack with parameter types
/// \param tup Tupple with all arguments
/// \returns Number of arguments provided
template <typename... ARGS>
size_t count_args(const std::tuple<ARGS...> &tup) {
    return count_args(tup, std::make_index_sequence<sizeof...(ARGS)>{});
}

/// \brief Parse arguments from an array
/// \tparam ARGS Parameter pack with parameter types
/// \tparam I Parameter pack with indices of each parameter
/// \param j JSONRPC request params
/// \returns tuple with arguments
template <typename... ARGS, size_t... I>
std::tuple<ARGS...> parse_array_args(const json &j, std::index_sequence<I...>) {
    std::tuple<ARGS...> tp;
    (ju_get_field(j, static_cast<uint64_t>(I), std::get<I>(tp)), ...);
    return tp;
}

/// \brief Parse arguments from an array
/// \tparam ARGS Parameter pack with parameter types
/// \param j JSONRPC request params
/// \returns tuple with arguments
template <typename... ARGS>
std::tuple<ARGS...> parse_array_args(const json &j) {
    return parse_array_args<ARGS...>(j, std::make_index_sequence<sizeof...(ARGS)>{});
}

/// \brief Parse arguments from an object
/// \tparam ARGS Parameter pack with parameter types
/// \tparam I Parameter pack with indices of each parameter
/// \param j JSONRPC request params
/// \param param_name Name of each parameter
/// \returns tuple with arguments
template <typename... ARGS, size_t... I>
std::tuple<ARGS...> parse_object_args(const json &j, const char *(&param_name)[sizeof...(ARGS)],
    std::index_sequence<I...>) {
    std::tuple<ARGS...> tp;
    (ju_get_field(j, std::string(param_name[I]), std::get<I>(tp)), ...);
    return tp;
}

/// \brief Parse arguments from an object
/// \tparam ARGS Parameter pack with parameter types
/// \param j JSONRPC request params
/// \param param_name Name of each parameter
/// \returns tuple with arguments
template <typename... ARGS>
std::tuple<ARGS...> parse_object_args(const json &j, const char *(&param_name)[sizeof...(ARGS)]) {
    return parse_object_args<ARGS...>(j, param_name, std::make_index_sequence<sizeof...(ARGS)>{});
}

/// \brief Parse arguments from an object or array
/// \tparam ARGS Parameter pack with parameter types
/// \param j JSONRPC request params
/// \param param_name Name of each parameter
/// \returns tuple with arguments
template <typename... ARGS>
std::tuple<ARGS...> parse_args(const json &j, const char *(&param_name)[sizeof...(ARGS)]) {
    constexpr auto mandatory_params = count_mandatory_params<ARGS...>();
    if (!j.contains("params")) {
        if constexpr (mandatory_params == 0) {
            return std::make_tuple(ARGS{}...);
        }
        throw std::invalid_argument("missing field \"params\"");
    }
    const json &params = j["params"];
    if (!params.is_object() && !params.is_array()) {
        throw std::invalid_argument("\"params\" field not object or array");
    }
    if (params.is_object()) {
        //??D This could be optimized so we don't construct these sets every call
        jsonrpc_check_mandatory_fields(params,
            std::unordered_set<std::string>{param_name, param_name + mandatory_params});
        jsonrpc_check_allowed_fields(params, std::unordered_set<std::string>{param_name, param_name + sizeof...(ARGS)});
        return parse_object_args<ARGS...>(params, param_name);
    }
    if (params.size() < mandatory_params) {
        throw std::invalid_argument("not enough entries in \"params\" array");
    }
    if (params.size() > sizeof...(ARGS)) {
        throw std::invalid_argument("too many entries in \"params\" array");
    }
    return parse_array_args<ARGS...>(params);
}

/// \brief JSONRPC handler for the shutdown method
/// \param j JSON request object
/// \param con Mongoose connection
/// \param h Handler data
/// \returns JSON response object
static json jsonrpc_shutdown_handler(const json &j, mg_connection *con, rollup_state_type *h) {
    (void) h;
    jsonrpc_check_no_params(j);
    con->is_draining = 1;
    con->data[0] = 'X';
    return jsonrpc_response_ok(j);
}

/// \brief JSONRPC handler for the inspect method
/// \param j JSON request object
/// \param con Mongoose connection
/// \param h Handler data
/// \returns JSON response object
static json jsonrpc_inspect_handler(const json &j, mg_connection *con, rollup_state_type *h) {
    (void) con;
    static const char *param_name[] = {"query"};
    auto args = parse_args<query_type>(j, param_name);
    h->reports = json::array();
    bool ret = inspect_state(h, reinterpret_cast<lambda_type *>(h->lambda), std::get<0>(args), sizeof(query_type));
    return jsonrpc_response_ok(j, {{"reports", h->reports }, { "accept", ret} });
}

/// \brief Sends a JSONRPC response through the Mongoose connection
/// \param con Mongoose connection
/// \param j JSON response object
void jsonrpc_http_reply(mg_connection *con, rollup_state_type *h, const json &j) {
    std::cerr << "\t-> " << j.dump().data() << "\n";
    return mg_http_reply(con, 200, "Access-Control-Allow-Origin: *\r\nContent-Type: application/json\r\n", "%s",
        j.dump().data());
}

/// \brief Sends an empty response through the Mongoose connection
/// \param con Mongoose connection
void jsonrpc_send_empty_reply(mg_connection *con, rollup_state_type *h) {
    std::cerr << "\t ->\n";
    return mg_http_reply(con, 200, "Access-Control-Allow-Origin: *\r\nContent-Type: application/json\r\n", "");
}

/// \brief jsonrpc handler is a function pointer
using jsonrpc_handler = json (*)(const json &ji, mg_connection *con, rollup_state_type *h);

/// \brief Dispatch request to appropriate JSONRPC handler
/// \param j JSON request object
/// \param con Mongoose connection
/// \param h_data Handler data
/// \returns JSON with response
static json jsonrpc_dispatch_method(const json &j, mg_connection *con, rollup_state_type *h) try {
    static const std::unordered_map<std::string, jsonrpc_handler> dispatch = {
        {"shutdown", jsonrpc_shutdown_handler},
        {"inspect", jsonrpc_inspect_handler},
    };
    auto method = j["method"].get<std::string>();
    auto found = dispatch.find(method);
    if (found != dispatch.end()) {
        return found->second(j, con, h);
    }
    return jsonrpc_response_method_not_found(j, method);
} catch (std::invalid_argument &x) {
    return jsonrpc_response_invalid_params(j, x.what());
} catch (std::exception &x) {
    return jsonrpc_response_internal_error(j, x.what());
}

/// \brief Handler for HTTP requests
/// \param con Mongoose connection
/// \param ev Mongoose event
/// \param ev_data Mongoose event data
/// \param h_data Handler data
static void http_handler(mg_connection *con, int ev, void *ev_data, void *h_data) {
    auto *h = static_cast<rollup_state_type *>(h_data);
    if (ev == MG_EV_HTTP_MSG) {
        auto *hm = static_cast<mg_http_message *>(ev_data);
        const std::string_view method{hm->method.ptr, hm->method.len};
        // Answer OPTIONS request to support cross origin resource sharing (CORS) preflighted browser requests
        if (method == "OPTIONS") {
            std::string headers;
            headers += "Access-Control-Allow-Origin: *\r\n";
            headers += "Access-Control-Allow-Methods: *\r\n";
            headers += "Access-Control-Allow-Headers: *\r\n";
            headers += "Access-Control-Max-Age: 0\r\n";
            mg_http_reply(con, 204, headers.c_str(), "");
            return;
        }
        // Only accept POST requests
        if (method != "POST") {
            std::string headers;
            headers += "Access-Control-Allow-Origin: *\r\n";
            mg_http_reply(con, 405, headers.c_str(), "method not allowed");
            return;
        }
        // Only accept / URI
        const std::string_view uri{hm->uri.ptr, hm->uri.len};
        std::cerr << h->config.server_address << " <- " << std::string_view{hm->body.ptr, hm->body.len} << "\n";
        if (uri != "/") {
            std::cerr << h->config.server_address << " rejected unexpected \"" << uri << "\" uri\n";
            // anything else
            mg_http_reply(con, 404, "Access-Control-Allow-Origin: *\r\n", "not found");
            return;
        }
        // Parse request body into a JSON object
        json j;
        try {
            j = json::parse(hm->body.ptr, hm->body.ptr + hm->body.len);
        } catch (std::exception &x) {
            return jsonrpc_http_reply(con, h, jsonrpc_response_parse_error(x.what()));
        }
        // JSONRPC allows batch requests, each an entry in an array
        // We deal uniformly with batch and singleton requests by wrapping the singleton into a batch
        auto was_array = j.is_array();
        if (!was_array) {
            j = json::array({std::move(j)});
        }
        if (j.empty()) {
            return jsonrpc_http_reply(con, h, jsonrpc_response_invalid_request(j, "empty batch request array"));
        }
        json jr;
        // Obtain response to each request in batch
        for (auto ji : j) {
            if (!ji.is_object()) {
                jr.push_back(jsonrpc_response_invalid_request(ji, "request not an object"));
                continue;
            }
            if (!ji.contains("jsonrpc")) {
                jr.push_back(jsonrpc_response_invalid_request(ji, "missing field \"jsonrpc\""));
                continue;
            }
            if (!ji["jsonrpc"].is_string() || ji["jsonrpc"] != "2.0") {
                jr.push_back(jsonrpc_response_invalid_request(ji, R"(invalid field "jsonrpc" (expected "2.0"))"));
                continue;
            }
            if (!ji.contains("method")) {
                jr.push_back(jsonrpc_response_invalid_request(ji, "missing field \"method\""));
                continue;
            }
            if (!ji["method"].is_string() || ji["method"].get<std::string>().empty()) {
                jr.push_back(
                    jsonrpc_response_invalid_request(ji, "invalid field \"method\" (expected non-empty string)"));
                continue;
            }
            // check for valid id
            if (ji.contains("id")) {
                const auto &jiid = ji["id"];
                if (!jiid.is_string() && !jiid.is_number() && !jiid.is_null()) {
                    jr.push_back(jsonrpc_response_invalid_request(ji,
                        "invalid field \"id\" (expected string, number, or null)"));
                }
            }
            json jri = jsonrpc_dispatch_method(ji, con, h);
            // Except for errors, do not add result of "notification" requests
            if (ji.contains("id")) {
                jr.push_back(std::move(jri));
            }
        }
        // Unwrap singleton request from batch, if it was indeed a singleton
        // Otherwise, just send the response
        if (!jr.empty()) {
            if (was_array) {
                return jsonrpc_http_reply(con, h, jr);
            }
            return jsonrpc_http_reply(con, h, jr[0]);
        }
        return jsonrpc_send_empty_reply(con, h);
    }
    if (ev == MG_EV_CLOSE) {
        if (con->data[0] == 'X') {
            h->status = http_handler_status::shutdown;
            return;
        }
    }
    if (ev == MG_EV_ERROR) {
        return;
    }
}

rollup_state_type *rollup_open(const rollup_config_type &config) {
    static rollup_state_type rollup_state{
        .lambda = nullptr,
        .lambda_length = 0
    };
    rollup_state.config = config;
    if (!config.image_filename) {
        (void) fprintf(stderr, "[dapp] missing image filename\n");
        return nullptr;
    }
    if (!config.server_address) {
        (void) fprintf(stderr, "[dapp] missing server address\n");
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

    install_signal_handlers();

    mg_mgr_init(&rollup_state.event_manager);
#if MG_ENABLE_EPOLL
    // Event manager initialization does not return whether it failed or not
    // It could only fail if the epoll_fd allocation failed
    if (rollup_state.event_manager.epoll_fd < 0) {
        mg_mgr_free(&rollup_state.event_manager);
        munmap(rollup_state.lambda, rollup_state.lambda_length);
        return nullptr;
    }
#endif
    const auto *con = mg_http_listen(&rollup_state.event_manager, config.server_address, http_handler, &rollup_state);
    if (!con) {
        mg_mgr_free(&rollup_state.event_manager);
        munmap(rollup_state.lambda, rollup_state.lambda_length);
        return nullptr;
    }
    return &rollup_state;
}

// Process rollup requests until there are no more inputs.
template <typename LAMBDA, typename ADVANCE_INPUT, typename INSPECT_QUERY, typename ADVANCE_STATE,
    typename INSPECT_STATE>
static int rollup_request_loop(rollup_state_type *rollup_state, ADVANCE_STATE advance_cb, INSPECT_STATE inspect_cb) {
    for ( ;; ) {
        rollup_state->status = http_handler_status::ready_for_next;
        mg_mgr_poll(&rollup_state->event_manager, 10000);
        switch (rollup_state->status) {
            case http_handler_status::shutdown:
                mg_mgr_free(&rollup_state->event_manager);
                munmap(rollup_state->lambda, rollup_state->lambda_length);
                return 0;
            case http_handler_status::ready_for_next:
            default:
                break;
        }
    }
}

[[nodiscard, maybe_unused]] static bool rollup_write_report(rollup_state_type *rollup_state, const report_type &report) {
    rollup_state->reports.push_back(report);
    return true;
}

template <typename T>
[[nodiscard, maybe_unused]] static bool rollup_write_notice(rollup_state_type *rollup_state, const T &payload) {
    return true;
}

template <typename T>
[[nodiscard, maybe_unused]] static bool rollup_write_voucher(rollup_state_type *rollup_state,
    const eth_address &destination, const T &payload) {
    return true;
}

#endif
