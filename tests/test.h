
#include <stdio.h>
#include <unistd.h>
#include <ctypes.h>
#include <string.h>

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

#define PASS   0
#define FAIL   1

extern int SOS_TEST_RUN_SILENT;


#define SOS_test_result(level, module_name, pass_fail);         \
    {                                                           \
        int indent_spaces; indent_spaces = (level * 2);         \
        while ( indent_spaces-- ) { printf(" "); }              \
        if (pass_fail == PASS) {                                \
            printf("[  " colorGreen "OK" colorNormal "  ]");    \
        else {                                                  \
            printf("[ " colorRed "FAIL" colorNormal " ]");      \
        }                                                       \
        printf(" : %s\n", module_name);                         \
     }


#define SOS_test_section_start(level, section_name);                    \
    {                                                                   \
        int indent_spaces; indent_spaces = (level * 2);                 \
        while ( indent_spaces-- ) { printf(" "); }                      \
        printf("[" colorWhite ">>>>>>" colorNormal "]");                \
        printf(" : Checking %s ...\n",                                  \
            err_count, ((err_count != 1) ? "s " : " "), section_name);  \
    }


#define SOS_test_section_report(level, section_name, err_count);        \
    {                                                                   \
        int indent_spaces; indent_spaces = (level * 2);                 \
        while ( indent_spaces-- ) { printf(" "); }                      \
        printf("[" colorWhite "<<<<<<" colorNormal "]");                \
        printf(" :   %d error%sdetected in %s.\n",                      \
            err_count, ((err_count != 1) ? "s " : " "), section_name);  \
    }

