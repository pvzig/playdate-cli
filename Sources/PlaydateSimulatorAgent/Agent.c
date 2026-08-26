#include "DescriptorIO.h"
#include "PlaydateSimulatorProtocol.h"

#include "CommandParser.h"
#include "ButtonInputState.h"
#include "FramebufferImage.h"
#include "GIFRecorder.h"
#include "MachOSymbolResolver.h"
#include "ScreenshotWriter.h"

#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

typedef void (*pdsim_handle_button_function)(int button, int is_down);
typedef void (*pdsim_set_crank_position_function)(float degrees);
typedef void (*pdsim_set_crank_docked_function)(int is_docked);
typedef void (*pdsim_set_accelerometer_function)(float x, float y, float z);
typedef void (*pdsim_load_pdx_function)(const char *path);
typedef int (*pdsim_get_volume_function)(void);
typedef void (*pdsim_set_volume_function)(int volume);
typedef uint8_t *(*pdsim_get_framebuffer_function)(void);
typedef int (*pdsim_is_locked_function)(void);
typedef void (*pdsim_ui_event_function)(void *main_frame, void *event);
typedef void (*pdsim_set_paused_function)(void *main_frame, bool is_paused, int reason);

typedef enum {
    pdsim_action_button,
    pdsim_action_crank,
    pdsim_action_crank_docked,
    pdsim_action_accelerometer,
    pdsim_action_load,
    pdsim_action_lock,
    pdsim_action_pause,
    pdsim_action_simulator_reset,
    pdsim_action_volume_adjust,
    pdsim_action_volume_set,
    pdsim_action_screenshot_copy,
    pdsim_action_toolbar_pause,
    pdsim_action_toolbar_console,
    pdsim_action_toolbar_sampler,
    pdsim_action_toolbar_memory,
    pdsim_action_toolbar_record,
    pdsim_action_toolbar_device,
    pdsim_action_toolbar_controls,
} pdsim_action_kind;

typedef struct {
    pdsim_action_kind kind;
    int button;
    int integer_value;
    float x;
    float y;
    float z;
    uint8_t *framebuffer_copy;
    const char *path;
    uint64_t button_generation;
    bool begins_button_input;
    bool conditional_button_release;
    bool succeeded;
} pdsim_action;

typedef struct {
    pdsim_action action;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    atomic_uint references;
    bool started;
    bool completed;
    bool cancelled;
    char path[PDSIM_PROTOCOL_BUFFER_CAPACITY];
    uint8_t *framebuffer_copy;
} pdsim_pending_action;

#define PDSIM_MAIN_QUEUE_TIMEOUT_MILLISECONDS 5000

static pdsim_handle_button_function pdsim_handle_button;
static pdsim_set_crank_position_function pdsim_set_crank_position;
static pdsim_set_crank_docked_function pdsim_set_crank_docked;
static pdsim_set_accelerometer_function pdsim_set_accelerometer;
static pdsim_load_pdx_function pdsim_load_pdx;
static pdsim_get_volume_function pdsim_get_volume;
static pdsim_set_volume_function pdsim_set_volume;
static pdsim_get_framebuffer_function pdsim_get_framebuffer;
static pdsim_is_locked_function pdsim_is_locked;
static pdsim_ui_event_function pdsim_on_pause_button;
static pdsim_ui_event_function pdsim_on_reset;
static pdsim_ui_event_function pdsim_on_lock;
static pdsim_set_paused_function pdsim_set_paused;
static pdsim_ui_event_function pdsim_on_console_button;
static pdsim_ui_event_function pdsim_on_sampler_button;
static pdsim_ui_event_function pdsim_on_memory_button;
static pdsim_ui_event_function pdsim_on_record;
static pdsim_ui_event_function pdsim_on_device_action;
static pdsim_ui_event_function pdsim_on_toggle_device_controls;
static void **pdsim_main_frame_storage;
static char pdsim_active_pdx_path[PDSIM_PROTOCOL_BUFFER_CAPACITY];

