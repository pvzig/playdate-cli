#ifndef BUTTON_INPUT_STATE_H
#define BUTTON_INPUT_STATE_H

#include <stdbool.h>
#include <stdint.h>

// Thread-safe. Call from the main-queue action that applies an intentional
// input so generation order matches Simulator mutation order. Invalidates any
// delayed release associated with an older generation of that button.
uint64_t pdsim_begin_button_input(int button);

bool pdsim_is_current_button_input(int button, uint64_t generation);

#endif
