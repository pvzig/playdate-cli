#include "SocketTestSupport.h"
#include "DescriptorIO.h"
#include "PlaydateSimulatorProtocol.h"

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

struct pdsim_test_server {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    bool is_ready;
    bool setup_succeeded;
    uint32_t response_delay_milliseconds;
    char socket_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    char response[PDSIM_PROTOCOL_BUFFER_CAPACITY];
    char request[PDSIM_PROTOCOL_BUFFER_CAPACITY];
    char error_message[PDSIM_PROTOCOL_BUFFER_CAPACITY];
};

static void pdsim_test_set_message(
    char *destination,
    size_t capacity,
    const char *message
);

static void pdsim_test_set_errno_message(
    char *destination,
    size_t capacity,
    const char *operation
);

bool pdsim_test_create_stale_socket(
    const char *socket_path,
    char *error_message,
    size_t error_capacity
) {
    if (socket_path == NULL) {
        pdsim_test_set_message(error_message, error_capacity, "invalid stale socket path");
        return false;
    }

    int descriptor = socket(AF_UNIX, SOCK_STREAM, 0);
    if (descriptor < 0) {
        pdsim_test_set_errno_message(error_message, error_capacity, "socket");
        return false;
    }

    struct sockaddr_un address = {0};
    address.sun_family = AF_UNIX;
    size_t path_length = strlcpy(address.sun_path, socket_path, sizeof(address.sun_path));
    if (path_length >= sizeof(address.sun_path)) {
        close(descriptor);
        pdsim_test_set_message(error_message, error_capacity, "stale socket path is too long");
        return false;
    }
    address.sun_len = (uint8_t)(offsetof(struct sockaddr_un, sun_path) + path_length);
    (void)unlink(socket_path);
    if (bind(descriptor, (struct sockaddr *)&address, address.sun_len) != 0) {
        pdsim_test_set_errno_message(error_message, error_capacity, "bind stale socket");
        close(descriptor);
        return false;
    }
    close(descriptor);
    pdsim_test_set_message(error_message, error_capacity, "");
    return true;
}

static void pdsim_test_set_message(
    char *destination,
    size_t capacity,
    const char *message
) {
    if (destination != NULL && capacity > 0) {
        snprintf(destination, capacity, "%s", message);
    }
}

static void pdsim_test_set_errno_message(
    char *destination,
    size_t capacity,
    const char *operation
) {
    if (destination != NULL && capacity > 0) {
        snprintf(destination, capacity, "%s: %s", operation, strerror(errno));
    }
}

static void pdsim_test_publish_setup(
    pdsim_test_server *server,
    bool succeeded,
    const char *error_message
) {
    pthread_mutex_lock(&server->mutex);
    server->setup_succeeded = succeeded;
    server->is_ready = true;
    pdsim_test_set_message(
        server->error_message,
        sizeof(server->error_message),
        error_message
    );
    pthread_cond_signal(&server->condition);
    pthread_mutex_unlock(&server->mutex);
}

static void pdsim_test_read_request(pdsim_test_server *server, int descriptor) {
    size_t received = 0;
    pdsim_line_read_result result = pdsim_read_line(
        descriptor,
        server->request,
        sizeof(server->request),
        &received
    );
    if (result == pdsim_line_read_io_error) {
        pdsim_test_set_errno_message(
            server->error_message,
            sizeof(server->error_message),
            "read request"
        );
    } else if (
        result != pdsim_line_read_success || received == 0
        || memchr(server->request, '\r', received) != NULL
        || memchr(server->request, '\0', received) != NULL
    ) {
        pdsim_test_set_message(
            server->error_message,
            sizeof(server->error_message),
            "invalid request framing"
        );
    }
}

static void pdsim_test_delay(uint32_t milliseconds) {
    struct timespec duration = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (long)(milliseconds % 1000) * 1000000L,
    };
    while (nanosleep(&duration, &duration) != 0 && errno == EINTR) {
    }
}

