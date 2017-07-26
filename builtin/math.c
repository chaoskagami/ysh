// Do note that these builtin commands, implementation-wise, accept more than
// two arguments; make sure to carefully consider inputs, since for example:
//   / 10 2 2
// is equivalent to 10 / 2 / 2.
// This is similar to scheme/lisp dialects.
// Additionally, these will parse octal (0) and hexadecimal (0x) when
// provided; this is desirable behavior to me but it may not be to you.
// Make sure you don't have leading zeroes, or strip them beforehand.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "parse.h"
#include "util.h"

int builtin_add(char* nam, char** argv, char** stdout) {
    assert(nam);
    assert(argv[0]);

    long long int total = 0;

    for(size_t idx = 1; argv[idx] != 0; idx++) {
        long long int ret = strtoll(argv[idx], NULL, 0);
        if ((ret == LLONG_MIN || ret == LLONG_MAX) && errno) {
            perror("builtin: add");
            return -1;
        }
        total += ret;
    }

    char* str = malloc_trap(50); // This is the max possible length of a LLONG, plus nul term
    memset(str, 0, 50);

    switch(nam[0]) {
        case 'x':
            snprintf(str, 50, "0x%llx", total);
            break;
        case 'X':
            snprintf(str, 50, "0x%llX", total);
            break;
        case 'o':
            snprintf(str, 50, "0%llo", total);
            break;
        default:
            snprintf(str, 50, "%lld", total);
            break;
    }

    if (stdout) {
        *stdout = str;
    } else {
        printf("%s\n", str);
    }

    return 0;
}

int builtin_sub(char* nam, char** argv, char** stdout) {
    assert(nam);
    assert(argv[0]);

    long long int total = 0;

    for(size_t idx = 1; argv[idx] != 0; idx++) {
        long long int ret = strtoll(argv[idx], NULL, 0);
        if ((ret == LLONG_MIN || ret == LLONG_MAX) && errno) {
            perror("builtin: add");
            return -1;
        }
        if (idx == 1) {
            total = ret;
        } else {
            total -= ret;
        }
    }

    char* str = malloc_trap(50); // This is the max possible length of a LLONG, plus nul term
    memset(str, 0, 50);

    switch(nam[0]) {
        case 'x':
            snprintf(str, 50, "0x%llx", total);
            break;
        case 'X':
            snprintf(str, 50, "0x%llX", total);
            break;
        case 'o':
            snprintf(str, 50, "0%llo", total);
            break;
        default:
            snprintf(str, 50, "%lld", total);
            break;
    }

    if (stdout) {
        *stdout = str;
    } else {
        printf("%s\n", str);
    }

    return 0;
}

int builtin_mul(char* nam, char** argv, char** stdout) {
    assert(nam);
    assert(argv[0]);

    long long int total = 1;

    for(size_t idx = 1; argv[idx] != 0; idx++) {
        long long int ret = strtoll(argv[idx], NULL, 0);
        if ((ret == LLONG_MIN || ret == LLONG_MAX) && errno) {
            perror("builtin: add");
            return -1;
        }
        total *= ret;
    }

    char* str = malloc_trap(50); // This is the max possible length of a LLONG, plus nul term
    memset(str, 0, 50);

    switch(nam[0]) {
        case 'x':
            snprintf(str, 50, "0x%llx", total);
            break;
        case 'X':
            snprintf(str, 50, "0x%llX", total);
            break;
        case 'o':
            snprintf(str, 50, "0%llo", total);
            break;
        default:
            snprintf(str, 50, "%lld", total);
            break;
    }

    if (stdout) {
        *stdout = str;
    } else {
        printf("%s\n", str);
    }

    return 0;
}

int builtin_div(char* nam, char** argv, char** stdout) {
    assert(nam);
    assert(argv[0]);

    long long int total = 0;

    for(size_t idx = 1; argv[idx] != 0; idx++) {
        long long int ret = strtoll(argv[idx], NULL, 0);
        if ((ret == LLONG_MIN || ret == LLONG_MAX) && errno) {
            perror("builtin: add");
            return -1;
        }
        if (idx == 1) {
            total = ret;
        } else {
            total /= ret;
        }
    }

    char* str = malloc_trap(50); // This is the max possible length of a LLONG, plus nul term
    memset(str, 0, 50);

    switch(nam[0]) {
        case 'x':
            snprintf(str, 50, "0x%llx", total);
            break;
        case 'X':
            snprintf(str, 50, "0x%llX", total);
            break;
        case 'o':
            snprintf(str, 50, "0%llo", total);
            break;
        default:
            snprintf(str, 50, "%lld", total);
            break;
    }

    if (stdout) {
        *stdout = str;
    } else {
        printf("%s\n", str);
    }

    return 0;
}

int builtin_modulo(char* nam, char** argv, char** stdout) {
    assert(nam);
    assert(argv[0]);

    long long int total = 0;

    for(size_t idx = 1; argv[idx] != 0; idx++) {
        long long int ret = strtoll(argv[idx], NULL, 0);
        if ((ret == LLONG_MIN || ret == LLONG_MAX) && errno) {
            perror("builtin: add");
            return -1;
        }
        if (idx == 1) {
            total = ret;
        } else {
            total %= ret;
        }
    }

    char* str = malloc_trap(50); // This is the max possible length of a LLONG, plus nul term
    memset(str, 0, 50);

    switch(nam[0]) {
        case 'x':
            snprintf(str, 50, "0x%llx", total);
            break;
        case 'X':
            snprintf(str, 50, "0x%llX", total);
            break;
        case 'o':
            snprintf(str, 50, "0%llo", total);
            break;
        default:
            snprintf(str, 50, "%lld", total);
            break;
    }

    if (stdout) {
        *stdout = str;
    } else {
        printf("%s\n", str);
    }

    return 0;
}
