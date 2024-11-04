#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>

#include "common.h"

#define AOF_FILE "verokv.aof"
#define BATCH_FILE "verokv.batch"
#define BATCH_SIZE 100 // Set a batch size limit for flushing
#define FLUSH_INTERVAL 10 // Time in seconds for flushing batch file

// Toggle variables for AOF and Batch processing
int ENABLE_AOF = 1;  // 1 to enable AOF, 0 to disable
int ENABLE_BATCH = 1; // 1 to enable Batch processing, 0 to disable

typedef struct QueueNode {
    char *cmd;
    struct QueueNode *next;
} QueueNode;

typedef struct {
    QueueNode *front, *rear;
    pthread_mutex_t lock;
    int size;
} Queue;

Queue *initQueue() {
    Queue *q = malloc(sizeof(Queue));
    q->front = q->rear = NULL;
    q->size = 0;
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
    q->size++;
    pthread_mutex_unlock(&q->lock);
}

void flushQueueToFile(Queue *q, FILE *batchFile) {
    pthread_mutex_lock(&q->lock);
    if (q->size == 0) {
        pthread_mutex_unlock(&q->lock);
        return;
    }

    QueueNode *temp;
    while (q->front) {
        fputs(q->front->cmd, batchFile);
        fputc('\n', batchFile);
        temp = q->front;
        q->front = q->front->next;
        free(temp->cmd);
        free(temp);
        q->size--;
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

void log_to_aof(FILE *aof, const char *cmd) {
    if (ENABLE_AOF) {
        if (fputs(cmd, aof) == EOF || fputc('\n', aof) == EOF) {
            perror("Error writing to AOF file");
        }
        fflush(aof);
    }
}

char *readline(int cfd) {
    char *msg = dmalloc(1024 * sizeof(char));
    read(cfd, msg, 1024);
    msg[strlen(msg) - 1] = '\0';
    return msg;
}

void writeline(int cfd, char *msg) {
    char *tmp = dmalloc((strlen(msg) + 2) * sizeof(char));
    sprintf(tmp, "%s\n", msg);
    write(cfd, tmp, strlen(tmp) + 1);
    free(tmp);
}

void replay_commands(FILE *file, HashTable *ht) {
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0; // Remove the newline character
        Command *cmd = parse(line);
        char *resp = interpret(ht, cmd);
        // You may want to handle the response here
    }
}

int verokv(int cfd, HashTable *ht) {
    FILE *aof = NULL;
    if (ENABLE_AOF) {
        aof = fopen(AOF_FILE, "a+");
        if (!aof) {
            perror("Failed to open AOF file");
            exit(EXIT_FAILURE);
        }

        // Replay AOF commands at the start
        rewind(aof);
        replay_commands(aof, ht);
    }

    Queue *batchQueue = NULL;
    pthread_t batchThread;
    if (ENABLE_BATCH) {
        batchQueue = initQueue();
        pthread_create(&batchThread, NULL, batch_flush_thread, (void *)batchQueue);
    }

    while (1) {
        char *msg = readline(cfd);
        Command *cmd = parse(msg);
        char *resp = interpret(ht, cmd);
        
        if (resp) {
            writeline(cfd, resp);
            if (strncmp(msg, "set", 3) == 0 || strncmp(msg, "del", 3) == 0) {
                if (ENABLE_AOF) {
                    log_to_aof(aof, msg);  // Log command to AOF if enabled
                }
                if (ENABLE_BATCH) {
                    enqueue(batchQueue, msg); // Enqueue command for batch writing

                    if (batchQueue->size >= BATCH_SIZE) {
                        FILE *batchFile = fopen(BATCH_FILE, "a+");
                        flushQueueToFile(batchQueue, batchFile);
                        fclose(batchFile);
                    }
                }
            }
        }
        
        free(msg);
        if (*resp == 'q') break; // Exit command
    }

    if (ENABLE_AOF) fclose(aof);
    if (ENABLE_BATCH) {
        pthread_cancel(batchThread);
        flushQueueToFile(batchQueue, fopen(BATCH_FILE, "a+")); // Flush remaining commands
        free(batchQueue);
    }
    return 0;
}

int init_server() {
    struct sockaddr_in serv_addr;
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
        fputs("failed to create socket", stderr);
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT_NUM);

    if (bind(sfd, (SA *)&serv_addr, sizeof(serv_addr)) != 0) {
        fputs("failed to bind socket", stderr);
        exit(1);
    }

    if (listen(sfd, 5) != 0) {
        fputs("listen failed", stderr);
        exit(1);
    }

    puts("server started");
    return sfd;
}

int accept_connection(int sfd) {
    struct sockaddr_in client_addr;
    unsigned int len = sizeof(struct sockaddr_in);
    int cfd = accept(sfd, (SA *)&client_addr, &len);
    if (cfd < 0) {
        fputs("failed to accept connection", stderr);
        exit(1);
    }
    return cfd;
}

void close_socket(int sockfd) {
    close(sockfd);
}

void close_client(int cfd) {
    shutdown(cfd, SHUT_RDWR);
    close(cfd);
}
