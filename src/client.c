#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include "common.h"

#define AOF_FILE "verokv.aof"
#define BATCH_FILE "verokv.batch"
#define AOF_ENABLED 0
#define BATCH_FLUSH_ENABLED 1
#define FLUSH_INTERVAL 10
#define BATCH_SIZE 100 // Set a batch size limit for flushing

// Replay options: Set to either "AOF" or "BATCH"
#define REPLAY_OPTION "BATCH"

static void parse_resp(char *resp);
void log_to_aof(FILE *aof, const char *cmd);
void replay_aof(int sfd, FILE *aof);
void replay_batch(int sfd, FILE *batch);
void *batch_flush_thread(void *arg);

typedef struct QueueNode {
    char *cmd;
    struct QueueNode *next;
} QueueNode;

typedef struct {
    QueueNode *front, *rear;
    pthread_mutex_t lock;
    int size; // Track the size of the queue
} Queue;

Queue *initQueue() {
    Queue *q = malloc(sizeof(Queue));
    q->front = q->rear = NULL;
    q->size = 0; // Initialize size
    pthread_mutex_init(&q->lock, NULL);
    return q;
}

void enqueue(Queue *q, const char *cmd) {
    QueueNode *newNode = malloc(sizeof(QueueNode));
    newNode->cmd = strdup(cmd);
    newNode->next = NULL;

    pthread_mutex_lock(&q->lock);
    if (q->rear) {
        q->rear->next = newNode;
    } else {
        q->front = newNode;
    }
    q->rear = newNode;
    q->size++; // Increment size
    pthread_mutex_unlock(&q->lock);
}

void flushQueueToFile(Queue *q, FILE *batchFile) {
    pthread_mutex_lock(&q->lock);
    if (q->size == 0) {
        pthread_mutex_unlock(&q->lock);
        return; // Nothing to flush
    }

    QueueNode *temp;
    while (q->front) {
        fputs(q->front->cmd, batchFile);
        fputc('\n', batchFile);
        temp = q->front;
        q->front = q->front->next;
        free(temp->cmd);
        free(temp);
        q->size--; // Decrement size
    }
    q->rear = NULL;
    pthread_mutex_unlock(&q->lock);
    fflush(batchFile);
}

void *batch_flush_thread(void *arg) {
    Queue *q = (Queue *)arg;
    FILE *batchFile = fopen(BATCH_FILE, "a+");
    if (!batchFile) {
        perror("Failed to open batch file");
        pthread_exit(NULL);
    }

    struct timespec req;
    req.tv_sec = FLUSH_INTERVAL;
    req.tv_nsec = 0;

    while (1) {
        nanosleep(&req, NULL);
        flushQueueToFile(q, batchFile);
    }

    fclose(batchFile);
    pthread_exit(NULL);
}

int connect_server(char *addr, int port) {
    struct sockaddr_in serv_addr;
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(addr);
    serv_addr.sin_port = htons(port);

    if (connect(sfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
        perror("Connection to server failed");
        close(sfd);
        exit(EXIT_FAILURE);
    }

    return sfd;
}

static void print_quote_encase(const char *str) {
    printf("\"%s\"\n", str);
}

static int parse_int(char *resp) {
    int n = 0, min = 0;
    resp++;
    if (*resp == '-') {
        min = 1;
        resp++;
    }
    while (*resp && *resp != '\r') {
        n = n * 10 + (*resp - '0');
        resp++;
    }
    return min ? -n : n;
}

static char *parse_str(char *resp) {
    int len = parse_int(resp);
    if (len < 0) return NULL;

    while (*resp && *resp != '\n') resp++;
    resp++;

    char *result = malloc(len + 1);
    if (result) {
        strncpy(result, resp, len);
        result[len] = '\0';
    }
    return result;
}

static char *parse_err(char *resp) {
    resp++;
    char *result = strndup(resp, strlen(resp) - 2);
    return result;
}

static void parse_resp(char *resp) {
    char *tmp;
    switch (*resp) {
        case '$':
            tmp = parse_str(resp);
            if (!tmp) puts("(nil)");
            else {
                print_quote_encase(tmp);
                free(tmp);
            }
            break;
        case ':':
            printf("(integer) %d\n", parse_int(resp));
            break;
        case '-':
            tmp = parse_err(resp);
            puts(tmp);
            free(tmp);
            break;
        default:
            break;
    }
}

void log_to_aof(FILE *aof, const char *cmd) {
    struct timespec current_time;
    clock_gettime(CLOCK_REALTIME, &current_time);

    // Write command to AOF file
    if (fputs(cmd, aof) == EOF || fputc('\n', aof) == EOF) {
        perror("Error writing to AOF file");
        return;
    }
    fflush(aof);

    // Log the time after every query insertion
    fprintf(aof, "# Timestamp: %ld.%09ld\n", current_time.tv_sec, current_time.tv_nsec);
    fflush(aof);
}


long elapsed_time_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;
}

