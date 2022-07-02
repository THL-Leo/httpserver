#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "request.h"
#include "queue.h"

#define OPTIONS              "t:l:"
#define DEFAULT_THREAD_COUNT 4

static FILE *logfile;
#define LOG(...) fprintf(logfile, __VA_ARGS__);
pthread_mutex_t mutex;
pthread_mutex_t writer;
pthread_cond_t cond;
Queue *queue;
pthread_t *pool;
int threads = DEFAULT_THREAD_COUNT;
bool exit_flag = false;

// Converts a string to an 16 bits unsigned integer.
// Returns 0 if the string is malformed or out of the range.
static size_t strtouint16(char number[]) {
    char *last;
    long num = strtol(number, &last, 10);
    if (num <= 0 || num > UINT16_MAX || *last != '\0') {
        return 0;
    }
    return num;
}

// Creates a socket for listening for connections.
// Closes the program and prints an error message on error.
static int create_listen_socket(uint16_t port) {
    struct sockaddr_in addr;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        err(EXIT_FAILURE, "socket error");
    }
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *) &addr, sizeof addr) < 0) {
        err(EXIT_FAILURE, "bind error");
    }
    if (listen(listenfd, 128) < 0) {
        err(EXIT_FAILURE, "listen error");
    }
    return listenfd;
}

int read_bytes(int infile, char *buf, int nbytes) {
    int counter = 0;
    while (nbytes > 0) {
        if (strstr(buf, "\r\n\r\n") != NULL) {
            break;
        }
        int temp = read(infile, buf + counter, 1);
        if (temp <= 0) {
            break;
        }
        counter += temp;
        nbytes -= temp;
    }
    return counter;
}

#define BUF_SIZE 2048
void handle_connection(int connfd) {
    // printf("connfd is %d\n", connfd);
    // make the compiler not complain
    char buffer[BUF_SIZE + 1] = { 0 };
    buffer[2048] = '\0';
    char method[9] = { 0 };
    method[8] = '\0';
    char URI[20] = { 0 };
    URI[19] = '\0';
    char ver[11] = { 0 };
    ver[10] = '\0';
    int offset = 0, ID = 0, stcode = 0;
    char *pointer;
    char copy[2049] = { 0 };
    copy[2048] = '\0';
    char *exist = buffer;
    int status = 0;
    int buffer_size = 2048;
    // status = read_bytes(connfd, buffer, buffer_size);
    status = read_bytes(connfd, buffer, buffer_size);
    // printf("%s\n", buffer);
    memcpy(copy, buffer, status);
    char *token = copy;
    exist = strstr(buffer, "\r\n\r\n");
    if (exist == NULL) {
        write(connfd, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n",
            strlen("HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n"));
        // printf("here\n");
        return;
    } else {
        pointer = strtok_r(token, " ", &token);
        strncpy(method, pointer, 9);
        pointer = strtok_r(token, " ", &token);
        strncpy(URI, pointer, 20);
        pointer = strtok_r(token, "\r\n", &token);
        strncpy(ver, pointer, 11);
        offset += strnlen(method, 9);
        offset += strnlen(URI, 20);
        offset += strnlen(ver, 11);
        offset += 4;
        // printf("pthread is %lu %s %s\n", pthread_self(), method, URI);
        if (strcmp(ver, "HTTP/1.1") != 0) {
            write(connfd, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n",
                strlen("HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n"));
            return;
        }
        if (strcmp(method, "GET") == 0) {
            helper_get(connfd, URI, buffer, offset, &stcode, &ID, status);
            pthread_mutex_lock(&writer);
            LOG("%s,%s,%d,%d\n", method, URI, stcode, ID);
            fflush(logfile);
            pthread_mutex_unlock(&writer);
        } else if (strcmp(method, "PUT") == 0) {
            // printf("%s %s %s\n", method, URI, ver);
            helper_put(connfd, URI, buffer, offset, status, &stcode, &ID, status);
            pthread_mutex_lock(&writer);
            LOG("%s,%s,%d,%d\n", method, URI, stcode, ID);
            fflush(logfile);
            pthread_mutex_unlock(&writer);
        } else if (strcmp(method, "APPEND") == 0) {
            helper_append(connfd, URI, buffer, offset, status, &stcode, &ID, status);
            pthread_mutex_lock(&writer);
            LOG("%s,%s,%d,%d\n", method, URI, stcode, ID);
            fflush(logfile);
            pthread_mutex_unlock(&writer);
        }
    }
    // close(connfd);
}

static void usage(char *exec) {
    fprintf(stderr, "usage: %s [-t threads] [-l logfile] <port>\n", exec);
}

void *thread_pool(void *arg) {
    (void) arg;
    while (1) {
        pthread_mutex_lock(&mutex);
        while (queue_size(queue) <= 0) {
            pthread_cond_wait(&cond, &mutex);
        }
        uint32_t connfd = 0;
        dequeue(queue, &connfd);
        pthread_mutex_unlock(&mutex);
        handle_connection(connfd);
        close(connfd);
        if (exit_flag) {
            break;
        }
    }
    return NULL;
}

void join_threads(int threads) {
    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    for (int i = 0; i < threads; i++) {
        if (pthread_cancel(pool[i]) != 0) {
            fprintf(stderr, "Unable to cancel Thread %d\n", i);
            if (pthread_join(pool[i], NULL) != 0) {
                fprintf(stderr, "Unable to join Thread %d\n", i);
            }
            exit(EXIT_FAILURE);
        }
    }
}

static void sigterm_handler(int sig) {
    if (sig == SIGTERM) {
        join_threads(threads);
        warnx("received SIGTERM");
        fclose(logfile);
        exit(EXIT_SUCCESS);
    }
    if (sig == SIGINT) {
        join_threads(threads);
        warnx("received SIGINT");
        fclose(logfile);
        exit(EXIT_SUCCESS);
    }
}

int main(int argc, char *argv[]) {
    int opt = 0;

    logfile = stderr;

    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
        case 't':
            threads = strtol(optarg, NULL, 10);
            if (threads <= 0) {
                errx(EXIT_FAILURE, "bad number of threads");
            }
            break;
        case 'l':
            logfile = fopen(optarg, "w");
            if (!logfile) {
                errx(EXIT_FAILURE, "bad logfile");
            }
            break;
        default: usage(argv[0]); return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        warnx("wrong number of arguments");
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    uint16_t port = strtouint16(argv[optind]);
    if (port == 0) {
        errx(EXIT_FAILURE, "bad port number: %s", argv[1]);
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    int listenfd = create_listen_socket(port);
    // LOG("port=%" PRIu16 ", threads=%d\n", port, threads);
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    queue = queue_create();

    pool = calloc(threads, sizeof(pthread_t));
    for (int i = 0; i < threads; i++) {
        if (pthread_create(&pool[i], NULL, &thread_pool, NULL) != 0) {
            fprintf(stderr, "Thread %d cannot be made\n", i);
            exit(EXIT_FAILURE);
        }
    }

    for (;;) {
        uint32_t connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            warn("accept error");
            continue;
        }
        pthread_mutex_lock(&mutex);
        enqueue(queue, connfd);
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }
    queue_delete(&queue);
    return EXIT_SUCCESS;
}
