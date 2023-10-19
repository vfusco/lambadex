#include <filesystem>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

constexpr  uint64_t LAMBDA_VIRTUAL_START = UINT64_C(0x1000000000);

class memory_arena {
    uint64_t m_length;
    uint64_t m_next_free;
    unsigned char m_data[0];
public:
    memory_arena(uint64_t length) : m_length(length), m_next_free(0) { }
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
    void deallocate(void *p, int length) {
    }
    uint64_t get_data_length() {
        return m_length - offsetof(memory_arena, m_data);
    }
    
};

extern memory_arena *arena;

template <typename T>
class arena_allocator {
public:
    arena_allocator() = default;
    using pointer = T*;
    using value_type = T;
    pointer allocate(std::size_t n) {
        return static_cast<pointer>(arena->allocate(n * sizeof(T)));    
    }
    void deallocate(pointer p, std::size_t n) noexcept {
        arena->deallocate(p, n * sizeof(T));
    }
};

class lambda_state {
    int m_fd;    
    uint64_t m_length = 0;    
    void *m_state;

public:
    lambda_state(const char *file_name) {
        m_length = std::filesystem::file_size(file_name);
        m_fd = open(file_name, O_RDWR);
        m_state = mmap((void*)LAMBDA_VIRTUAL_START, m_length,  PROT_WRITE | PROT_READ,  MAP_SHARED | MAP_NOEXTEND  | MAP_FIXED, m_fd, 0);
        if (m_state == MAP_FAILED) {
            throw std::runtime_error(std::string("mmap - ") + std::strerror(errno));
        }
        if (m_state != reinterpret_cast<void*>(LAMBDA_VIRTUAL_START)) {
            close(m_fd);
            throw std::runtime_error("mmap - unexpected address"); 
    }
    }

    lambda_state(uint64_t phys_start, uint64_t length) : m_length(length) {
        m_length = length;
        m_fd = open("/dev/mem", O_RDWR);
        m_state = mmap((void*)LAMBDA_VIRTUAL_START, m_length,  PROT_WRITE | PROT_READ,  MAP_SHARED | MAP_NOEXTEND  | MAP_FIXED, m_fd, phys_start);
        if (m_state == MAP_FAILED) {
            throw std::runtime_error(std::string("mmap - ") + std::strerror(errno));
        }
        if (m_state != reinterpret_cast<void*>(LAMBDA_VIRTUAL_START)) {
            close(m_fd);
            throw std::runtime_error("mmap - unexpected address"); 
        }
    }

    virtual ~lambda_state() {
        printf("munmap\n");
        munmap(m_state, m_length);
        close(m_fd);
    }

    uint64_t get_length() {
        return m_length;
    }
    void *get_state() {
        return m_state;
    }

};
