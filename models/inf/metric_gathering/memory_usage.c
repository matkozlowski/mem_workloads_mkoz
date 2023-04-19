#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#define BUFFER_SIZE 4096
#define PAGE_SIZE 4096

#define TOTAL_MEM_FILENAME "total_mem_usage.txt"
#define TF_MEM_FILENAME "tf_mem_usage.txt"

typedef struct {
    time_t timestamp;
    unsigned long memory_usage_kb;
} MemoryUsageEntry;

MemoryUsageEntry total_buffer[BUFFER_SIZE];
MemoryUsageEntry tf_buffer[BUFFER_SIZE];
size_t buffer_pos = 0;

void write_buffer_to_file(char *filename, MemoryUsageEntry buffer[]) {
    FILE *file = fopen(filename, "w");
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
    write_buffer_to_file(TOTAL_MEM_FILENAME, total_buffer);
    write_buffer_to_file(TF_MEM_FILENAME, tf_buffer);
    exit(0);
}

void buffer_store(unsigned long memory_usage_kb, MemoryUsageEntry buffer[], char *filename) {
    buffer[buffer_pos].timestamp = time(NULL);
    buffer[buffer_pos].memory_usage_kb = memory_usage_kb;
    buffer_pos++;
    if (buffer_pos >= BUFFER_SIZE) {
        write_buffer_to_file(filename, buffer);
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

long get_memory_usage_of_tensorflow_processes_kb() {
    DIR *proc_dir;
    struct dirent *entry;
    char comm_path[256], statm_path[256];
    char buffer[256];
    long total_memory_usage = 0;

    // Open the /proc directory
    if ((proc_dir = opendir("/proc")) == NULL) {
        perror("Error opening /proc directory");
        return -1;
    }

    // Iterate through the /proc directory
    while ((entry = readdir(proc_dir)) != NULL) {
        // Check if the entry is a directory and its name is a number (PID)
        if (entry->d_type == DT_DIR && atoi(entry->d_name) > 0) {
            // Build the /proc/PID/comm file path
            snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", entry->d_name);

            FILE *comm_file;

            // Open the /proc/PID/comm file, read the process name, and check if it contains "tensorflow"
            if ((comm_file = fopen(comm_path, "r")) != NULL &&
                fgets(buffer, sizeof(buffer), comm_file) != NULL &&
                strstr(buffer, "tensorflow") != NULL) {
                fclose(comm_file);

                // Build the /proc/PID/statm file path
                snprintf(statm_path, sizeof(statm_path), "/proc/%s/statm", entry->d_name);

                // Open the /proc/PID/statm file, read the resident memory pages, and add to the total memory usage
                FILE *statm_file;
                unsigned long resident_pages;
                if ((statm_file = fopen(statm_path, "r")) != NULL &&
                    fscanf(statm_file, "%*s %lu", &resident_pages) == 1) {
                    total_memory_usage += resident_pages * PAGE_SIZE;
                    fclose(statm_file);
                }
            } else if (comm_file != NULL) {
                fclose(comm_file);
            }
        }
    }

    // Close the /proc directory
    closedir(proc_dir);

    // Return the total memory usage
    return total_memory_usage;
}


int main() {
    signal(SIGINT, sigint_handler);

    while (1) {
        unsigned long total_memory_usage_kb = get_total_memory_usage_kb();
        buffer_store(total_memory_usage_kb, total_buffer, TOTAL_MEM_FILENAME);

        unsigned long tf_memory_usage_kb = get_memory_usage_of_tensorflow_processes_kb();
        buffer_store(tf_memory_usage_kb, tf_buffer, TF_MEM_FILENAME);

        sleep(1);
    }

    return 0;
}