static void pdsim_resolve_symbols(void) {
#define PDSIM_RESOLVE_SIMULATOR_SYMBOL(identifier, symbol_name) \
    *(void **)(&pdsim_##identifier) = dlsym(RTLD_DEFAULT, symbol_name);
    PDSIM_REQUIRED_SIMULATOR_SYMBOLS(PDSIM_RESOLVE_SIMULATOR_SYMBOL)
#undef PDSIM_RESOLVE_SIMULATOR_SYMBOL
    pdsim_main_frame_storage = pdsim_find_main_image_symbol(pdsim_main_frame_symbol());
}

static void *pdsim_main_frame(void) {
    return pdsim_main_frame_storage == NULL ? NULL : *pdsim_main_frame_storage;
}

static bool pdsim_has_ui(void) {
    return pdsim_main_frame() != NULL && pdsim_on_pause_button != NULL && pdsim_on_reset != NULL
        && pdsim_on_lock != NULL && pdsim_set_paused != NULL && pdsim_on_console_button != NULL
        && pdsim_on_sampler_button != NULL && pdsim_on_memory_button != NULL
        && pdsim_on_record != NULL && pdsim_on_device_action != NULL
        && pdsim_on_toggle_device_controls != NULL;
}

static int pdsim_clamp_volume(int volume) {
    if (volume < 0) {
        return 0;
    }
    if (volume > 255) {
        return 255;
    }
    return volume;
}

static void pdsim_perform_action(void *context) {
    pdsim_action *action = context;
    void *main_frame = pdsim_main_frame();

    switch (action->kind) {
    case pdsim_action_button:
        if (action->begins_button_input) {
            action->button_generation = pdsim_begin_button_input(action->button);
        }
        if (
            !action->conditional_button_release
            || pdsim_is_current_button_input(
                action->button,
                action->button_generation
            )
        ) {
            pdsim_handle_button(action->button, action->integer_value);
        }
        break;
    case pdsim_action_crank:
        pdsim_set_crank_position(action->x);
        break;
    case pdsim_action_crank_docked:
        pdsim_set_crank_docked(action->integer_value);
        break;
    case pdsim_action_accelerometer:
        pdsim_set_accelerometer(action->x, action->y, action->z);
        break;
    case pdsim_action_load:
        pdsim_load_pdx(action->path);
        break;
    case pdsim_action_lock:
        pdsim_on_lock(main_frame, NULL);
        break;
    case pdsim_action_pause:
        pdsim_set_paused(main_frame, action->integer_value != 0, 2);
        break;
    case pdsim_action_simulator_reset:
        pdsim_on_reset(main_frame, NULL);
        break;
    case pdsim_action_volume_adjust: {
        int delta = (int)lround((double)action->integer_value * 255.0 / 100.0);
        int volume = pdsim_clamp_volume(pdsim_get_volume() + delta);
        pdsim_set_volume(volume);
        action->integer_value = volume;
        break;
    }
    case pdsim_action_volume_set: {
        int volume = (int)lround((double)action->integer_value * 255.0 / 100.0);
        volume = pdsim_clamp_volume(volume);
        pdsim_set_volume(volume);
        action->integer_value = volume;
        break;
    }
    case pdsim_action_screenshot_copy: {
        uint8_t *framebuffer = pdsim_get_framebuffer();
        if (framebuffer != NULL) {
            memcpy(action->framebuffer_copy, framebuffer, PDSIM_FRAMEBUFFER_SIZE);
            action->succeeded = true;
        }
        break;
    }
    case pdsim_action_toolbar_pause:
        pdsim_on_pause_button(main_frame, NULL);
        break;
    case pdsim_action_toolbar_console:
        pdsim_on_console_button(main_frame, NULL);
        break;
    case pdsim_action_toolbar_sampler:
        pdsim_on_sampler_button(main_frame, NULL);
        break;
    case pdsim_action_toolbar_memory:
        pdsim_on_memory_button(main_frame, NULL);
        break;
    case pdsim_action_toolbar_record:
        pdsim_on_record(main_frame, NULL);
        break;
    case pdsim_action_toolbar_device:
        pdsim_on_device_action(main_frame, NULL);
        break;
    case pdsim_action_toolbar_controls:
        pdsim_on_toggle_device_controls(main_frame, NULL);
        break;
    }
}

static void pdsim_release_pending_action(pdsim_pending_action *pending_action) {
    if (
        atomic_fetch_sub_explicit(
            &pending_action->references,
            1,
            memory_order_acq_rel
        )
        != 1
    ) {
        return;
    }

    free(pending_action->framebuffer_copy);
    pthread_cond_destroy(&pending_action->condition);
    pthread_mutex_destroy(&pending_action->mutex);
    free(pending_action);
}

static void pdsim_perform_pending_action(void *context) {
    pdsim_pending_action *pending_action = context;

    pthread_mutex_lock(&pending_action->mutex);
    if (pending_action->cancelled) {
        pending_action->completed = true;
        pthread_cond_signal(&pending_action->condition);
        pthread_mutex_unlock(&pending_action->mutex);
        pdsim_release_pending_action(pending_action);
        return;
    }
    pending_action->started = true;
    pthread_mutex_unlock(&pending_action->mutex);

    pdsim_perform_action(&pending_action->action);

    pthread_mutex_lock(&pending_action->mutex);
    pending_action->completed = true;
    pthread_cond_signal(&pending_action->condition);
    pthread_mutex_unlock(&pending_action->mutex);
    pdsim_release_pending_action(pending_action);
}

static bool pdsim_main_queue_deadline(struct timespec *deadline) {
    if (clock_gettime(CLOCK_REALTIME, deadline) != 0) {
        return false;
    }

    deadline->tv_nsec += PDSIM_MAIN_QUEUE_TIMEOUT_MILLISECONDS * 1000000L;
    deadline->tv_sec += deadline->tv_nsec / 1000000000L;
    deadline->tv_nsec %= 1000000000L;
    return true;
}

static bool pdsim_dispatch_action(pdsim_action *action) {
    pdsim_pending_action *pending_action = calloc(1, sizeof(*pending_action));
    if (pending_action == NULL) {
        return false;
    }
    if (pthread_mutex_init(&pending_action->mutex, NULL) != 0) {
        free(pending_action);
        return false;
    }
    if (pthread_cond_init(&pending_action->condition, NULL) != 0) {
        pthread_mutex_destroy(&pending_action->mutex);
        free(pending_action);
        return false;
    }

    pending_action->action = *action;
    if (action->path != NULL) {
        size_t path_length = strlcpy(
            pending_action->path,
            action->path,
            sizeof(pending_action->path)
        );
        if (path_length >= sizeof(pending_action->path)) {
            pthread_cond_destroy(&pending_action->condition);
            pthread_mutex_destroy(&pending_action->mutex);
            free(pending_action);
            return false;
        }
        pending_action->action.path = pending_action->path;
    }
    if (action->framebuffer_copy != NULL) {
        pending_action->framebuffer_copy = malloc(PDSIM_FRAMEBUFFER_SIZE);
        if (pending_action->framebuffer_copy == NULL) {
            pthread_cond_destroy(&pending_action->condition);
            pthread_mutex_destroy(&pending_action->mutex);
            free(pending_action);
            return false;
        }
        pending_action->action.framebuffer_copy = pending_action->framebuffer_copy;
    }

    struct timespec deadline;
    if (!pdsim_main_queue_deadline(&deadline)) {
        free(pending_action->framebuffer_copy);
        pthread_cond_destroy(&pending_action->condition);
        pthread_mutex_destroy(&pending_action->mutex);
        free(pending_action);
        return false;
    }

    atomic_init(&pending_action->references, 2);
    dispatch_async_f(
        dispatch_get_main_queue(),
        pending_action,
        pdsim_perform_pending_action
    );

    pthread_mutex_lock(&pending_action->mutex);
    int wait_result = 0;
    while (!pending_action->completed && wait_result == 0) {
        wait_result = pthread_cond_timedwait(
            &pending_action->condition,
            &pending_action->mutex,
            &deadline
        );
    }
    bool completed = pending_action->completed && !pending_action->cancelled;
    if (!completed && !pending_action->started) {
        pending_action->cancelled = true;
    }

    uint8_t *framebuffer_copy = action->framebuffer_copy;
    const char *path = action->path;
    if (completed) {
        if (framebuffer_copy != NULL) {
            memcpy(
                framebuffer_copy,
                pending_action->framebuffer_copy,
                PDSIM_FRAMEBUFFER_SIZE
            );
        }
        *action = pending_action->action;
        action->framebuffer_copy = framebuffer_copy;
        action->path = path;
    }
    pthread_mutex_unlock(&pending_action->mutex);
    pdsim_release_pending_action(pending_action);
    return completed;
}

static void pdsim_reply(int descriptor, const char *response);

static bool pdsim_dispatch_action_or_reply(
    int descriptor,
    pdsim_action *action
) {
    if (pdsim_dispatch_action(action)) {
        return true;
    }
    pdsim_reply(descriptor, "error Simulator main queue timed out");
    return false;
}

static void pdsim_perform_owned_action(void *context) {
    pdsim_perform_action(context);
    free(context);
}

static bool pdsim_schedule_action(const pdsim_action *action) {
    pdsim_action *owned_action = malloc(sizeof(*owned_action));
    if (owned_action == NULL) {
        return false;
    }
    *owned_action = *action;
    dispatch_async_f(dispatch_get_main_queue(), owned_action, pdsim_perform_owned_action);
    return true;
}

static bool pdsim_schedule_action_after_milliseconds(
    const pdsim_action *action,
    int delay_milliseconds
) {
    pdsim_action *owned_action = malloc(sizeof(*owned_action));
    if (owned_action == NULL) {
        return false;
    }
    *owned_action = *action;
    dispatch_time_t deadline = dispatch_time(
        DISPATCH_TIME_NOW,
        (int64_t)delay_milliseconds * NSEC_PER_MSEC
    );
    dispatch_after_f(
        deadline,
        dispatch_get_main_queue(),
        owned_action,
        pdsim_perform_owned_action
    );
    return true;
}

static void pdsim_reply(int descriptor, const char *response) {
    (void)pdsim_write_line(descriptor, response, strlen(response));
}

static void pdsim_reply_error(int descriptor, const char *message) {
    char response[PDSIM_PROTOCOL_BUFFER_CAPACITY];
    snprintf(response, sizeof(response), "error %s", message);
    pdsim_reply(descriptor, response);
}

static bool pdsim_sleep_milliseconds(int milliseconds) {
    struct timespec requested = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000L,
    };

    while (nanosleep(&requested, &requested) != 0) {
        if (errno != EINTR) {
            return false;
        }
    }
    return true;
}

