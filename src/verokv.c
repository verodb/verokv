#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include "common.h"

#define SNAPSHOT_FILE "snapshot.dat"
#define SNAPSHOT_INTERVAL 3
#define ENABLE_SNAPSHOTS 0

static HashTable *global_ht; 
static int server_running = 1;

// Function to calculate elapsed time in microseconds
static long calculate_elapsed_time(struct timeval start, struct timeval end) {
    long seconds = end.tv_sec - start.tv_sec;
    long microseconds = end.tv_usec - start.tv_usec;
    return (seconds * 1000000) + microseconds; // Return in microseconds
}

static int save_snapshot(HashTable *ht, const char *filename) {
#if ENABLE_SNAPSHOTS
    struct timeval start, end;
    gettimeofday(&start, NULL);

    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file for snapshot");
        return -1;
    }

    for (int i = 0; i < ht->size; i++) {
        HashTableItem *item = ht->items[i];
        if (item && item->key && item->value) {
            size_t key_len = strlen(item->key);
            size_t value_len = strlen((char *)item->value);

            fwrite(&key_len, sizeof(size_t), 1, file);     
            fwrite(item->key, sizeof(char), key_len, file);
            fwrite(&value_len, sizeof(size_t), 1, file);         
            fwrite(item->value, sizeof(char), value_len, file);  
        }
    }

    fclose(file);
    gettimeofday(&end, NULL);
    long microseconds = calculate_elapsed_time(start, end);
    printf("Snapshot saved in %ld microseconds.\n", microseconds);
#endif
    return 0;
}

// Function to load the hash table state from a snapshot file
static int load_snapshot(HashTable *ht, const char *filename) {
#if ENABLE_SNAPSHOTS
    struct timeval start, end;
    gettimeofday(&start, NULL);

    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Snapshot file not found or cannot be opened");
        return -1;
    }

    htable_free(ht);
    ht = htable_init(HT_BASE_SIZE);
    while (!feof(file)) {
        size_t key_len, value_len;

        if (fread(&key_len, sizeof(size_t), 1, file) != 1) break;
        char *key = (char *)malloc(key_len + 1);
        fread(key, sizeof(char), key_len, file);
        key[key_len] = '\0';

        fread(&value_len, sizeof(size_t), 1, file);
        char *value = (char *)malloc(value_len + 1);
        fread(value, sizeof(char), value_len, file);
        value[value_len] = '\0';

        htable_set(ht, key, value);

        free(key);
        free(value);
    }

    fclose(file);
    gettimeofday(&end, NULL);
    long microseconds = calculate_elapsed_time(start, end);
    printf("Snapshot loaded in %ld microseconds.\n", microseconds);
#endif
    return 0;
}

// Snapshot thread function
void *snapshot_thread(void *arg) {
#if ENABLE_SNAPSHOTS
    while (server_running) {
        sleep(SNAPSHOT_INTERVAL);
        if (server_running) {
            save_snapshot(global_ht, SNAPSHOT_FILE);
        }
    }
#endif
    return NULL;
}

// Function to close the server, saving the snapshot first
static void close_server(int sfd, HashTable *ht) {
#if ENABLE_SNAPSHOTS
    server_running = 0; 
    pthread_join(*(pthread_t *)ht, NULL); 
    save_snapshot(ht, SNAPSHOT_FILE);
#endif
    close_socket(sfd);
    htable_free(ht);
    exit(0);
}

// Main function to initialize the server, load snapshot, and start accepting connections
int main() {
    HashTable *ht = htable_init(HT_BASE_SIZE);
    global_ht = ht;  // Set global reference for the snapshot thread

    // Load snapshot if available
#if ENABLE_SNAPSHOTS
    if (load_snapshot(ht, SNAPSHOT_FILE) == 0) {
        printf("Snapshot loaded successfully.\n");
    }
#endif

    // Start snapshot thread
#if ENABLE_SNAPSHOTS
    pthread_t snapshot_tid;
    pthread_create(&snapshot_tid, NULL, snapshot_thread, NULL);
#endif

    int sfd = init_server();
    while (1) {
        int cfd = accept_connection(sfd);
        struct timeval start, end;
        gettimeofday(&start, NULL);
        
        int code = verokv(cfd, ht);
        
        gettimeofday(&end, NULL);
        long microseconds = calculate_elapsed_time(start, end);
        printf("Connection handled in %ld microseconds.\n", microseconds);

        code == 1 ? close_server(sfd, ht) : close_socket(cfd);
    }

    return 0;
}
