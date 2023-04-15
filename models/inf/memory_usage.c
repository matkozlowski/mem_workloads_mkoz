#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

typedef struct {
    time_t timestamp;
    unsigned long memory_usage_kb;
} MemoryUsageEntry;

MemoryUsageEntry buffer[BUFFER_SIZE];
size_t buffer_pos = 0;

void write_buffer_to_file() {
    FILE *file = fopen("memory_usage_stats.txt", "w");
    if (file) {
        for (size_t i = 0; i < buffer_pos; i++) {
            fprintf(file, "%ld %lu\n", buffer[i].timestamp, buffer[i].memory_usage_kb);
        }
        fclose(file);
    } else {
        perror("fopen");
    }
}

void sigint_handler(int sig) {
    write_buffer_to_file();
    exit(0);
}

void buffer_store(unsigned long memory_usage_kb) {
    buffer[buffer_pos].timestamp = time(NULL);
    buffer[buffer_pos].memory_usage_kb = memory_usage_kb;
    buffer_pos++;
    if (buffer_pos >= BUFFER_SIZE) {
        write_buffer_to_file();
        buffer_pos = 0;
    }
}

unsigned long get_total_memory_usage_kb() {
    unsigned long total_memory_kb = 0;
    unsigned long free_memory_kb = 0;
    unsigned long buffers_memory_kb = 0;
    unsigned long cached_memory_kb = 0;

    FILE *meminfo_file = fopen("/proc/meminfo", "r");
    if (meminfo_file) {
        char line[256];
        int values_found = 0;
        while (fgets(line, sizeof(line), meminfo_file) && values_found < 4) {
            if (sscanf(line, "MemTotal: %lu kB", &total_memory_kb) == 1) {
                values_found++;
            } else if (sscanf(line, "MemFree: %lu kB", &free_memory_kb) == 1) {
                values_found++;
            } else if (sscanf(line, "Buffers: %lu kB", &buffers_memory_kb) == 1) {
                values_found++;
            } else if (sscanf(line, "Cached: %lu kB", &cached_memory_kb) == 1) {
                values_found++;
            }
        }
        fclose(meminfo_file);
    }

    unsigned long used_memory_kb = total_memory_kb - (free_memory_kb + buffers_memory_kb + cached_memory_kb);
    return used_memory_kb;
}


int main() {
    signal(SIGINT, sigint_handler);

    while (1) {
        unsigned long total_memory_usage_kb = get_total_memory_usage_kb();
        buffer_store(total_memory_usage_kb);
        sleep(1);
    }

    return 0;
}
