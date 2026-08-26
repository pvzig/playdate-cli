#include "MachOSymbolResolver.h"

#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

void *pdsim_find_main_image_symbol(const char *symbol_name) {
    if (symbol_name == NULL) {
        return NULL;
    }

    const struct mach_header *header = _dyld_get_image_header(0);
    if (header == NULL || header->magic != MH_MAGIC_64) {
        return NULL;
    }

    const struct mach_header_64 *header_64 = (const struct mach_header_64 *)header;
    const struct symtab_command *symbol_table_command = NULL;
    const struct segment_command_64 *link_edit_segment = NULL;
    const uint8_t *command_bytes = (const uint8_t *)(header_64 + 1);

    for (uint32_t index = 0; index < header_64->ncmds; index++) {
        const struct load_command *command = (const struct load_command *)command_bytes;
        if (command->cmdsize < sizeof(struct load_command)) {
            return NULL;
        }

        if (command->cmd == LC_SYMTAB) {
            symbol_table_command = (const struct symtab_command *)command;
        } else if (command->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *segment =
                (const struct segment_command_64 *)command;
            if (strncmp(segment->segname, SEG_LINKEDIT, sizeof(segment->segname)) == 0) {
                link_edit_segment = segment;
            }
        }

        command_bytes += command->cmdsize;
    }

    if (symbol_table_command == NULL || link_edit_segment == NULL) {
        return NULL;
    }

    intptr_t slide = _dyld_get_image_vmaddr_slide(0);
    uintptr_t link_edit_base =
        (uintptr_t)(slide + (intptr_t)link_edit_segment->vmaddr -
                    (intptr_t)link_edit_segment->fileoff);
    const struct nlist_64 *symbols =
        (const struct nlist_64 *)(link_edit_base + symbol_table_command->symoff);
    const char *strings =
        (const char *)(link_edit_base + symbol_table_command->stroff);

    for (uint32_t index = 0; index < symbol_table_command->nsyms; index++) {
        const struct nlist_64 *symbol = &symbols[index];
        if ((symbol->n_type & N_STAB) != 0 || symbol->n_un.n_strx >= symbol_table_command->strsize) {
            continue;
        }

        const char *candidate = strings + symbol->n_un.n_strx;
        size_t remaining = symbol_table_command->strsize - symbol->n_un.n_strx;
        if (memchr(candidate, '\0', remaining) == NULL) {
            continue;
        }
        if (strcmp(candidate, symbol_name) == 0) {
            return (void *)(uintptr_t)(slide + (intptr_t)symbol->n_value);
        }
    }

    return NULL;
}
