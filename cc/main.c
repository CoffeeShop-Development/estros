#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <ctype.h>
#include <stdbool.h>
#include <limits.h>

#include "util.h"
#include "parser.h"
#include "state.h"
#include "ssa.h"
#include "asm.h"

typedef struct cc_driver_state { 
    /* State for parsing and processing */
    cc_state_t state;
    /* Open file descriptor of the source file */
    int fd;
} cc_driver_state_t;

static bool cc_driver_open_file(cc_driver_state_t *driver, const char *path) {
    if ((driver->fd = open(path, O_RDONLY)) > 0) {
        struct stat st;
        fstat(driver->fd, &st);
        if ((driver->state.data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, driver->fd, 0)) != MAP_FAILED) {
            driver->state.n_data = st.st_size;
            return true;
        }
    }
    return false;
}

static void cc_driver_close_file(cc_driver_state_t *driver) {
    munmap(driver->state.data, driver->state.n_data);
    close(driver->fd);
}

static void cc_driver_parse_file(cc_driver_state_t *driver) {
    cc_state_t *state = &driver->state;
    DYNARR_INIT(*state, tokens);
    DYNARR_INIT(*state, nodes);
    DYNARR_INIT(*state, ssa_funcs);
    DYNARR_INIT(*state, ssa_tokens);
    DYNARR_INIT(*state, raw_data);

    cc_tokenize(state);
    cc_dump_tokens(state);

    cc_parse_translation_unit(state);

    state->ssa_funcs[0].name.pos = 0;
    state->ssa_funcs[0].name.len = 3;
    state->ssa_funcs[0].tokens.pos = 0;
    state->ssa_funcs[0].tokens.len = 3;
    state->n_ssa_funcs = 1;

    /* %1 = <raw data> */
    state->ssa_tokens[0].type = CC_SSA_TOK_RAW_DATA;
    state->ssa_tokens[0].result.id = 1;
    state->ssa_tokens[0].result.width = CC_SSA_WIDTH_SET(32);
    state->ssa_tokens[0].data.raw = cc_push_raw_data_u32(state, 1);
    /* %2 = <raw data> */
    state->ssa_tokens[1].type = CC_SSA_TOK_RAW_DATA;
    state->ssa_tokens[1].result.id = 2;
    state->ssa_tokens[1].result.width = CC_SSA_WIDTH_SET(32);
    state->ssa_tokens[1].data.raw = cc_push_raw_data_u32(state, 2);
    /* %3 = %1 add %2 */
    state->ssa_tokens[2].type = CC_SSA_TOK_ADD;
    state->ssa_tokens[2].result.id = 3;
    state->ssa_tokens[2].result.width = CC_SSA_WIDTH_SET(32);
    state->ssa_tokens[2].data.args[0].id = 1;
    state->ssa_tokens[2].data.args[0].width = CC_SSA_WIDTH_SET(32);
    state->ssa_tokens[2].data.args[1].id = 2;
    state->ssa_tokens[2].data.args[1].width = CC_SSA_WIDTH_SET(32);
    state->n_ssa_tokens = 3;

    cc_asm_lower_ssa(state);

    DYNARR_FINISH(*state, tokens);
    DYNARR_FINISH(*state, nodes);
    DYNARR_FINISH(*state, ssa_funcs);
    DYNARR_FINISH(*state, ssa_tokens);
    DYNARR_FINISH(*state, raw_data);
}

int main(int argc, char *argv[]) {
    static cc_driver_state_t driver;
    if (cc_driver_open_file(&driver, argv[1])) {
        cc_driver_parse_file(&driver);
        cc_driver_close_file(&driver);
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