static void pdsim_handle_status(int descriptor) {
    char response[PDSIM_PROTOCOL_BUFFER_CAPACITY];
    snprintf(
        response,
        sizeof(response),
        "ok protocol=%d pid=%d buttons=%d crank=%d dock=%d "
        "accelerometer=%d lock=%d "
        "volume=%d ui=%d screenshot=%d load=%d record=%d",
        PLAYDATE_SIMULATOR_AGENT_PROTOCOL_VERSION,
        getpid(),
        pdsim_handle_button != NULL,
        pdsim_set_crank_position != NULL,
        pdsim_set_crank_docked != NULL,
        pdsim_set_accelerometer != NULL,
        pdsim_is_locked != NULL && pdsim_main_frame() != NULL && pdsim_on_lock != NULL,
        pdsim_get_volume != NULL && pdsim_set_volume != NULL,
        pdsim_has_ui(),
        pdsim_get_framebuffer != NULL,
        pdsim_load_pdx != NULL,
        pdsim_get_framebuffer != NULL
    );
    pdsim_reply(descriptor, response);
}

static void pdsim_handle_button_command(
    int descriptor,
    const pdsim_agent_command *command
) {
    if (pdsim_handle_button == NULL) {
        pdsim_reply(descriptor, "error sim_handleButton is unavailable");
        return;
    }

    pdsim_action action = {
        .kind = pdsim_action_button,
        .button = command->button,
        .integer_value = command->integer_value,
        .begins_button_input = true,
    };
    if (!pdsim_dispatch_action_or_reply(descriptor, &action)) {
        return;
    }
    pdsim_reply(descriptor, "ok");
}

