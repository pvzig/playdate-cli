#include "CommandParser.h"

#include "PlaydateSimulatorProtocol.h"

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <xlocale.h>

static bool pdsim_is_valid_button(int button) {
    switch (button) {
    case PDSIM_BUTTON_LEFT_MASK:
    case PDSIM_BUTTON_RIGHT_MASK:
    case PDSIM_BUTTON_UP_MASK:
    case PDSIM_BUTTON_DOWN_MASK:
    case PDSIM_BUTTON_B_MASK:
    case PDSIM_BUTTON_A_MASK:
    case PDSIM_BUTTON_MENU_MASK:
        return true;
    default:
        return false;
    }
}

static bool pdsim_is_ascii_whitespace(char character) {
    return character == ' ' || character == '\t' || character == '\n'
        || character == '\r' || character == '\f' || character == '\v';
}

static bool pdsim_skip_required_whitespace(const char **cursor) {
    if (!pdsim_is_ascii_whitespace(**cursor)) {
        return false;
    }
    do {
        (*cursor)++;
    } while (pdsim_is_ascii_whitespace(**cursor));
    return true;
}

static bool pdsim_has_only_trailing_whitespace(const char *cursor) {
    while (pdsim_is_ascii_whitespace(*cursor)) {
        cursor++;
    }
    return *cursor == '\0';
}

static bool pdsim_parse_c_locale_float(const char **cursor, float *value) {
    locale_t c_locale = newlocale(LC_NUMERIC_MASK, "C", NULL);
    if (c_locale == NULL) {
        return false;
    }

    errno = 0;
    char *end = NULL;
    float parsed_value = strtof_l(*cursor, &end, c_locale);
    int parse_error = errno;
    freelocale(c_locale);

    if (
        end == *cursor || (parse_error != 0 && parse_error != ERANGE)
        || !isfinite(parsed_value)
    ) {
        return false;
    }
    *cursor = end;
    *value = parsed_value;
    return true;
}

static bool pdsim_parse_int(const char **cursor, int *value) {
    errno = 0;
    char *end = NULL;
    long parsed_value = strtol(*cursor, &end, 10);
    if (
        end == *cursor || errno == ERANGE || parsed_value < INT_MIN
        || parsed_value > INT_MAX
    ) {
        return false;
    }
    *cursor = end;
    *value = (int)parsed_value;
    return true;
}

static bool pdsim_has_suffix(const char *value, const char *suffix) {
    size_t value_length = strlen(value);
    size_t suffix_length = strlen(suffix);
    return value_length >= suffix_length
        && strcasecmp(value + value_length - suffix_length, suffix) == 0;
}

static bool pdsim_is_absolute_pdx_path(const char *path) {
    return path[0] == '/' && pdsim_has_suffix(path, ".pdx");
}

static bool pdsim_parse_toolbar_action(
    const char *name,
    pdsim_toolbar_action *action
) {
    if (strcmp(name, "pause") == 0) {
        *action = pdsim_toolbar_pause;
    } else if (strcmp(name, "restart") == 0) {
        *action = pdsim_toolbar_restart;
    } else if (strcmp(name, "console") == 0) {
        *action = pdsim_toolbar_console;
    } else if (strcmp(name, "sampler") == 0) {
        *action = pdsim_toolbar_sampler;
    } else if (strcmp(name, "lua-memory") == 0) {
        *action = pdsim_toolbar_memory;
    } else if (strcmp(name, "gif") == 0) {
        *action = pdsim_toolbar_record;
    } else if (strcmp(name, "device") == 0) {
        *action = pdsim_toolbar_device;
    } else if (strcmp(name, "controls") == 0) {
        *action = pdsim_toolbar_controls;
    } else {
        return false;
    }
    return true;
}

