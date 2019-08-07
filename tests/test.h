#ifndef SOS_TEST_H
#define SOS_TEST_H

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "sos.h"

/*
 *  Should work fine in most linux terminals.
 *    Ex:   printf(colorRed "Red text!" colorNormal "\n");
 */

#define colorNormal  "\x1B[0m"
#define colorRed     "\x1B[31m"
#define colorGreen   "\x1B[32m"
#define colorYellow  "\x1B[33m"
#define colorBlue    "\x1B[34m"
#define colorMagenta "\x1B[35m"
#define colorCyan    "\x1B[36m"
#define colorWhite   "\x1B[37m"

#define INDENT 4

#define PASS   0
#define FAIL   1
#define NOTEST 2

extern int SOS_RUN_MODE;

extern void random_string(char *dest_str, size_t size);
extern void random_double(double *dest_dbl);

extern SOS_runtime *TEST_sos;

#define SOS_test_result(level, module_name, pass_fail);         \
    {                                                           \
        int indent_spaces; indent_spaces = (level * INDENT);    \
        while ( indent_spaces-- ) { printf(" "); }              \
        if (pass_fail == PASS) {                                \
            printf("[  " colorGreen "OK" colorNormal "  ]");    \
        } else if (pass_fail == FAIL) {                         \
            printf("[ " colorRed "FAIL" colorNormal " ]");      \
        } else {                                                \
            printf("[" colorYellow "NOTEST" colorNormal "]");   \
        }                                                       \
        printf(" : %s\n", module_name);                         \
     }


#define SOS_test_run(level, title, TEST_FUNCTION, errvar, errtot);      \
    {                                                                   \
        int indent_spaces; indent_spaces = (level * INDENT);            \
        while ( indent_spaces-- ) { printf(" "); }                      \
        printf("[" colorBlue " wait " colorNormal "]");                 \
        printf(" : %s (testing) ", title); fflush(stdout);              \
        errvar = 0; errvar = TEST_FUNCTION;                             \
        if (errvar == FAIL) { errtot += 1; }                            \
        indent_spaces = 0;                                              \
        indent_spaces += strlen(title);                                 \
        indent_spaces += (level * 2) + 30;                              \
        while ( indent_spaces-- ) { printf(colorNormal "\b \b"); }      \
        SOS_test_result(level, title, errvar);                          \
    }


#define SOS_test_section_start(level, section_name);                    \
    {                                                                   \
        int indent_spaces; indent_spaces = (level * INDENT);            \
        while ( indent_spaces-- ) { printf(" "); }                      \
        printf("[" colorCyan ">>>>>>" colorNormal "]");                 \
        printf(" " colorYellow "%s" colorNormal "\n", section_name);    \
    }


#define SOS_test_section_report(level, section_name, err_count);        \
    {                                                                   \
        int indent_spaces; indent_spaces = (level * INDENT);            \
        while ( indent_spaces-- ) { printf(" "); }                      \
        if (err_count > 0) {                                            \
            printf("[" colorRed "######" colorNormal "]");              \
        } else {                                                        \
            printf("[" colorCyan "<<<<<<" colorNormal "]");             \
        }                                                               \
        printf(" " colorYellow "%s" colorNormal, section_name);         \
        if (err_count > 0) {                                            \
            printf(" -- [" colorRed "%d" colorNormal "]\n", err_count); \
        } else {                                                        \
            printf("\n");                                               \
        }                                                               \
    }

#endif