static void pdsim_handle_press_command(
    int descriptor,
    const pdsim_agent_command *command
) {
    if (pdsim_handle_button == NULL) {
        pdsim_reply(descriptor, "error sim_handleButton is unavailable");
        return;
    }

    pdsim_action action = {
        .kind = pdsim_action_button,
        .button = command->button,
        .integer_value = 1,
        .begins_button_input = true,
    };
    if (!pdsim_dispatch_action_or_reply(descriptor, &action)) {
        return;
    }
    action.integer_value = 0;
    action.begins_button_input = false;
    action.conditional_button_release = true;
    if (!pdsim_schedule_action_after_milliseconds(
            &action,
            command->duration_milliseconds
        )) {
        (void)pdsim_dispatch_action(&action);
        pdsim_reply(descriptor, "error could not schedule button release");
        return;
    }

    pdsim_reply(descriptor, "ok");
}

static void pdsim_handle_crank_command(
    int descriptor,
    const pdsim_agent_command *command
) {
    if (pdsim_set_crank_position == NULL) {
        pdsim_reply(descriptor, "error sim_setCrankPosition is unavailable");
        return;
    }

    pdsim_action action = {
        .kind = pdsim_action_crank,
        .x = command->x,
    };
    if (!pdsim_dispatch_action_or_reply(descriptor, &action)) {
        return;
    }
    pdsim_reply(descriptor, "ok");
}