pdsim_agent_command_parse_result pdsim_parse_agent_command(
    const char *request,
    pdsim_agent_command *command
) {
    if (request == NULL || command == NULL) {
        return pdsim_agent_command_parse_unknown;
    }
    *command = (pdsim_agent_command){0};

    if (strcmp(request, "status") == 0) {
        command->kind = pdsim_agent_command_status;
        return pdsim_agent_command_parse_success;
    }
    if (strncmp(request, "button ", 7) == 0) {
        const char *cursor = request + 7;
        int is_down = 0;
        if (
            !pdsim_parse_int(&cursor, &command->button)
            || !pdsim_skip_required_whitespace(&cursor)
            || !pdsim_parse_int(&cursor, &is_down)
            || !pdsim_has_only_trailing_whitespace(cursor)
            || !pdsim_is_valid_button(command->button)
            || (is_down != 0 && is_down != 1)
        ) {
            return pdsim_agent_command_parse_invalid_button;
        }
        command->kind = pdsim_agent_command_button;
        command->integer_value = is_down;
        return pdsim_agent_command_parse_success;
    }
    if (strncmp(request, "press ", 6) == 0) {
        const char *cursor = request + 6;
        if (
            !pdsim_parse_int(&cursor, &command->button)
            || !pdsim_skip_required_whitespace(&cursor)
            || !pdsim_parse_int(&cursor, &command->duration_milliseconds)
            || !pdsim_has_only_trailing_whitespace(cursor)
            || !pdsim_is_valid_button(command->button)
            || command->duration_milliseconds < 1
            || command->duration_milliseconds > PDSIM_MAX_PRESS_MILLISECONDS
        ) {
            return pdsim_agent_command_parse_invalid_press;
        }
        command->kind = pdsim_agent_command_press;
        return pdsim_agent_command_parse_success;
    }
    if (strncmp(request, "crank-docked ", 13) == 0) {
        const char *cursor = request + 13;
        if (
            !pdsim_parse_int(&cursor, &command->integer_value)
            || !pdsim_has_only_trailing_whitespace(cursor)
            || (command->integer_value != 0 && command->integer_value != 1)
        ) {
            return pdsim_agent_command_parse_invalid_crank_docked;
        }
        command->kind = pdsim_agent_command_crank_docked;
        return pdsim_agent_command_parse_success;
    }
    if (strncmp(request, "crank ", 6) == 0) {
        const char *cursor = request + 6;
        if (
            !pdsim_parse_c_locale_float(&cursor, &command->x)
            || !pdsim_has_only_trailing_whitespace(cursor) || command->x < 0
            || command->x > 360
        ) {
            return pdsim_agent_command_parse_invalid_crank;
        }
        command->kind = pdsim_agent_command_crank;
        return pdsim_agent_command_parse_success;
    }
    if (strncmp(request, "accelerometer ", 14) == 0) {
        const char *cursor = request + 14;
        if (
            !pdsim_parse_c_locale_float(&cursor, &command->x)
            || !pdsim_skip_required_whitespace(&cursor)
            || !pdsim_parse_c_locale_float(&cursor, &command->y)
            || !pdsim_skip_required_whitespace(&cursor)
            || !pdsim_parse_c_locale_float(&cursor, &command->z)
            || !pdsim_has_only_trailing_whitespace(cursor)
        ) {
            return pdsim_agent_command_parse_invalid_accelerometer;
        }
        command->kind = pdsim_agent_command_accelerometer;
        return pdsim_agent_command_parse_success;
    }
    if (strcmp(request, "lock") == 0) {
        command->kind = pdsim_agent_command_lock;
        return pdsim_agent_command_parse_success;
    }
    if (strncmp(request, "load ", 5) == 0) {
        command->path = request + 5;
        if (!pdsim_is_absolute_pdx_path(command->path)) {
            return pdsim_agent_command_parse_invalid_load;
        }
        command->kind = pdsim_agent_command_load;
        return pdsim_agent_command_parse_success;
    }
    if (strncmp(request, "set-active-pdx ", 15) == 0) {
        command->path = request + 15;
        if (!pdsim_is_absolute_pdx_path(command->path)) {
            return pdsim_agent_command_parse_invalid_active_pdx;
        }
        command->kind = pdsim_agent_command_set_active_pdx;
        return pdsim_agent_command_parse_success;
    }
    if (strncmp(request, "pause ", 6) == 0) {
        const char *cursor = request + 6;
        if (
            !pdsim_parse_int(&cursor, &command->integer_value)
            || !pdsim_has_only_trailing_whitespace(cursor)
            || (command->integer_value != 0 && command->integer_value != 1)
        ) {
            return pdsim_agent_command_parse_invalid_pause;
        }
        command->kind = pdsim_agent_command_pause;
        return pdsim_agent_command_parse_success;
    }
    if (strcmp(request, "restart") == 0) {
        command->kind = pdsim_agent_command_restart;
        return pdsim_agent_command_parse_success;
    }
    if (strncmp(request, "volume-adjust ", 14) == 0) {
        const char *cursor = request + 14;
        if (
            !pdsim_parse_int(&cursor, &command->integer_value)
            || !pdsim_has_only_trailing_whitespace(cursor)
            || command->integer_value < -100 || command->integer_value > 100
            || command->integer_value == 0
        ) {
            return pdsim_agent_command_parse_invalid_volume;
        }
        command->kind = pdsim_agent_command_volume_adjust;
        return pdsim_agent_command_parse_success;
    }
    if (strncmp(request, "volume-set ", 11) == 0) {
        const char *cursor = request + 11;
        if (
            !pdsim_parse_int(&cursor, &command->integer_value)
            || !pdsim_has_only_trailing_whitespace(cursor)
            || command->integer_value < 0 || command->integer_value > 100
        ) {
            return pdsim_agent_command_parse_invalid_volume;
        }
        command->kind = pdsim_agent_command_volume_set;
        return pdsim_agent_command_parse_success;
    }
    if (strncmp(request, "screenshot ", 11) == 0) {
        command->path = request + 11;
        if (command->path[0] != '/') {
            return pdsim_agent_command_parse_invalid_screenshot;
        }
        command->kind = pdsim_agent_command_screenshot;
        return pdsim_agent_command_parse_success;
    }
    if (strncmp(request, "toolbar ", 8) == 0) {
        if (!pdsim_parse_toolbar_action(request + 8, &command->toolbar_action)) {
            return pdsim_agent_command_parse_invalid_toolbar;
        }
        command->kind = pdsim_agent_command_toolbar;
        return pdsim_agent_command_parse_success;
    }
    if (strncmp(request, "record-start ", 13) == 0) {
        command->path = request + 13;
        if (command->path[0] != '/' || !pdsim_has_suffix(command->path, ".gif")) {
            return pdsim_agent_command_parse_invalid_record_start;
        }
        command->kind = pdsim_agent_command_record_start;
        return pdsim_agent_command_parse_success;
    }
    if (strcmp(request, "record-stop") == 0) {
        command->kind = pdsim_agent_command_record_stop;
        return pdsim_agent_command_parse_success;
    }
    return pdsim_agent_command_parse_unknown;
}

