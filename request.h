#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

void helper_get(
    int connfd, char *URI, char *buffer, int offset, int *status_code, int *ID, int header_size);

void helper_put(int connfd, char *URI, char *buffer, int offset, int bytes_read, int *status_code,
    int *ID, int header_size);

void helper_append(int connfd, char *URI, char *buffer, int offset, int bytes_read,
    int *status_code, int *ID, int header_size);