static void pdsim_handle_crank_docked_command(
    int descriptor,
    const pdsim_agent_command *command
) {
    if (pdsim_set_crank_docked == NULL) {
        pdsim_reply(descriptor, "error sim_setCrankDocked is unavailable");
        return;
    }

    pdsim_action action = {
        .kind = pdsim_action_crank_docked,
        .integer_value = command->integer_value,
    };
    if (!pdsim_dispatch_action_or_reply(descriptor, &action)) {
        return;
    }
    pdsim_reply(descriptor, "ok");
}

static void pdsim_handle_accelerometer_command(
    int descriptor,
    const pdsim_agent_command *command
) {
    if (pdsim_set_accelerometer == NULL) {
        pdsim_reply(descriptor, "error sim_setAccelerometer is unavailable");
        return;
    }

    pdsim_action action = {
        .kind = pdsim_action_accelerometer,
        .x = command->x,
        .y = command->y,
        .z = command->z,
    };
    if (!pdsim_dispatch_action_or_reply(descriptor, &action)) {
        return;
    }
    pdsim_reply(descriptor, "ok");
}

static void pdsim_handle_lock_command(int descriptor) {
    if (pdsim_main_frame() == NULL || pdsim_on_lock == NULL || pdsim_is_locked == NULL) {
        pdsim_reply(descriptor, "error lock control is unavailable");
        return;
    }

    pdsim_action action = {.kind = pdsim_action_lock};
    if (!pdsim_dispatch_action_or_reply(descriptor, &action)) {
        return;
    }

    char response[64];
    snprintf(response, sizeof(response), "ok locked=%d", pdsim_is_locked() != 0);
    pdsim_reply(descriptor, response);
}

static void pdsim_handle_load_command(
    int descriptor,
    const pdsim_agent_command *command
) {
    if (pdsim_load_pdx == NULL) {
        pdsim_reply(descriptor, "error PDX loading is unavailable");
        return;
    }

    pdsim_action action = {
        .kind = pdsim_action_load,
        .path = command->path,
    };
    if (!pdsim_dispatch_action_or_reply(descriptor, &action)) {
        return;
    }
    (void)strlcpy(
        pdsim_active_pdx_path,
        command->path,
        sizeof(pdsim_active_pdx_path)
    );
    pdsim_reply(descriptor, "ok");
}

static void pdsim_handle_set_active_pdx_command(
    int descriptor,
    const pdsim_agent_command *command
) {
    size_t path_length = strlcpy(
        pdsim_active_pdx_path,
        command->path,
        sizeof(pdsim_active_pdx_path)
    );
    if (path_length >= sizeof(pdsim_active_pdx_path)) {
        pdsim_active_pdx_path[0] = '\0';
        pdsim_reply(descriptor, "error active PDX path is too long");
        return;
    }
    pdsim_reply(descriptor, "ok");
}

static void pdsim_handle_pause_command(
    int descriptor,
    const pdsim_agent_command *command
) {
    if (pdsim_main_frame() == NULL || pdsim_set_paused == NULL) {
        pdsim_reply(descriptor, "error pause control is unavailable");
        return;
    }

    pdsim_action action = {
        .kind = pdsim_action_pause,
        .integer_value = command->integer_value,
    };
    if (!pdsim_dispatch_action_or_reply(descriptor, &action)) {
        return;
    }

    char response[64];
    snprintf(response, sizeof(response), "ok paused=%d", command->integer_value);
    pdsim_reply(descriptor, response);
}