static void *pdsim_test_run_server(void *context) {
    pdsim_test_server *server = context;
    int server_descriptor = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_descriptor < 0) {
        char error_message[PDSIM_PROTOCOL_BUFFER_CAPACITY];
        pdsim_test_set_errno_message(error_message, sizeof(error_message), "socket");
        pdsim_test_publish_setup(server, false, error_message);
        return NULL;
    }

    struct sockaddr_un address = {0};
    address.sun_family = AF_UNIX;
    size_t path_length = strlcpy(
        address.sun_path,
        server->socket_path,
        sizeof(address.sun_path)
    );
    unlink(server->socket_path);
    if (path_length >= sizeof(address.sun_path)) {
        close(server_descriptor);
        pdsim_test_publish_setup(server, false, "test socket path is too long");
        return NULL;
    }
    address.sun_len = (uint8_t)(offsetof(struct sockaddr_un, sun_path) + path_length);
    if (
        bind(server_descriptor, (struct sockaddr *)&address, address.sun_len) != 0
        || listen(server_descriptor, 1) != 0
    ) {
        char error_message[PDSIM_PROTOCOL_BUFFER_CAPACITY];
        pdsim_test_set_errno_message(error_message, sizeof(error_message), "prepare server");
        close(server_descriptor);
        unlink(server->socket_path);
        pdsim_test_publish_setup(server, false, error_message);
        return NULL;
    }

    pdsim_test_publish_setup(server, true, "");
    int descriptor = accept(server_descriptor, NULL, NULL);
    if (descriptor < 0) {
        pdsim_test_set_errno_message(
            server->error_message,
            sizeof(server->error_message),
            "accept"
        );
    } else {
        if (pdsim_configure_socket(descriptor, 0, 0) != 0) {
            pdsim_test_set_errno_message(
                server->error_message,
                sizeof(server->error_message),
                "configure socket"
            );
        } else {
            pdsim_test_read_request(server, descriptor);
            pdsim_test_delay(server->response_delay_milliseconds);
            if (
                server->request[0] != '\0' && server->response[0] != '\0'
                && pdsim_write_line(descriptor, server->response, strlen(server->response)) != 0
                && errno != EPIPE
            ) {
                pdsim_test_set_errno_message(
                    server->error_message,
                    sizeof(server->error_message),
                    "write response"
                );
            }
        }
        close(descriptor);
    }

    close(server_descriptor);
    unlink(server->socket_path);
    return NULL;
}

static void pdsim_test_destroy_server(pdsim_test_server *server) {
    pthread_cond_destroy(&server->condition);
    pthread_mutex_destroy(&server->mutex);
    free(server);
}

pdsim_test_server *pdsim_test_server_start(
    const char *socket_path,
    const char *response,
    uint32_t response_delay_milliseconds,
    char *error_message,
    size_t error_capacity
) {
    if (socket_path == NULL || response == NULL) {
        pdsim_test_set_message(error_message, error_capacity, "invalid test server input");
        return NULL;
    }

    pdsim_test_server *server = calloc(1, sizeof(*server));
    if (server == NULL) {
        pdsim_test_set_message(error_message, error_capacity, "could not allocate test server");
        return NULL;
    }
    if (pthread_mutex_init(&server->mutex, NULL) != 0) {
        free(server);
        pdsim_test_set_message(error_message, error_capacity, "could not initialize test server");
        return NULL;
    }
    if (pthread_cond_init(&server->condition, NULL) != 0) {
        pthread_mutex_destroy(&server->mutex);
        free(server);
        pdsim_test_set_message(error_message, error_capacity, "could not initialize test server");
        return NULL;
    }
    if (
        strlcpy(server->socket_path, socket_path, sizeof(server->socket_path))
            >= sizeof(server->socket_path)
        || strlcpy(server->response, response, sizeof(server->response))
            >= sizeof(server->response)
    ) {
        pdsim_test_destroy_server(server);
        pdsim_test_set_message(error_message, error_capacity, "test server input is too long");
        return NULL;
    }
    server->response_delay_milliseconds = response_delay_milliseconds;

    if (pthread_create(&server->thread, NULL, pdsim_test_run_server, server) != 0) {
        pdsim_test_destroy_server(server);
        pdsim_test_set_message(error_message, error_capacity, "could not start test server");
        return NULL;
    }

    pthread_mutex_lock(&server->mutex);
    while (!server->is_ready) {
        pthread_cond_wait(&server->condition, &server->mutex);
    }
    bool setup_succeeded = server->setup_succeeded;
    pdsim_test_set_message(error_message, error_capacity, server->error_message);
    pthread_mutex_unlock(&server->mutex);

    if (!setup_succeeded) {
        pthread_join(server->thread, NULL);
        pdsim_test_destroy_server(server);
        return NULL;
    }
    return server;
}

bool pdsim_test_server_finish(
    pdsim_test_server *server,
    char *request,
    size_t request_capacity,
    char *error_message,
    size_t error_capacity
) {
    if (server == NULL || request == NULL || request_capacity == 0) {
        pdsim_test_set_message(error_message, error_capacity, "invalid test server output");
        return false;
    }

    pthread_join(server->thread, NULL);
    pdsim_test_set_message(request, request_capacity, server->request);
    pdsim_test_set_message(error_message, error_capacity, server->error_message);
    bool succeeded = server->error_message[0] == '\0';
    pdsim_test_destroy_server(server);
    return succeeded;
}
