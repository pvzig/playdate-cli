#ifndef SOCKET_TEST_SUPPORT_H
#define SOCKET_TEST_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct pdsim_test_server pdsim_test_server;

bool pdsim_test_create_stale_socket(
    const char *socket_path,
    char *error_message,
    size_t error_capacity
);

pdsim_test_server *pdsim_test_server_start(
    const char *socket_path,
    const char *response,
    uint32_t response_delay_milliseconds,
    char *error_message,
    size_t error_capacity
);

bool pdsim_test_server_finish(
    pdsim_test_server *server,
    char *request,
    size_t request_capacity,
    char *error_message,
    size_t error_capacity
);

#endif
