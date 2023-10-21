#ifndef ROLLUP_EMULATOR_H
#define ROLLUP_EMULATOR_H

////////////////////////////////////////////////////////////////////////////////
// Rollup utilities for emulated execution

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

#endif
