#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/statvfs.h>

#define DEFAULT_INTERVAL 1
#define KIB_PER_MIB 1024.0
#define BYTES_PER_GIB (1024.0 * 1024.0 * 1024.0)

typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
} CpuTimes;

typedef struct {
    unsigned long long total_kib;
    unsigned long long available_kib;
} MemoryInfo;

typedef struct {
    unsigned long long total_bytes;
    unsigned long long used_bytes;
} DiskInfo;

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int signal_number) {
    (void)signal_number;
    keep_running = 0;
}

/* Read the aggregate CPU counters from the first line of /proc/stat. */
static int get_cpu_times(CpuTimes *times) {
    FILE *file = fopen("/proc/stat", "r");
    if (file == NULL) {
        perror("Could not open /proc/stat");
        return 0;
    }

    char label[16];
    int fields_read = fscanf(file,
        "%15s %llu %llu %llu %llu %llu %llu %llu %llu",
        label,
        &times->user, &times->nice, &times->system, &times->idle,
        &times->iowait, &times->irq, &times->softirq, &times->steal);

    fclose(file);

    if (fields_read != 9 || strcmp(label, "cpu") != 0) {
        fprintf(stderr, "Could not parse aggregate CPU counters in /proc/stat.\n");
        return 0;
    }
    return 1;
}

/* Guest counters are not added: Linux already includes them in user/nice. */
static unsigned long long total_cpu_ticks(const CpuTimes *t) {
    return t->user + t->nice + t->system + t->idle +
           t->iowait + t->irq + t->softirq + t->steal;
}

static unsigned long long idle_cpu_ticks(const CpuTimes *t) {
    return t->idle + t->iowait;
}

static double calculate_cpu_usage(const CpuTimes *previous,
                                  const CpuTimes *current) {
    unsigned long long previous_total = total_cpu_ticks(previous);
    unsigned long long current_total = total_cpu_ticks(current);
    unsigned long long previous_idle = idle_cpu_ticks(previous);
    unsigned long long current_idle = idle_cpu_ticks(current);

    if (current_total <= previous_total) {
        return 0.0;
    }

    unsigned long long total_delta = current_total - previous_total;
    unsigned long long idle_delta = current_idle - previous_idle;

    if (idle_delta > total_delta) {
        return 0.0;
    }

    return 100.0 * (double)(total_delta - idle_delta) / (double)total_delta;
}

/* Read total and available RAM from /proc/meminfo (values are in KiB). */
static int get_memory_info(MemoryInfo *memory) {
    FILE *file = fopen("/proc/meminfo", "r");
    if (file == NULL) {
        perror("Could not open /proc/meminfo");
        return 0;
    }

    char line[256];
    int found_total = 0;
    int found_available = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (sscanf(line, "MemTotal: %llu kB", &memory->total_kib) == 1) {
            found_total = 1;
        } else if (sscanf(line, "MemAvailable: %llu kB",
                          &memory->available_kib) == 1) {
            found_available = 1;
        }
    }

    fclose(file);

    if (!found_total || !found_available ||
        memory->total_kib == 0 ||
        memory->available_kib > memory->total_kib) {
        fprintf(stderr, "Could not read valid RAM values from /proc/meminfo.\n");
        return 0;
    }
    return 1;
}

/* Measure the filesystem containing path (defaults to "/"). */
static int get_disk_info(const char *path, DiskInfo *disk) {
    struct statvfs stats;

    if (statvfs(path, &stats) != 0) {
        fprintf(stderr, "Could not inspect filesystem '%s': %s\n",
                path, strerror(errno));
        return 0;
    }

    unsigned long long block_size =
        (unsigned long long)(stats.f_frsize != 0 ? stats.f_frsize : stats.f_bsize);

    disk->total_bytes = block_size * (unsigned long long)stats.f_blocks;
    disk->used_bytes = block_size *
        ((unsigned long long)stats.f_blocks - (unsigned long long)stats.f_bfree);

    if (disk->total_bytes == 0) {
        fprintf(stderr, "Filesystem '%s' reports zero total capacity.\n", path);
        return 0;
    }
    return 1;
}

static void print_dashboard(double cpu_usage, const MemoryInfo *memory,
                            const DiskInfo *disk, const char *path,
                            int interval) {
    unsigned long long used_kib = memory->total_kib - memory->available_kib;
    double memory_percent =
        100.0 * (double)used_kib / (double)memory->total_kib;
    double disk_percent =
        100.0 * (double)disk->used_bytes / (double)disk->total_bytes;

    /* ANSI escape sequences clear the terminal and move the cursor home. */
    printf("\033[2J\033[H");
    printf("============================================\n");
    printf("          LINUX SYSTEM MONITOR\n");
    printf("============================================\n");
    printf("Refresh interval: %d second(s) | Press Ctrl+C to exit\n\n", interval);

    printf("CPU\n");
    printf("  Usage:          %6.1f%%\n\n", cpu_usage);

    printf("MEMORY\n");
    printf("  Total RAM:      %6.0f MiB\n", (double)memory->total_kib / KIB_PER_MIB);
    printf("  Used RAM:       %6.0f MiB\n", (double)used_kib / KIB_PER_MIB);
    printf("  Usage:          %6.1f%%\n\n", memory_percent);

    printf("FILESYSTEM: %s\n", path);
    printf("  Total space:    %6.2f GiB\n", (double)disk->total_bytes / BYTES_PER_GIB);
    printf("  Used space:     %6.2f GiB\n", (double)disk->used_bytes / BYTES_PER_GIB);
    printf("  Usage:          %6.1f%%\n", disk_percent);
    printf("============================================\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    const char *filesystem_path = "/";
    int interval = DEFAULT_INTERVAL;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [filesystem-path]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc == 2) {
        filesystem_path = argv[1];
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) == -1) {
        perror("Could not set Ctrl+C handler");
        return EXIT_FAILURE;
    }

    CpuTimes previous_cpu;
    if (!get_cpu_times(&previous_cpu)) {
        return EXIT_FAILURE;
    }

    while (keep_running) {
        struct timespec delay = { .tv_sec = interval, .tv_nsec = 0 };
        while (nanosleep(&delay, &delay) == -1 && errno == EINTR && keep_running) {
            /* Resume the remaining sleep unless Ctrl+C was pressed. */
        }
        if (!keep_running) {
            break;
        }

        CpuTimes current_cpu;
        MemoryInfo memory = {0};
        DiskInfo disk = {0};

        if (!get_cpu_times(&current_cpu) ||
            !get_memory_info(&memory) ||
            !get_disk_info(filesystem_path, &disk)) {
            return EXIT_FAILURE;
        }

        double cpu_usage = calculate_cpu_usage(&previous_cpu, &current_cpu);
        print_dashboard(cpu_usage, &memory, &disk, filesystem_path, interval);
        previous_cpu = current_cpu;
    }

    printf("\nMonitor stopped.\n");
    return EXIT_SUCCESS;
}