static void pdsim_handle_restart_command(int descriptor) {
    pdsim_action action = {0};
    if (pdsim_active_pdx_path[0] != '\0') {
        if (pdsim_load_pdx == NULL) {
            pdsim_reply(descriptor, "error restart control is unavailable");
            return;
        }
        action.kind = pdsim_action_load;
        action.path = pdsim_active_pdx_path;
    } else {
        if (pdsim_main_frame() == NULL || pdsim_on_reset == NULL) {
            pdsim_reply(descriptor, "error restart control is unavailable");
            return;
        }
        action.kind = pdsim_action_simulator_reset;
    }

    if (!pdsim_dispatch_action_or_reply(descriptor, &action)) {
        return;
    }
    pdsim_reply(descriptor, "ok");
}

static void pdsim_handle_volume_command(
    int descriptor,
    const pdsim_agent_command *command
) {
    if (pdsim_get_volume == NULL || pdsim_set_volume == NULL) {
        pdsim_reply(descriptor, "error volume control is unavailable");
        return;
    }

    pdsim_action action = {
        .kind = command->kind == pdsim_agent_command_volume_adjust
            ? pdsim_action_volume_adjust
            : pdsim_action_volume_set,
        .integer_value = command->integer_value,
    };
    if (!pdsim_dispatch_action_or_reply(descriptor, &action)) {
        return;
    }

    int result_percent = (int)lround((double)action.integer_value * 100.0 / 255.0);
    char response[64];
    snprintf(response, sizeof(response), "ok volume=%d", result_percent);
    pdsim_reply(descriptor, response);
}

static pdsim_action_kind pdsim_toolbar_action_kind(pdsim_toolbar_action action) {
    switch (action) {
    case pdsim_toolbar_pause:
        return pdsim_action_toolbar_pause;
    case pdsim_toolbar_restart:
        return pdsim_action_simulator_reset;
    case pdsim_toolbar_console:
        return pdsim_action_toolbar_console;
    case pdsim_toolbar_sampler:
        return pdsim_action_toolbar_sampler;
    case pdsim_toolbar_memory:
        return pdsim_action_toolbar_memory;
    case pdsim_toolbar_record:
        return pdsim_action_toolbar_record;
    case pdsim_toolbar_device:
        return pdsim_action_toolbar_device;
    case pdsim_toolbar_controls:
        return pdsim_action_toolbar_controls;
    }

    return pdsim_action_toolbar_pause;
}

static void pdsim_handle_toolbar_command(
    int descriptor,
    const pdsim_agent_command *command
) {
    if (!pdsim_has_ui()) {
        pdsim_reply(descriptor, "error simulator UI control is unavailable");
        return;
    }

    pdsim_action action = {
        .kind = pdsim_toolbar_action_kind(command->toolbar_action),
    };

    pdsim_reply(
        descriptor,
        pdsim_schedule_action(&action) ? "ok scheduled" : "error could not schedule toolbar action"
    );
}

static void pdsim_handle_screenshot_command(
    int descriptor,
    const pdsim_agent_command *command
) {
    if (pdsim_get_framebuffer == NULL) {
        pdsim_reply(descriptor, "error screenshot capture is unavailable");
        return;
    }

    uint8_t *framebuffer_copy = malloc(PDSIM_FRAMEBUFFER_SIZE);
    if (framebuffer_copy == NULL) {
        pdsim_reply(descriptor, "error could not allocate framebuffer copy");
        return;
    }

    pdsim_action action = {
        .kind = pdsim_action_screenshot_copy,
        .framebuffer_copy = framebuffer_copy,
    };
    if (!pdsim_dispatch_action_or_reply(descriptor, &action)) {
        free(framebuffer_copy);
        return;
    }
    if (!action.succeeded) {
        free(framebuffer_copy);
        pdsim_reply(descriptor, "error framebuffer is unavailable");
        return;
    }

    char error_message[256];
    bool wrote_screenshot = pdsim_write_screenshot_png(
        framebuffer_copy,
        command->path,
        error_message,
        sizeof(error_message)
    );
    free(framebuffer_copy);

    if (!wrote_screenshot) {
        pdsim_reply_error(descriptor, error_message);
        return;
    }
    pdsim_reply(descriptor, "ok");
}

static bool pdsim_capture_recording_frame(uint8_t *framebuffer, void *context) {
    (void)context;
    if (pdsim_get_framebuffer == NULL) {
        return false;
    }

    pdsim_action action = {
        .kind = pdsim_action_screenshot_copy,
        .framebuffer_copy = framebuffer,
    };
    return pdsim_dispatch_action(&action) && action.succeeded;
}

