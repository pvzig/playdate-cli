#include "ButtonInputState.h"

#include "PlaydateSimulatorProtocol.h"

#include <stdatomic.h>

#define PDSIM_BUTTON_COUNT 7

static atomic_uint_fast64_t pdsim_button_generations[PDSIM_BUTTON_COUNT];

static int pdsim_button_index(int button) {
    switch (button) {
    case PDSIM_BUTTON_LEFT_MASK:
        return 0;
    case PDSIM_BUTTON_RIGHT_MASK:
        return 1;
    case PDSIM_BUTTON_UP_MASK:
        return 2;
    case PDSIM_BUTTON_DOWN_MASK:
        return 3;
    case PDSIM_BUTTON_B_MASK:
        return 4;
    case PDSIM_BUTTON_A_MASK:
        return 5;
    case PDSIM_BUTTON_MENU_MASK:
        return 6;
    default:
        return -1;
    }
}

uint64_t pdsim_begin_button_input(int button) {
    int index = pdsim_button_index(button);
    if (index < 0) {
        return 0;
    }
    return atomic_fetch_add_explicit(
               &pdsim_button_generations[index],
               1,
               memory_order_acq_rel
           )
        + 1;
}

bool pdsim_is_current_button_input(int button, uint64_t generation) {
    int index = pdsim_button_index(button);
    return index >= 0
        && atomic_load_explicit(
               &pdsim_button_generations[index],
               memory_order_acquire
           )
            == generation;
}
