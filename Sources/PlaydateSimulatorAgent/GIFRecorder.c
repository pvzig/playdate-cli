#include "GIFRecorder.h"

#include "CaptureFile.h"
#include "Framebuffer.h"
#include "GIFEncoder.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define PDSIM_GIF_SAMPLE_INTERVAL_NANOSECONDS 20000000ULL
#define PDSIM_GIF_CENTISECOND_NANOSECONDS 10000000LL
#define PDSIM_GIF_MAXIMUM_DELAY_CENTISECONDS UINT16_MAX
#define PDSIM_GIF_MAXIMUM_DELAY_NANOSECONDS \
    (PDSIM_GIF_MAXIMUM_DELAY_CENTISECONDS * PDSIM_GIF_CENTISECOND_NANOSECONDS)

typedef struct {
    pthread_t thread;
    atomic_bool stop_requested;
    bool capture_failed;
    bool encoding_failed;
    bool timing_failed;
    bool has_pending_frame;
    uint8_t pending_frame[PDSIM_FRAMEBUFFER_SIZE];
    uint8_t sample_frame[PDSIM_FRAMEBUFFER_SIZE];
    uint64_t pending_frame_time_nanoseconds;
    uint64_t capture_failure_time_nanoseconds;
    uint64_t stop_time_nanoseconds;
    int64_t timing_error_nanoseconds;
    pdsim_capture_frame_function capture_frame;
    void *capture_context;
    pdsim_gif_encoder *encoder;
    char output_path[PATH_MAX];
    char temporary_path[PDSIM_CAPTURE_TEMPORARY_PATH_CAPACITY];
} pdsim_gif_recorder;

// Agent requests execute serially. Only stop_requested and stop_time_nanoseconds
// cross the capture-thread boundary before pthread_join publishes all results.
static pdsim_gif_recorder *pdsim_active_recorder;

static void pdsim_set_error(
    char *error_message,
    size_t error_message_capacity,
    const char *message
) {
    if (error_message != NULL && error_message_capacity > 0) {
        snprintf(error_message, error_message_capacity, "%s", message);
    }
}

static void pdsim_set_errno_error(
    char *error_message,
    size_t error_message_capacity,
    const char *operation,
    int error_number
) {
    if (error_message != NULL && error_message_capacity > 0) {
        snprintf(
            error_message,
            error_message_capacity,
            "%s: %s",
            operation,
            strerror(error_number)
        );
    }
}

static bool pdsim_monotonic_time(uint64_t *nanoseconds) {
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) {
        return false;
    }
    *nanoseconds = (uint64_t)time.tv_sec * 1000000000ULL
        + (uint64_t)time.tv_nsec;
    return true;
}

static bool pdsim_sleep_until(
    uint64_t deadline_nanoseconds,
    const atomic_bool *stop_requested
) {
    for (;;) {
        if (atomic_load_explicit(stop_requested, memory_order_acquire)) {
            return true;
        }

        uint64_t now_nanoseconds;
        if (!pdsim_monotonic_time(&now_nanoseconds)) {
            return false;
        }
        if (now_nanoseconds >= deadline_nanoseconds) {
            return true;
        }

        uint64_t remaining_nanoseconds = deadline_nanoseconds - now_nanoseconds;
        struct timespec remaining = {
            .tv_sec = (time_t)(remaining_nanoseconds / 1000000000ULL),
            .tv_nsec = (long)(remaining_nanoseconds % 1000000000ULL),
        };
        if (nanosleep(&remaining, NULL) == 0 || errno == EINTR) {
            continue;
        }
        return false;
    }
}

static bool pdsim_emit_pending_frame(
    pdsim_gif_recorder *recorder,
    uint64_t end_time_nanoseconds
) {
    uint64_t elapsed_nanoseconds = end_time_nanoseconds
            > recorder->pending_frame_time_nanoseconds
        ? end_time_nanoseconds - recorder->pending_frame_time_nanoseconds
        : 0;
    int64_t timed_nanoseconds = (int64_t)elapsed_nanoseconds
        + recorder->timing_error_nanoseconds;
    int64_t delay_centiseconds = timed_nanoseconds
        / PDSIM_GIF_CENTISECOND_NANOSECONDS;
    if (delay_centiseconds < 1) {
        delay_centiseconds = 1;
    }
    recorder->timing_error_nanoseconds = timed_nanoseconds
        - delay_centiseconds * PDSIM_GIF_CENTISECOND_NANOSECONDS;

    while (delay_centiseconds > 0) {
        uint16_t frame_delay = delay_centiseconds
                > PDSIM_GIF_MAXIMUM_DELAY_CENTISECONDS
            ? PDSIM_GIF_MAXIMUM_DELAY_CENTISECONDS
            : (uint16_t)delay_centiseconds;
        if (!pdsim_gif_encoder_add_frame(
                recorder->encoder,
                recorder->pending_frame,
                PDSIM_FRAMEBUFFER_BYTES_PER_ROW,
                frame_delay
            )) {
            return false;
        }
        delay_centiseconds -= frame_delay;
    }
    return true;
}

