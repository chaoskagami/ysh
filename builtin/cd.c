#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int builtin_chdir(char* nam, char** argv, char** stdout) {
    assert(nam);
    assert(argv[0]);

    int ret = 0;

    if (argv[1] == NULL) {
        // No path was provided in cd, so just go $HOME
    } else if (argv[1]) {
        ret = chdir(argv[1]);
    }

    if (ret == -1)
        perror("cd");

    return ret;
}