static void pdsim_handle_record_start_command(
    int descriptor,
    const pdsim_agent_command *command
) {
    if (pdsim_get_framebuffer == NULL) {
        pdsim_reply(descriptor, "error GIF recording is unavailable");
        return;
    }

    char error_message[256];
    if (!pdsim_gif_recorder_start(
            command->path,
            pdsim_capture_recording_frame,
            NULL,
            error_message,
            sizeof(error_message)
        )) {
        pdsim_reply_error(descriptor, error_message);
        return;
    }
    pdsim_reply(descriptor, "ok recording=1");
}

static void pdsim_handle_record_stop_command(int descriptor) {
    char error_message[256];
    if (!pdsim_gif_recorder_stop(error_message, sizeof(error_message))) {
        pdsim_reply_error(descriptor, error_message);
        return;
    }
    pdsim_reply(descriptor, "ok recording=0");
}

static void pdsim_handle_request(int descriptor, const char *request) {
    pdsim_agent_command command;
    pdsim_agent_command_parse_result parse_result = pdsim_parse_agent_command(
        request,
        &command
    );
    if (parse_result != pdsim_agent_command_parse_success) {
        pdsim_reply(descriptor, pdsim_agent_command_parse_error(parse_result));
        return;
    }

    switch (command.kind) {
    case pdsim_agent_command_status:
        pdsim_handle_status(descriptor);
        break;
    case pdsim_agent_command_button:
        pdsim_handle_button_command(descriptor, &command);
        break;
    case pdsim_agent_command_press:
        pdsim_handle_press_command(descriptor, &command);
        break;
    case pdsim_agent_command_crank:
        pdsim_handle_crank_command(descriptor, &command);
        break;
    case pdsim_agent_command_crank_docked:
        pdsim_handle_crank_docked_command(descriptor, &command);
        break;
    case pdsim_agent_command_accelerometer:
        pdsim_handle_accelerometer_command(descriptor, &command);
        break;
    case pdsim_agent_command_lock:
        pdsim_handle_lock_command(descriptor);
        break;
    case pdsim_agent_command_load:
        pdsim_handle_load_command(descriptor, &command);
        break;
    case pdsim_agent_command_set_active_pdx:
        pdsim_handle_set_active_pdx_command(descriptor, &command);
        break;
    case pdsim_agent_command_pause:
        pdsim_handle_pause_command(descriptor, &command);
        break;
    case pdsim_agent_command_restart:
        pdsim_handle_restart_command(descriptor);
        break;
    case pdsim_agent_command_volume_adjust:
    case pdsim_agent_command_volume_set:
        pdsim_handle_volume_command(descriptor, &command);
        break;
    case pdsim_agent_command_screenshot:
        pdsim_handle_screenshot_command(descriptor, &command);
        break;
    case pdsim_agent_command_toolbar:
        pdsim_handle_toolbar_command(descriptor, &command);
        break;
    case pdsim_agent_command_record_start:
        pdsim_handle_record_start_command(descriptor, &command);
        break;
    case pdsim_agent_command_record_stop:
        pdsim_handle_record_stop_command(descriptor);
        break;
    }
}

static void pdsim_serve_connection(int descriptor) {
    char request[PDSIM_PROTOCOL_BUFFER_CAPACITY];
    size_t received = 0;
    pdsim_line_read_result read_result = pdsim_read_line(
        descriptor,
        request,
        sizeof(request),
        &received
    );
    if (read_result == pdsim_line_read_io_error) {
        pdsim_reply(
            descriptor,
            errno == EAGAIN || errno == EWOULDBLOCK
                ? "error request timeout"
                : "error read failed"
        );
        return;
    }
    if (
        read_result != pdsim_line_read_success || received == 0
        || memchr(request, '\r', received) != NULL
        || memchr(request, '\0', received) != NULL
    ) {
        pdsim_reply(descriptor, "error invalid request size");
        return;
    }

    pdsim_handle_request(descriptor, request);
}

