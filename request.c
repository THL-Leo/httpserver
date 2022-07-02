#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/file.h>

#include "request.h"

struct stat st;
char forbidden[57] = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n";
char notFound[57] = "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n";
char badRequest[61] = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n";
char created[52] = "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n";
char ok[42] = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n";

void helper_get(
    int connfd, char *URI, char *buffer, int offset, int *status_code, int *ID, int header_size) {

    char copy[2049] = { 0 };
    copy[2048] = '\0';
    memcpy(copy, buffer + offset, header_size - offset);
    char *pointer = copy;
    char *token;
    int size = 0;

    while ((token = strtok_r(pointer, "\r\n", &pointer)) != NULL) {
        // printf("token %s\n", token);
        if (strncmp(token, "Request-Id", 10) == 0) {
            char *p2 = token;
            if ((strtok_r(p2, " ", &p2)) != NULL) {
                *ID = atoi(p2);
            }
        }
        if (strncmp(token, "Content-Length", 14) == 0) {
            char *p2 = token;
            if ((strtok_r(p2, " ", &p2)) != NULL) {
                size = atoi(p2);
            }
        }
        pointer += 1;
        // printf("pointer %s\n", pointer);
    }

    int infile = open(URI + 1, O_RDONLY | O_DIRECTORY);
    if (infile == -1) {
        infile = open(URI + 1, O_RDONLY);
    } else {
        *status_code = 403;
        write(connfd, forbidden, 57);
        close(infile);
        return;
    }
    if (infile < 0) {
        if (errno == ENOENT) {
            *status_code = 404;
            write(connfd, notFound, 57);
            close(infile);
            return;
        }
        if (errno == EACCES) {
            *status_code = 403;
            write(connfd, forbidden, 57);
        }
        close(infile);
        return;
    }
    flock(infile, LOCK_SH);
    ssize_t byte = 0;
    char buf[128] = "";
    fstat(infile, &st);
    int s = snprintf(buf, 128, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n", st.st_size);
    write(connfd, buf, s);
    *status_code = 200;
    while ((byte = read(infile, buf, 128)) > 0) {
        ssize_t byte_written = 0, cur_write = 0;
        while (byte_written < byte) {
            cur_write = write(connfd, buf + byte_written, byte - byte_written);
            if (cur_write < 0) {
                return;
            }
            byte_written += cur_write;
        }
    }
    flock(infile, LOCK_UN);
    close(infile);
}

void helper_put(int connfd, char *URI, char *buffer, int offset, int bytes_read, int *status_code,
    int *ID, int header_size) {
    // content length is how i exit the loop
    int create = 0, size = 0, msgBody = 0, header_flag = 0;
    int infile = open(URI + 1, O_WRONLY | O_TRUNC);
    if (infile == -1) {
        infile = open(URI + 1, O_WRONLY | O_TRUNC | O_CREAT, 0644);
        create = 1;
    }
    flock(infile, LOCK_EX);
    char copy[2049] = { 0 };
    copy[2048] = '\0';
    memcpy(copy, buffer + offset, header_size - offset);
    char *pointer = copy;
    char *token;

    while ((token = strtok_r(pointer, "\r\n", &pointer)) != NULL) {
        // printf("token %s\n", token);
        if (strncmp(token, "Request-Id", 10) == 0) {
            char *p2 = token;
            if ((strtok_r(p2, " ", &p2)) != NULL) {
                *ID = atoi(p2);
            }
        }
        if (strncmp(token, "Content-Length", 14) == 0) {
            char *p2 = token;
            if ((strtok_r(p2, " ", &p2)) != NULL) {
                size = atoi(p2);
                header_flag = 1;
            }
        }
        pointer += 1;
    }

    if (!header_flag) {
        printf("no header\n");
        *status_code = 400;
        write(connfd, badRequest, 61);
        close(infile);
        return;
    }
    // printf("header flag %d\n", connfd);
    int totalbyteswrite = 0;
    // printf("bytes_read %d msgBody %d size %d\n", bytes_read, msgBody, size);
    if (size < bytes_read - msgBody) {
        int count = 0;
        // printf("bytes_read %d msgBody %d size %d\n", bytes_read, msgBody, size);
        while (totalbyteswrite < size) {
            // printf("token + count %s\n", token+count);
            int temp = write(infile, token + count, size - count);
            if (temp < 0) {
                break;
            }
            count += temp;
            // bytes_read -= temp;
            // if (count == size) {
            //     break;
            // }
            totalbyteswrite += temp;
        }
        // totalbyteswrite += size;
    } else {
        // printf("bytes_read %d msgBody %d size %d\n", bytes_read, msgBody, size);
        if ((bytes_read - msgBody) != 0) {
            int count = 0;
            while (totalbyteswrite < (bytes_read - msgBody)) {
                // printf("token + count %s\n", token+count);
                int temp = write(infile, token + count, bytes_read - msgBody - count);
                if (temp < 0) {
                    break;
                }
                count += temp;
                // bytes_read -= temp;
                // if (bytes_read <= msgBody) {
                //     break;
                // }
                totalbyteswrite += temp;
            }
        }
    }
    // printf("extra in message body %d\n", connfd);
    // printf("buffer 4 %s\n", buffer);
    // int status = 0, counter = 0;
    int readBytes = 0;
    while (totalbyteswrite < size) {
        if (size - totalbyteswrite < 2048) {
            readBytes = read(connfd, buffer, size - totalbyteswrite);
        } else {
            readBytes = read(connfd, buffer, 2048);
        }
        int bytesWrote = write(infile, buffer, readBytes);
        if (bytesWrote < 0) {
            if (errno == EACCES || errno == EISDIR) {
                *status_code = 403;
                write(connfd, forbidden, 57);
                close(infile);
                return;
            }
            close(infile);
            return;
        }
        totalbyteswrite += bytesWrote;
    }
    flock(infile, LOCK_UN);
    close(infile);
    if (!create) {
        write(connfd, ok, 42);
        *status_code = 200;
    } else {
        write(connfd, created, 52);
        *status_code = 201;
    }
    return;
}

void helper_append(int connfd, char *URI, char *buffer, int offset, int bytes_read,
    int *status_code, int *ID, int header_size) {
    int size = 0, msgBody = 0, header_flag = 0;

    char copy[2049] = { 0 };
    copy[2048] = '\0';
    memcpy(copy, buffer + offset, header_size - offset);
    char *pointer = copy;
    char *token;

    while ((token = strtok_r(pointer, "\r\n", &pointer)) != NULL) {
        // printf("token %s\n", token);
        if (strncmp(token, "Request-Id", 10) == 0) {
            char *p2 = token;
            if ((strtok_r(p2, " ", &p2)) != NULL) {
                *ID = atoi(p2);
            }
        }
        if (strncmp(token, "Content-Length", 14) == 0) {
            char *p2 = token;
            if ((strtok_r(p2, " ", &p2)) != NULL) {
                size = atoi(p2);
                header_flag = 1;
            }
        }
        pointer += 1;
    }
    int infile = open(URI + 1, O_WRONLY | O_APPEND);
    if (infile == -1) {
        if (errno == ENOENT) {
            *status_code = 404;
            write(connfd, notFound, 57);
            close(infile);
            return;
        }
        if (errno == EACCES) {
            *status_code = 403;
            write(connfd, forbidden, 57);
        }
        close(infile);
        return;
    }
    flock(infile, LOCK_EX);
    if (!header_flag) {
        *status_code = 400;
        write(connfd, badRequest, 61);
        close(infile);
        return;
    }
    int totalbyteswrite = 0;
    // printf("bytes_read %d msgBody %d size %d\n", bytes_read, msgBody, size);
    if (size < bytes_read - msgBody) {
        int count = 0;
        // printf("bytes_read %d msgBody %d size %d\n", bytes_read, msgBody, size);
        while (totalbyteswrite < size) {
            // printf("token + count %s\n", token+count);
            int temp = write(infile, token + count, size - count);
            if (temp < 0) {
                break;
            }
            count += temp;
            // bytes_read -= temp;
            // if (count == size) {
            //     break;
            // }
            totalbyteswrite += temp;
        }
        // totalbyteswrite += size;
    } else {
        // printf("bytes_read %d msgBody %d size %d\n", bytes_read, msgBody, size);
        if ((bytes_read - msgBody) != 0) {
            int count = 0;
            while (totalbyteswrite < (bytes_read - msgBody)) {
                // printf("token + count %s\n", token+count);
                int temp = write(infile, token + count, bytes_read - msgBody - count);
                if (temp < 0) {
                    break;
                }
                count += temp;
                totalbyteswrite += temp;
            }
        }
    }

    int readBytes = 0;
    while (totalbyteswrite < size) {
        if (size - totalbyteswrite < 2048) {
            readBytes = read(connfd, buffer, size - totalbyteswrite);
        } else {
            readBytes = read(connfd, buffer, 2048);
        }
        int bytesWrote = write(infile, buffer, readBytes);
        if (bytesWrote < 0) {
            if (errno == EACCES || errno == EISDIR) {
                *status_code = 403;
                write(connfd, forbidden, 57);
                close(infile);
                return;
            }
            close(infile);
            return;
        }
        totalbyteswrite += bytesWrote;
    }
    *status_code = 200;
    write(connfd, ok, 42);
    flock(infile, LOCK_UN);
    close(infile);
    return;
}
