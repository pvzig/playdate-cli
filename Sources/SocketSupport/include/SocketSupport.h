#ifndef SOCKET_SUPPORT_H
#define SOCKET_SUPPORT_H

#include "PlaydateSimulatorProtocol.h"

#include <lifetimebound.h>
#include <ptrcheck.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    pdsim_socket_success = 0,
    pdsim_socket_agent_not_running,
    pdsim_socket_peer_mismatch,
    pdsim_socket_timeout,
    pdsim_socket_io_error,
    pdsim_socket_protocol_error,
    pdsim_socket_invalid_argument,
} pdsim_socket_result;

typedef struct {
    int32_t expected_peer_process_identifier;
    uint32_t connect_timeout_milliseconds;
    uint32_t response_timeout_milliseconds;
} pdsim_socket_configuration;

#pragma clang assume_nonnull begin

pdsim_socket_result pdsim_send_command(
    const char *socket_path __noescape,
    const char *command __noescape,
    pdsim_socket_configuration configuration,
    char * __counted_by(response_capacity) response __noescape,
    size_t response_capacity,
    char * __counted_by(error_capacity) error_message __noescape,
    size_t error_capacity
);

#pragma clang assume_nonnull end

#endif