static bool pdsim_prepare_socket_path(char *socket_path, size_t capacity) {
    char directory_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    int directory_length = pdsim_socket_directory_path(
        directory_path,
        sizeof(directory_path),
        getuid()
    );
    if (directory_length < 0 || (size_t)directory_length >= sizeof(directory_path)) {
        errno = ENAMETOOLONG;
        return false;
    }

    if (mkdir(directory_path, 0700) != 0 && errno != EEXIST) {
        return false;
    }

    struct stat directory_status;
    if (lstat(directory_path, &directory_status) != 0) {
        return false;
    }
    if (!S_ISDIR(directory_status.st_mode) || directory_status.st_uid != getuid()) {
        errno = EPERM;
        return false;
    }
    if ((directory_status.st_mode & 077) != 0 && chmod(directory_path, 0700) != 0) {
        return false;
    }

    int socket_length = pdsim_socket_path(
        socket_path,
        capacity,
        getuid(),
        getpid()
    );
    if (socket_length < 0 || (size_t)socket_length >= capacity) {
        errno = ENAMETOOLONG;
        return false;
    }

    struct stat socket_status;
    if (lstat(socket_path, &socket_status) == 0) {
        if (!S_ISSOCK(socket_status.st_mode) || socket_status.st_uid != getuid()) {
            errno = EPERM;
            return false;
        }
        if (unlink(socket_path) != 0) {
            return false;
        }
    } else if (errno != ENOENT) {
        return false;
    }

    return true;
}

static void pdsim_report_server_error(const char *operation, int error_number) {
    fprintf(
        stderr,
        "playdate-simctl agent could not %s: %s\n",
        operation,
        strerror(error_number)
    );
    fflush(stderr);
}

static void *pdsim_run_server(void *unused) {
    (void)unused;
    pthread_setname_np("playdate-simctl-agent");

    char socket_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    if (!pdsim_prepare_socket_path(socket_path, sizeof(socket_path))) {
        pdsim_report_server_error("prepare its private socket path", errno);
        return NULL;
    }

    int server_descriptor = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_descriptor < 0) {
        pdsim_report_server_error("create its server socket", errno);
        return NULL;
    }
    if (pdsim_configure_socket(server_descriptor, 0, 0) != 0) {
        int configuration_error = errno;
        close(server_descriptor);
        pdsim_report_server_error("configure its server socket", configuration_error);
        return NULL;
    }

    struct sockaddr_un address = {0};
    address.sun_family = AF_UNIX;
    size_t socket_path_length = strlcpy(
        address.sun_path,
        socket_path,
        sizeof(address.sun_path)
    );
    if (socket_path_length >= sizeof(address.sun_path)) {
        close(server_descriptor);
        pdsim_report_server_error("copy its server socket path", ENAMETOOLONG);
        return NULL;
    }
    address.sun_len = (uint8_t)(offsetof(struct sockaddr_un, sun_path) + socket_path_length);

    if (bind(server_descriptor, (struct sockaddr *)&address, address.sun_len) != 0) {
        int bind_error = errno;
        close(server_descriptor);
        pdsim_report_server_error("bind its server socket", bind_error);
        return NULL;
    }
    if (chmod(socket_path, 0600) != 0) {
        int mode_error = errno;
        close(server_descriptor);
        (void)unlink(socket_path);
        pdsim_report_server_error("secure its server socket", mode_error);
        return NULL;
    }
    if (listen(server_descriptor, SOMAXCONN) != 0) {
        int listen_error = errno;
        close(server_descriptor);
        (void)unlink(socket_path);
        pdsim_report_server_error("listen on its server socket", listen_error);
        return NULL;
    }

    for (;;) {
        int descriptor = accept(server_descriptor, NULL, NULL);
        if (descriptor < 0) {
            if (errno == EINTR) {
                continue;
            }
            (void)pdsim_sleep_milliseconds(100);
            continue;
        }

        if (
            pdsim_configure_socket(
                descriptor,
                PDSIM_SOCKET_IO_TIMEOUT_MILLISECONDS,
                PDSIM_SOCKET_IO_TIMEOUT_MILLISECONDS
            )
            != 0
        ) {
            close(descriptor);
            continue;
        }
        pdsim_serve_connection(descriptor);
        close(descriptor);
    }

    close(server_descriptor);
    unlink(socket_path);
    return NULL;
}

__attribute__((constructor)) static void pdsim_start(void) {
    pdsim_resolve_symbols();

    pthread_t server_thread;
    int creation_result = pthread_create(
        &server_thread,
        NULL,
        pdsim_run_server,
        NULL
    );
    if (creation_result == 0) {
        pthread_detach(server_thread);
    } else {
        pdsim_report_server_error("start its server thread", creation_result);
    }
}