static void pdsim_clear_framebuffer_padding(uint8_t *framebuffer) {
    for (size_t row = 0; row < PDSIM_SCREEN_HEIGHT; row++) {
        size_t padding_offset = row * PDSIM_FRAMEBUFFER_BYTES_PER_ROW
            + PDSIM_SCREEN_WIDTH / 8;
        memset(
            framebuffer + padding_offset,
            0,
            PDSIM_FRAMEBUFFER_BYTES_PER_ROW - PDSIM_SCREEN_WIDTH / 8
        );
    }
}

static bool pdsim_process_sample(
    pdsim_gif_recorder *recorder,
    uint64_t sample_time_nanoseconds
) {
    pdsim_clear_framebuffer_padding(recorder->sample_frame);
    if (!recorder->has_pending_frame) {
        memcpy(
            recorder->pending_frame,
            recorder->sample_frame,
            sizeof(recorder->pending_frame)
        );
        recorder->pending_frame_time_nanoseconds = sample_time_nanoseconds;
        recorder->has_pending_frame = true;
        return true;
    }

    bool changed = memcmp(
        recorder->pending_frame,
        recorder->sample_frame,
        sizeof(recorder->pending_frame)
    ) != 0;
    uint64_t elapsed_nanoseconds = sample_time_nanoseconds
            > recorder->pending_frame_time_nanoseconds
        ? sample_time_nanoseconds - recorder->pending_frame_time_nanoseconds
        : 0;
    if (
        !changed
        && elapsed_nanoseconds < PDSIM_GIF_MAXIMUM_DELAY_NANOSECONDS
    ) {
        return true;
    }
    if (!pdsim_emit_pending_frame(recorder, sample_time_nanoseconds)) {
        return false;
    }

    memcpy(
        recorder->pending_frame,
        recorder->sample_frame,
        sizeof(recorder->pending_frame)
    );
    recorder->pending_frame_time_nanoseconds = sample_time_nanoseconds;
    return true;
}

static void *pdsim_record_frames(void *context) {
    pdsim_gif_recorder *recorder = context;
    pthread_setname_np("playdate-simctl-gif");

    uint64_t next_sample_time_nanoseconds;
    if (!pdsim_monotonic_time(&next_sample_time_nanoseconds)) {
        recorder->timing_failed = true;
        return NULL;
    }

    do {
        if (!recorder->capture_frame(
                recorder->sample_frame,
                recorder->capture_context
            )) {
            recorder->capture_failed = true;
            if (!pdsim_monotonic_time(
                    &recorder->capture_failure_time_nanoseconds
                )) {
                recorder->timing_failed = true;
            }
            break;
        }

        uint64_t sample_time_nanoseconds;
        if (!pdsim_monotonic_time(&sample_time_nanoseconds)) {
            recorder->timing_failed = true;
            break;
        }
        if (
            atomic_load_explicit(
                &recorder->stop_requested,
                memory_order_acquire
            ) && sample_time_nanoseconds > recorder->stop_time_nanoseconds
        ) {
            sample_time_nanoseconds = recorder->stop_time_nanoseconds;
        }
        if (!pdsim_process_sample(recorder, sample_time_nanoseconds)) {
            recorder->encoding_failed = true;
            break;
        }

        next_sample_time_nanoseconds += PDSIM_GIF_SAMPLE_INTERVAL_NANOSECONDS;
        if (next_sample_time_nanoseconds <= sample_time_nanoseconds) {
            uint64_t missed_intervals =
                (sample_time_nanoseconds - next_sample_time_nanoseconds)
                    / PDSIM_GIF_SAMPLE_INTERVAL_NANOSECONDS
                + 1;
            next_sample_time_nanoseconds += missed_intervals
                * PDSIM_GIF_SAMPLE_INTERVAL_NANOSECONDS;
        }
        if (!pdsim_sleep_until(
                next_sample_time_nanoseconds,
                &recorder->stop_requested
            )) {
            recorder->timing_failed = true;
            break;
        }
    } while (!atomic_load_explicit(
        &recorder->stop_requested,
        memory_order_acquire
    ));

    return NULL;
}

static void pdsim_destroy_recorder(pdsim_gif_recorder *recorder) {
    pdsim_gif_encoder_destroy(recorder->encoder);
    free(recorder);
}

