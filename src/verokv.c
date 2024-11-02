#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "common.h"

#define SNAPSHOT_FILE "snapshot.dat"
#define SNAPSHOT_INTERVAL 10 

static HashTable *global_ht; 
static int server_running = 1;

static int save_snapshot(HashTable *ht, const char *filename) {
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
    return 0;
}

// Function to load the hash table state from a snapshot file
static int load_snapshot(HashTable *ht, const char *filename) {
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
    return 0;
}

// Snapshot thread function
void *snapshot_thread(void *arg) {
    while (server_running) {
        sleep(SNAPSHOT_INTERVAL);
        if (server_running) {
            save_snapshot(global_ht, SNAPSHOT_FILE);
        }
    }
    return NULL;
}

// Function to close the server, saving the snapshot first
static void close_server(int sfd, HashTable *ht) {
    server_running = 0; 
    pthread_join(*(pthread_t *)ht, NULL); 
    save_snapshot(ht, SNAPSHOT_FILE);
    close_socket(sfd);
    htable_free(ht);
    exit(0);
}

// Main function to initialize the server, load snapshot, and start accepting connections
int main() {
    HashTable *ht = htable_init(HT_BASE_SIZE);
    global_ht = ht;  // Set global reference for the snapshot thread

    // Load snapshot if available
    if (load_snapshot(ht, SNAPSHOT_FILE) == 0) {
        printf("Snapshot loaded successfully.\n");
    }

    // Start snapshot thread
    pthread_t snapshot_tid;
    pthread_create(&snapshot_tid, NULL, snapshot_thread, NULL);

    int sfd = init_server();
    while (1) {
        int cfd = accept_connection(sfd);
        int code = verokv(cfd, ht);
        code == 1 ? close_server(sfd, ht) : close_socket(cfd);
    }

    return 0;
}
