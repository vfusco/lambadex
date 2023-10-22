#ifndef ROLLUP_BARE_METAL_H
#define ROLLUP_BARE_METAL_H
////////////////////////////////////////////////////////////////////////////////
// Rollup utilities for bare metal execution

struct rollup_config_type {
    uint64_t lambda_virtual_start = 0;
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
        .current_query = 0,
        .current_voucher = 0,
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
    if (config.query_begin != config.query_end) {
        if (!config.query_format) {
            (void) fprintf(stderr, "[dapp] missing rollup query format\n");
            return nullptr;
        }
    }
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
    for (rollup_state->current_input = rollup_state->config.input_begin;
        rollup_state->current_input < rollup_state->config.input_end; ++rollup_state->current_input) {
        // Start report/notice/voucher counters anew
        rollup_state->current_notice = rollup_state->current_voucher = rollup_state->current_report = 0;
        char filename[FILENAME_MAX];
        // Load input metadata
        snprintf(filename, std::size(filename), rollup_state->config.input_metadata_format, rollup_state->current_input);
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
        snprintf(filename, std::size(filename), rollup_state->config.input_format, rollup_state->current_input);
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
            (void) fprintf(stderr, "Accepted input %d\n", rollup_state->current_input);
        } else {
            (void) fprintf(stderr, "Rejected input %d\n", rollup_state->current_input);
        }
    }
    for (rollup_state->current_query = rollup_state->config.query_begin;
         rollup_state->current_query < rollup_state->config.query_end; ++rollup_state->current_query) {
        // Start report/notice/voucher counters anew
        rollup_state->current_notice = rollup_state->current_voucher = rollup_state->current_report = 0;
        char filename[FILENAME_MAX];
        // Load input metadata
        snprintf(filename, std::size(filename), rollup_state->config.query_format, rollup_state->current_query);
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
            (void) fprintf(stderr, "Accepted query %d\n", rollup_state->current_query);
        } else {
            (void) fprintf(stderr, "Rejected query %d\n", rollup_state->current_query);
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
    if (rollup_state->current_input <= rollup_state->config.input_end) {
        snprintf(filename, std::size(filename), "input-%d-%s-%d.bin", rollup_state->current_input, what, index);
    } else {
        snprintf(filename, std::size(filename), "query-%d-%s-%d.bin", rollup_state->current_query, what, index);
    }
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
    if (rollup_state->current_input <= rollup_state->config.input_end) {
        snprintf(filename, std::size(filename), "input-%d-voucher-%d.bin", rollup_state->current_input,
            rollup_state->current_voucher);
    } else {
        snprintf(filename, std::size(filename), "query-%d-voucher-%d.bin", rollup_state->current_query,
            rollup_state->current_voucher);
    }
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
