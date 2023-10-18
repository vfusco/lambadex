//
// compile this file for RISC-V with name 'mmap-test', copy to fs.ext2
// create image file 'lambda.bin' using 'truncate --size 64K lambda.bin'
// run cartesi machine as such
/*
cartesi-machine \
  --append-dtb-bootargs="single=yes" \
  --flash-drive=label:fs,filename:fs.ext2 \
  --flash-drive=label:lambda,filename:lambda.bin,shared \
  -- '/mnt/fs/mmap-test.emulator --emulated --flash-drive-label=lambda'
*/
// 'lambda.bin' now contains a linked list that skips over empty space, using native pointers
// now compile this file for the host and run
// mmap-test --bare-metal --image-filename=lambda.bin
// the program will map 'lambda.bin' over the same virtual space as it was in the cartesi machine and traverse
// the linked list printing the elements
//
#include <iostream>
#include <cstdint>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

constexpr uint64_t LAMBDA_VIRTUAL_START = UINT64_C(0x8000000); // something that worked

enum class mode_what {
    bare_metal,
    emulated
};

struct t_list {
    int value;
    t_list *next;
};

static int bare_metal(uint64_t lambda_virt_start, const char *image_filename) {
    int memfd = open(image_filename, O_RDWR);
    if (memfd < 0) {
        std::cerr << "open failed for '" << image_filename << "'\n";
        std::cerr << strerror(errno) << '\n';
        return 1;
    }
    auto off = lseek(memfd, 0, SEEK_END);
    if (off < 0) {
        std::cerr << "unable to get device length\n";
        std::cerr << strerror(errno) << '\n';
    }
    size_t length = static_cast<size_t>(off);
    std::cerr << "opened '" << image_filename << "'\n";
    void *lambda = mmap(reinterpret_cast<void *>(lambda_virt_start), length, PROT_WRITE | PROT_READ,
        MAP_SHARED | MAP_FIXED_NOREPLACE, memfd, 0);
    close(memfd);
    if (lambda == MAP_FAILED) {
        std::cerr << "mmap failed\n";
        std::cerr << strerror(errno) << '\n';
        return 1;
    }
    if (lambda != reinterpret_cast<void *>(lambda_virt_start)) {
        std::cerr << "mmap mapped wrong virtual address\n";
        munmap(lambda, length);
        return 1;
    }
    auto f = std::cerr.flags();
    std::cerr << "virtual start: " << std::hex << std::setfill('0') << std::setw(16) << lambda_virt_start << '\n';
    std::cerr << "length: " << std::hex << std::setfill('0') << std::setw(16) << length << '\n';
    std::cerr.flags(f);
    t_list *node = reinterpret_cast<t_list *>(lambda);
    std::cout << "list items are:\n";
    while (node) {
        std::cout << "\t" << node->value << '\n';
        node = node->next;
    }
    munmap(lambda, length);
    return 0;
}

