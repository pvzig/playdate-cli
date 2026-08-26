#ifndef PLAYDATE_SIMULATOR_PROTOCOL_H
#define PLAYDATE_SIMULATOR_PROTOCOL_H

#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#define PLAYDATE_SIMULATOR_AGENT_PROTOCOL_VERSION 1

#define PDSIM_PROTOCOL_MAXIMUM_LINE_BYTES 510
#define PDSIM_PROTOCOL_BUFFER_CAPACITY (PDSIM_PROTOCOL_MAXIMUM_LINE_BYTES + 2)
#define PDSIM_SOCKET_IO_TIMEOUT_MILLISECONDS 5000
#define PDSIM_MAX_PRESS_MILLISECONDS 10000

#define PDSIM_BUTTON_LEFT_MASK 1
#define PDSIM_BUTTON_RIGHT_MASK 2
#define PDSIM_BUTTON_UP_MASK 4
#define PDSIM_BUTTON_DOWN_MASK 8
#define PDSIM_BUTTON_B_MASK 16
#define PDSIM_BUTTON_A_MASK 32
#define PDSIM_BUTTON_MENU_MASK 64

#define PDSIM_REQUIRED_SIMULATOR_SYMBOLS(APPLY)                                      \
    APPLY(handle_button, "sim_handleButton")                                        \
    APPLY(set_crank_position, "sim_setCrankPosition")                               \
    APPLY(set_crank_docked, "sim_setCrankDocked")                                   \
    APPLY(set_accelerometer, "sim_setAccelerometer")                                \
    APPLY(load_pdx, "sim_loadPDXAtPath")                                             \
    APPLY(get_volume, "sim_getVolume")                                               \
    APPLY(set_volume, "sim_setVolume")                                               \
    APPLY(get_framebuffer, "sim_getFrameBuffer")                                     \
    APPLY(is_locked, "pd_isLocked")                                                  \
    APPLY(on_pause_button, "_ZN9MainFrame13OnPauseButtonER14wxCommandEvent")         \
    APPLY(on_device_action, "_ZN9MainFrame14OnDeviceActionER14wxCommandEvent")       \
    APPLY(on_memory_button, "_ZN9MainFrame14OnMemoryButtonER14wxCommandEvent")       \
    APPLY(on_console_button, "_ZN9MainFrame15OnConsoleButtonER14wxCommandEvent")     \
    APPLY(on_sampler_button, "_ZN9MainFrame15OnSamplerButtonER14wxCommandEvent")     \
    APPLY(                                                                            \
        on_toggle_device_controls,                                                    \
        "_ZN9MainFrame22OnToggleDeviceControlsER14wxCommandEvent"                    \
    )                                                                                 \
    APPLY(on_lock, "_ZN9MainFrame6OnLockER14wxCommandEvent")                         \
    APPLY(on_reset, "_ZN9MainFrame7OnResetER14wxCommandEvent")                       \
    APPLY(on_record, "_ZN9MainFrame8OnRecordER14wxCommandEvent")                     \
    APPLY(set_paused, "_ZN9MainFrame9SetPausedEbi")

#define PDSIM_SYMBOL_NAME(identifier, name) name,
static const char *const pdsim_required_simulator_symbols[] = {
    PDSIM_REQUIRED_SIMULATOR_SYMBOLS(PDSIM_SYMBOL_NAME)
};
#undef PDSIM_SYMBOL_NAME

static inline size_t pdsim_required_simulator_symbol_count(void) {
    return sizeof(pdsim_required_simulator_symbols)
        / sizeof(pdsim_required_simulator_symbols[0]);
}

static inline const char *pdsim_required_simulator_symbol_at(size_t index) {
    if (index >= pdsim_required_simulator_symbol_count()) {
        return NULL;
    }
    return pdsim_required_simulator_symbols[index];
}

static inline const char *pdsim_main_frame_symbol(void) {
    return "__ZL5frame";
}

static inline int pdsim_socket_directory_path(
    char *destination,
    size_t capacity,
    uid_t user_identifier
) {
    return snprintf(
        destination,
        capacity,
        "/tmp/playdate-simctl-%u",
        (unsigned int)user_identifier
    );
}

static inline int pdsim_socket_path(
    char *destination,
    size_t capacity,
    uid_t user_identifier,
    pid_t process_identifier
) {
    return snprintf(
        destination,
        capacity,
        "/tmp/playdate-simctl-%u/%d.sock",
        (unsigned int)user_identifier,
        (int)process_identifier
    );
}

static inline int pdsim_set_socket_timeout(
    int descriptor,
    int option,
    uint32_t milliseconds
) {
    struct timeval timeout = {
        .tv_sec = (time_t)(milliseconds / 1000),
        .tv_usec = (suseconds_t)((milliseconds % 1000) * 1000),
    };
    return setsockopt(descriptor, SOL_SOCKET, option, &timeout, sizeof(timeout));
}

/// Configures descriptor inheritance, SIGPIPE handling, and optional I/O
/// deadlines. A zero timeout leaves that direction without a deadline.
static inline int pdsim_configure_socket(
    int descriptor,
    uint32_t send_timeout_milliseconds,
    uint32_t receive_timeout_milliseconds
) {
    int descriptor_flags = fcntl(descriptor, F_GETFD);
    if (
        descriptor_flags < 0
        || fcntl(descriptor, F_SETFD, descriptor_flags | FD_CLOEXEC) != 0
    ) {
        return -1;
    }

    int no_sigpipe = 1;
    if (
        setsockopt(
            descriptor,
            SOL_SOCKET,
            SO_NOSIGPIPE,
            &no_sigpipe,
            sizeof(no_sigpipe)
        )
        != 0
    ) {
        return -1;
    }
    if (
        send_timeout_milliseconds > 0
        && pdsim_set_socket_timeout(
               descriptor,
               SO_SNDTIMEO,
               send_timeout_milliseconds
           )
            != 0
    ) {
        return -1;
    }
    if (
        receive_timeout_milliseconds > 0
        && pdsim_set_socket_timeout(
               descriptor,
               SO_RCVTIMEO,
               receive_timeout_milliseconds
           )
            != 0
    ) {
        return -1;
    }
    return 0;
}

#endif
