#include "core/cycle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *config = "ngnix.conf";
    if (argc == 3 && strcmp(argv[1], "-c") == 0) config = argv[2];
    else if (argc != 1) {
        fprintf(stderr, "usage: %s [-c config-file]\n", argv[0]);
        return EXIT_FAILURE;
    }
    return ng_cycle_run(config) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