static int emulated(uint64_t lambda_virt_start, const char *flash_drive_label) {
    char cmd[512];
    sprintf(cmd, "flashdrive \"%.256s\"", flash_drive_label);
    std::cerr << "running '" << cmd << "'\n";
    auto *fin = popen(cmd, "r");
    if (fin == nullptr) {
        std::cerr << "popen failed for '" << cmd << "'\n";
        std::cerr << strerror(errno) << '\n';
        return 1;
    }
    char *flash_drive_device = nullptr;
    size_t dummy_n = 0;
    std::cerr << "running emulated\n";
    if (getline(&flash_drive_device, &dummy_n, fin) < 0) {
        std::cerr << "unable to get flash-drive device from label '" << flash_drive_label << "'\n";
        std::cerr << strerror(errno) << '\n';
        pclose(fin);
        free(flash_drive_device);
    };
    dummy_n = strlen(flash_drive_device);
    if (dummy_n > 0 && flash_drive_device[dummy_n-1] == '\n') {
        flash_drive_device[dummy_n-1] = '\0';
    }
    if (int ret = pclose(fin); ret != 0) {
        std::cerr << "'" << cmd << "' returned " << ret << "\n";
        std::cerr << strerror(errno) << '\n';
        free(flash_drive_device);
        return 1;
    }
    std::cerr << "device for flash-drive label '" << flash_drive_label << "' is '" << flash_drive_device << "'\n";
    int memfd = open(flash_drive_device, O_RDWR);
    if (memfd < 0) {
        std::cerr << "failed to open '" << flash_drive_device << "'\n";
        std::cerr << strerror(errno) << '\n';
        return 1;
    }
    std::cerr << "opened '" << flash_drive_device << "'\n";
    free(flash_drive_device);
    auto off = lseek(memfd, 0, SEEK_END);
    if (off < 0) {
        std::cerr << "unable to get device length\n";
        std::cerr << strerror(errno) << '\n';
    }
    size_t length = static_cast<size_t>(off);
    void *lambda = mmap(reinterpret_cast<void *>(lambda_virt_start), length, PROT_WRITE | PROT_READ,
        MAP_SHARED | MAP_FIXED_NOREPLACE, memfd, 0);
    close(memfd);
    if (lambda == MAP_FAILED) {
        std::cerr << "mmap failed\n";
        std::cerr << strerror(errno) << '\n';
        return 1;
    }
    if (lambda != reinterpret_cast<void *>(lambda_virt_start)) {
        std::cerr << "mmap mapped wrong virtual address\n";
        munmap(lambda, length);
        return 1;
    }
    auto f = std::cerr.flags();
    std::cerr << "virtual start: " << std::hex << std::setfill('0') << std::setw(16) << lambda_virt_start << '\n';
    std::cerr << "length: " << std::hex << std::setfill('0') << std::setw(16) << length << '\n';
    std::cerr.flags(f);
    int value = 0;
    auto *node = reinterpret_cast<t_list *>(lambda);
    auto *last = reinterpret_cast<t_list *>(lambda_virt_start+length-sizeof(t_list));
    std::cerr << "building list\n";
    while (1) {
        node->value = ++value;
        auto *next = node + 3;
        if (next < last) {
            node->next = next;
        } else {
            node->next = nullptr;
            break;
        }
	    node = next;
    }
    munmap(lambda, length);
    std::cerr << "done\n";
    return 1;
}

int main(int argc, char *argv[]) {
    uint64_t lambda_virt_start = LAMBDA_VIRTUAL_START;
    const char *image_filename = nullptr;
    const char *flash_drive_label = nullptr;
    int end = 0;
    mode_what mode = mode_what::bare_metal;
    for (int i = 1; i < argc; ++i) {
        end = 0;
        if (sscanf(argv[i], "--image-filename=%n", &end) == 0 && end != 0) {
            image_filename = argv[i]+end;
        } else if (sscanf(argv[i], "--flash-drive-label=%n", &end) == 0 && end != 0) {
            flash_drive_label = argv[i]+end;
        } else if (sscanf(argv[i], "--lambda-virtual-start=0x%" SCNx64 "%n", &lambda_virt_start, &end) == 1 &&
            argv[i][end] == 0) {
            ;
        } else if (sscanf(argv[i], "--lambda-virtual-start=%" SCNu64 "%n", &lambda_virt_start, &end) == 1 &&
            argv[i][end] == 0) {
            ;
        } else if (strcmp(argv[i], "--bare-metal") == 0) {
            mode = mode_what::bare_metal;
        } else if (strcmp(argv[i], "--emulated") == 0) {
            mode = mode_what::emulated;
        } else {
            std::cerr << "invalid argument '" << argv[i] << "'\n";
        }
    }
    if (mode == mode_what::bare_metal) {
        if (image_filename == nullptr) {
            std::cerr << "missing image-filename argument\n";
            return 1;
        }
        return bare_metal(lambda_virt_start, image_filename);
    } else {
        if (flash_drive_label == nullptr) {
            std::cerr << "missing flash-drive-label argument\n";
            return 1;
        }
        return emulated(lambda_virt_start, flash_drive_label);
    }
    return 0;
}
