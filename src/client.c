#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"

#define AOF_FILE "verokv.aof"
#define AOF_ENABLED 0

static void parse_resp(char *resp);
void log_to_aof(FILE *aof, char *cmd);
void replay_aof(int sfd, FILE *aof);

int connect_server(char *addr, int port) {
    struct sockaddr_in serv_addr;
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
        fputs("failed to create socket", stderr);
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(addr);
    serv_addr.sin_port = htons(port);

    if (connect(sfd, (SA *)&serv_addr, sizeof(serv_addr)) != 0) {
        fputs("failed to connect to server", stderr);
        exit(1);
    }

    return sfd;
}

static void print_quote_encase(char *str) {
    char *tmp = malloc(strlen(str) + 3);
    sprintf(tmp, "\"%s\"", str);
    puts(tmp);
    free(tmp);
}

static int parse_int(char *resp) {
    resp++;
    int n = 0, min = 0;
    if (*resp == '-') resp++, min++;
    while (*resp != '\r' && *resp != '\0') {
        n = n * 10 + (*resp - '0');
        resp++;
    }
    return min ? n * -1 : n;
}

static char *parse_str(char *resp) {
    int n = parse_int(resp);
    if (n < 0) return NULL;

    while (*resp != '\n') resp++;
    resp++;

    char *result = calloc(n + 1, sizeof(char));
    if (result) {
        strncpy(result, resp, n);
        result[n] = '\0';
    }
    return result;
}

static char *parse_err(char *resp) {
    resp++;
    char *result = malloc(strlen(resp) - 1);
    if (result) {
        strncpy(result, resp, strlen(resp) - 2);
        result[strlen(resp) - 2] = '\0';  // Null-terminate the string
    }
    return result;
}

static void parse_resp(char *resp) {
    char *tmp;
    switch (*resp) {
        case '$':
            tmp = parse_str(resp);
            if (tmp == NULL) {
                puts("(nil)");
            } else {
                print_quote_encase(tmp);
                free(tmp);
            }
            break;
        case ':':
            printf("(integer) %d\n", parse_int(resp));
            break;
        case '*': 
            // Handle multi-bulk response if needed
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

void log_to_aof(FILE *aof, char *cmd) {
    if (fputs(cmd, aof) == EOF) {
        perror("Failed to write command to AOF file");
        return;
    }
    fputc('\n', aof); 
    if (fflush(aof) != 0) {
        perror("Failed to flush AOF file");
    }
}

void replay_aof(int sfd, FILE *aof) {
    fseek(aof, 0, SEEK_SET);
    char line[1024];
    while (fgets(line, sizeof(line), aof) != NULL) {
        writeline(sfd, line);
        char *resp = readline(sfd);
        parse_resp(resp);
        free(resp);
    }
    printf("Data Regeneration Successful with AOF \n");
}

void repl(int sfd) {
#if AOF_ENABLED
    FILE *aof = fopen(AOF_FILE, "a+");
    if (!aof) {
        perror("Failed to open AOF file");
        exit(1);
    }

    replay_aof(sfd, aof);
#endif

    while (1) {
        char *inp = malloc(1024);
        if (inp == NULL) {
            perror("Failed to allocate memory");
            break;
        }
        
        printf("verokv> ");
        fgets(inp, 1024, stdin);

#if AOF_ENABLED
        if (strncmp(inp, "set", 3) == 0 || 
            strncmp(inp, "del", 3) == 0 || 
            strncmp(inp, "hset", 4) == 0 || 
            strncmp(inp, "hdel", 4) == 0 || 
            strncmp(inp, "lpush", 5) == 0 || 
            strncmp(inp, "rpush", 5) == 0 || 
            strncmp(inp, "lpop", 4) == 0 || 
            strncmp(inp, "rpop", 4) == 0 || 
            strncmp(inp, "sadd", 4) == 0 || 
            strncmp(inp, "srem", 4) == 0 || 
            strncmp(inp, "hmset", 5) == 0 || 
            strncmp(inp, "hmget", 5) == 0 || 
            strncmp(inp, "incr", 4) == 0 || 
            strncmp(inp, "decr", 4) == 0 || 
            strncmp(inp, "incrby", 6) == 0 || 
            strncmp(inp, "decrby", 6) == 0 || 
            strncmp(inp, "append", 6) == 0 || 
            strncmp(inp, "setnx", 6) == 0 || 
            strncmp(inp, "mset", 4) == 0 || 
            strncmp(inp, "msetnx", 6) == 0 || 
            strncmp(inp, "hsetnx", 6) == 0 || 
            strncmp(inp, "hincrby", 8) == 0 || 
            strncmp(inp, "lrem", 4) == 0 || 
            strncmp(inp, "lset", 4) == 0 || 
            strncmp(inp, "ltrim", 6) == 0 || 
            strncmp(inp, "rename", 6) == 0) {
            log_to_aof(aof, inp);
        }
#endif

        writeline(sfd, inp);
        char *resp = readline(sfd);
        if (resp == NULL) break;
        parse_resp(resp);
        free(inp);
    }

#if AOF_ENABLED
    fclose(aof);
#endif
}