bool pdsim_gif_recorder_start(
    const char *path,
    pdsim_capture_frame_function capture_frame,
    void *capture_context,
    char *error_message,
    size_t error_message_capacity
) {
    if (path == NULL || path[0] != '/' || capture_frame == NULL) {
        pdsim_set_error(error_message, error_message_capacity, "invalid GIF recording input");
        return false;
    }
    if (pdsim_active_recorder != NULL) {
        pdsim_set_error(error_message, error_message_capacity, "a GIF recording is already active");
        return false;
    }

    struct stat output_status;
    if (lstat(path, &output_status) == 0) {
        pdsim_set_error(error_message, error_message_capacity, "GIF output already exists");
        return false;
    }
    if (errno != ENOENT) {
        pdsim_set_errno_error(
            error_message,
            error_message_capacity,
            "could not inspect GIF output",
            errno
        );
        return false;
    }

    pdsim_gif_recorder *recorder = calloc(1, sizeof(*recorder));
    if (recorder == NULL) {
        pdsim_set_error(error_message, error_message_capacity, "could not allocate GIF recorder");
        return false;
    }

    int output_length = snprintf(recorder->output_path, sizeof(recorder->output_path), "%s", path);
    if (output_length < 0 || (size_t)output_length >= sizeof(recorder->output_path)) {
        pdsim_destroy_recorder(recorder);
        pdsim_set_error(error_message, error_message_capacity, "GIF output path is too long");
        return false;
    }

    int temporary_descriptor = pdsim_create_recording_temporary_file(
        recorder->temporary_path,
        sizeof(recorder->temporary_path)
    );
    if (temporary_descriptor < 0) {
        int creation_error = errno;
        pdsim_destroy_recorder(recorder);
        pdsim_set_errno_error(
            error_message,
            error_message_capacity,
            "could not create GIF temporary file",
            creation_error
        );
        return false;
    }

    recorder->encoder = pdsim_gif_encoder_create(
        temporary_descriptor,
        PDSIM_SCREEN_WIDTH,
        PDSIM_SCREEN_HEIGHT
    );
    if (recorder->encoder == NULL) {
        unlink(recorder->temporary_path);
        pdsim_destroy_recorder(recorder);
        pdsim_set_error(error_message, error_message_capacity, "could not initialize GIF encoder");
        return false;
    }

    recorder->capture_frame = capture_frame;
    recorder->capture_context = capture_context;
    atomic_init(&recorder->stop_requested, false);
    int creation_result = pthread_create(
        &recorder->thread,
        NULL,
        pdsim_record_frames,
        recorder
    );
    if (creation_result != 0) {
        unlink(recorder->temporary_path);
        pdsim_destroy_recorder(recorder);
        pdsim_set_errno_error(
            error_message,
            error_message_capacity,
            "could not start GIF recorder thread",
            creation_result
        );
        return false;
    }

    pdsim_active_recorder = recorder;
    return true;
}

bool pdsim_gif_recorder_stop(
    char *error_message,
    size_t error_message_capacity
) {
    pdsim_gif_recorder *recorder = pdsim_active_recorder;
    if (recorder == NULL) {
        pdsim_set_error(error_message, error_message_capacity, "no GIF recording is active");
        return false;
    }

    if (!pdsim_monotonic_time(&recorder->stop_time_nanoseconds)) {
        pdsim_set_error(error_message, error_message_capacity, "could not read GIF recording time");
        return false;
    }
    atomic_store_explicit(
        &recorder->stop_requested,
        true,
        memory_order_release
    );

    int join_result = pthread_join(recorder->thread, NULL);
    if (join_result != 0) {
        pdsim_set_errno_error(
            error_message,
            error_message_capacity,
            "could not stop GIF recorder thread",
            join_result
        );
        return false;
    }

    uint64_t final_frame_time_nanoseconds = recorder->capture_failed
            && recorder->capture_failure_time_nanoseconds != 0
        ? recorder->capture_failure_time_nanoseconds
        : recorder->stop_time_nanoseconds;
    bool encoded = !recorder->encoding_failed && recorder->has_pending_frame
        && pdsim_emit_pending_frame(recorder, final_frame_time_nanoseconds);
    bool finished = encoded && pdsim_gif_encoder_finish(recorder->encoder);
    int save_error = 0;
    bool saved = finished
        && pdsim_publish_capture_temporary_file(
            recorder->temporary_path,
            recorder->output_path,
            &save_error
        );
    if (!finished) {
        (void)unlink(recorder->temporary_path);
    }
    pdsim_active_recorder = NULL;

    bool capture_failed = recorder->capture_failed;
    bool timing_failed = recorder->timing_failed;
    if (!encoded || !finished) {
        pdsim_destroy_recorder(recorder);
        pdsim_set_error(error_message, error_message_capacity, "could not encode GIF recording");
        return false;
    }
    if (!saved) {
        if (error_message != NULL && error_message_capacity > 0) {
            snprintf(
                error_message,
                error_message_capacity,
                "could not save GIF recording; temporary file preserved at %s: %s",
                recorder->temporary_path,
                strerror(save_error)
            );
        }
        pdsim_destroy_recorder(recorder);
        return false;
    }
    pdsim_destroy_recorder(recorder);
    if (capture_failed) {
        pdsim_set_error(
            error_message,
            error_message_capacity,
            "framebuffer capture failed; captured GIF frames were saved"
        );
        return false;
    }
    if (timing_failed) {
        pdsim_set_error(
            error_message,
            error_message_capacity,
            "recording clock failed; captured GIF frames were saved"
        );
        return false;
    }
    return true;
}