void replay_batch(int sfd, FILE *batch) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    fseek(batch, 0, SEEK_SET);
    char line[1024];
    while (fgets(line, sizeof(line), batch)) {
        writeline(sfd, line);
        char *resp = readline(sfd);
        if (resp) {
            parse_resp(resp);
            free(resp);
        }
    }
    puts("Batch file replay completed.");

    clock_gettime(CLOCK_MONOTONIC, &end);
    printf("Time taken for Batch replay: %ld milliseconds\n", elapsed_time_ms(start, end));
}

void replay_aof(int sfd, FILE *aof) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    fseek(aof, 0, SEEK_SET);
    char line[1024];
    while (fgets(line, sizeof(line), aof)) {
        writeline(sfd, line);
        char *resp = readline(sfd);
        if (resp) {
            parse_resp(resp);
            free(resp);
        }
    }
    puts("AOF replay completed.");

    clock_gettime(CLOCK_MONOTONIC, &end);
    printf("Time taken for AOF replay: %ld milliseconds\n", elapsed_time_ms(start, end));
}

void repl(int sfd) {
#if AOF_ENABLED
    FILE *aof = fopen(AOF_FILE, "a+");
    if (!aof) {
        perror("Failed to open AOF file");
        exit(EXIT_FAILURE);
    }
    if (strcmp(REPLAY_OPTION, "AOF") == 0) {
        replay_aof(sfd, aof);
    }
#endif

#if BATCH_FLUSH_ENABLED
    Queue *batchQueue = initQueue();
    pthread_t batchThread;
    pthread_create(&batchThread, NULL, batch_flush_thread, (void *)batchQueue);

    if (strcmp(REPLAY_OPTION, "BATCH") == 0) {
        FILE *batch = fopen(BATCH_FILE, "a+");
        if (!batch) {
            perror("Failed to open batch file for replay");
            exit(EXIT_FAILURE);
        }
        replay_batch(sfd, batch);
        fclose(batch);
    }
#endif

    char inp[1024];
    while (1) {
        printf("verokv> ");
        if (!fgets(inp, sizeof(inp), stdin)) break;

#if AOF_ENABLED
        if (strncmp(inp, "set", 3) == 0 || 
            strncmp(inp, "del", 3) == 0) {
            log_to_aof(aof, inp);
        }
#endif

#if BATCH_FLUSH_ENABLED
        if (strncmp(inp, "set", 3) == 0 || 
            strncmp(inp, "del", 3) == 0) {
            enqueue(batchQueue, inp);
            if (batchQueue->size >= BATCH_SIZE) { // Flush if the batch size limit is reached
                FILE *batchFile = fopen(BATCH_FILE, "a+");
                flushQueueToFile(batchQueue, batchFile);
                fclose(batchFile);
            }
        }
#endif

        writeline(sfd, inp);
        char *resp = readline(sfd);
        if (!resp) break;
        parse_resp(resp);
        free(resp);
    }

#if AOF_ENABLED
    fclose(aof);
#endif

#if BATCH_FLUSH_ENABLED
    pthread_cancel(batchThread);
    flushQueueToFile(batchQueue, fopen(BATCH_FILE, "a+")); // Flush remaining commands
    free(batchQueue);
#endif
}
