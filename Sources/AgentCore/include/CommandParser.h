#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

typedef enum {
    pdsim_agent_command_status,
    pdsim_agent_command_button,
    pdsim_agent_command_press,
    pdsim_agent_command_crank,
    pdsim_agent_command_crank_docked,
    pdsim_agent_command_accelerometer,
    pdsim_agent_command_lock,
    pdsim_agent_command_load,
    pdsim_agent_command_set_active_pdx,
    pdsim_agent_command_pause,
    pdsim_agent_command_restart,
    pdsim_agent_command_volume_adjust,
    pdsim_agent_command_volume_set,
    pdsim_agent_command_screenshot,
    pdsim_agent_command_toolbar,
    pdsim_agent_command_record_start,
    pdsim_agent_command_record_stop,
} pdsim_agent_command_kind;

typedef enum {
    pdsim_toolbar_pause,
    pdsim_toolbar_restart,
    pdsim_toolbar_console,
    pdsim_toolbar_sampler,
    pdsim_toolbar_memory,
    pdsim_toolbar_record,
    pdsim_toolbar_device,
    pdsim_toolbar_controls,
} pdsim_toolbar_action;

typedef struct {
    pdsim_agent_command_kind kind;
    int button;
    int integer_value;
    int duration_milliseconds;
    float x;
    float y;
    float z;
    const char *path;
    pdsim_toolbar_action toolbar_action;
} pdsim_agent_command;

typedef enum {
    pdsim_agent_command_parse_success,
    pdsim_agent_command_parse_unknown,
    pdsim_agent_command_parse_invalid_button,
    pdsim_agent_command_parse_invalid_press,
    pdsim_agent_command_parse_invalid_crank,
    pdsim_agent_command_parse_invalid_crank_docked,
    pdsim_agent_command_parse_invalid_accelerometer,
    pdsim_agent_command_parse_invalid_load,
    pdsim_agent_command_parse_invalid_active_pdx,
    pdsim_agent_command_parse_invalid_pause,
    pdsim_agent_command_parse_invalid_volume,
    pdsim_agent_command_parse_invalid_screenshot,
    pdsim_agent_command_parse_invalid_toolbar,
    pdsim_agent_command_parse_invalid_record_start,
} pdsim_agent_command_parse_result;

// On success, path aliases request storage and remains valid only while that
// storage remains valid. Other command values are copied into command.
pdsim_agent_command_parse_result pdsim_parse_agent_command(
    const char *request,
    pdsim_agent_command *command
);

const char *pdsim_agent_command_parse_error(
    pdsim_agent_command_parse_result result
);

#endif