const char *pdsim_agent_command_parse_error(
    pdsim_agent_command_parse_result result
) {
    switch (result) {
    case pdsim_agent_command_parse_success:
        return "";
    case pdsim_agent_command_parse_unknown:
        return "error unknown command";
    case pdsim_agent_command_parse_invalid_button:
        return "error invalid button command";
    case pdsim_agent_command_parse_invalid_press:
        return "error invalid press command";
    case pdsim_agent_command_parse_invalid_crank:
        return "error invalid crank command";
    case pdsim_agent_command_parse_invalid_crank_docked:
        return "error invalid crank-docked command";
    case pdsim_agent_command_parse_invalid_accelerometer:
        return "error invalid accelerometer command";
    case pdsim_agent_command_parse_invalid_load:
        return "error load requires an absolute .pdx path";
    case pdsim_agent_command_parse_invalid_active_pdx:
        return "error active PDX requires an absolute .pdx path";
    case pdsim_agent_command_parse_invalid_pause:
        return "error invalid pause command";
    case pdsim_agent_command_parse_invalid_volume:
        return "error invalid volume command";
    case pdsim_agent_command_parse_invalid_screenshot:
        return "error screenshot path must be absolute";
    case pdsim_agent_command_parse_invalid_toolbar:
        return "error invalid toolbar command";
    case pdsim_agent_command_parse_invalid_record_start:
        return "error record start requires an absolute .gif path";
    }

    return "error unknown command";
}
