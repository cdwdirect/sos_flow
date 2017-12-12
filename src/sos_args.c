

//============================================================================
// Author       : Triston J. Taylor (pc.wiz.tt@gmail.com)
// Version      : 2.0
// Copyright    : (C) Triston J. Taylor 2012. All Rights Reserved
//============================================================================

/* License:   (Found at: https://github.com/hypersoft/nargv/blob/master/LICENSE.md)
 * OPENWARE
 *
 * A license is permission to do an act, which would otherwise be illegal, a tresspass or a tort
 *
 * Free to use, free to distribute, free to license, free to sell.
 *
 * FOR ANY SOFTWARE WITH THE LACK OF THE LICENSING IS WITH YOUR FREEDOM FOR THE TAKING AND THE USING, FOR THE LACK OF THE CLAIMS
 * OF THE TRESPASSING OR TORTS.
 *
 * A workman is worhty of his/her hire. If you pay for the product, then you have the rights to recovery for the losses.
 *
 * Please use this code to better the versatility of your applications for the benefit of all who may become a user of your
 * software.
 *
 * If you find any bugs, or have betterments to offer, please do send me a pull request.
 */


#include <stddef.h>
#include <stdlib.h>

const char *SOS_NARGV_IFS = " \t\n";

typedef struct SOS_NARGV {
    char **argv, *data, *error_message;
    int argc, data_length, error_index, error_code;
} SOS_NARGV;

void SOS_nargv_free(SOS_NARGV* props) {
    free(props->data); free(props->argv);
    free(props);
}

void SOS_nargv_ifs(const char *nifs) {
    if (! nifs) {
        SOS_NARGV_IFS = " \t\n";
    } else {
        SOS_NARGV_IFS = nifs;
    }
}

int SOS_nargv_field_seperator(char seperator) {
    const char *list = SOS_NARGV_IFS;
    if (seperator) {
        while (*list) {
            if (*list++ == seperator) return 1;
        }
        return 0;
    }
    return 1; // null is always a field seperator
}

SOS_NARGV *nargv_parse(const char *input) {

    SOS_NARGV *nvp = calloc(1, sizeof(SOS_NARGV));

    if (! input) {
        nvp->error_code = 1;
        nvp->error_message = "cannot parse null pointer";
        return nvp;
    }

    /* Get the input length */
    long input_length = -1;
    test_next_input_char:
    if (input[++input_length]) goto test_next_input_char;

    if (! input_length) {
        nvp->error_code = 2;
        nvp->error_message = "cannot parse empty input";
        return nvp;
    }

    int composing_argument = 0;
    long quote = 0;
    long index;
    char look_ahead;

    // FIRST PASS
    // discover how many elements we have, and discover how large a data buffer we need
    for (index = 0; index <= input_length; index++) {

        if (nargv_field_seperator(input[index])) {
            if (composing_argument) {
                // close the argument
                composing_argument = 0;
                nvp->data_length++;
            }
            continue;
        }

        if (! composing_argument) {
            nvp->argc++;
            composing_argument = 1;
        }

        switch (input[index]) {

            /* back slash */
            case '\\':
                // If the sequence is not \' or \" or seperator copy the back slash, and
                // the data
                look_ahead = *(input+index+1);
                if (look_ahead == '"' || look_ahead == '\'' || nargv_field_seperator(look_ahead)) {
                    index++;
                } else {
                    index++;
                    nvp->data_length++;
                }
            break;

            /* double quote */
            case '"':
                quote = index;
                while (input[++index] != '"') {
                    switch (input[index]) {
                        case '\0':
                            nvp->error_index = quote + 1;
                            nvp->error_code = 3;
                            nvp->error_message = "unterminated double quote";
                            return nvp;
                        break;
                        case '\\':
                            look_ahead = *(input + index + 1);
                            if (look_ahead == '"') {
                                index++;
                            } else {
                                index++;
                                nvp->data_length++;
                            }
                        break;
                    }
                    nvp->data_length++;
                }

                continue;
            break;

            /* single quote */
            case '\'':
                /* copy single quoted data */
                quote = index; // QT used as temp here...
                while (input[++index] != '\'') {
                    if (! input[index]) {
                        // unterminated double quote @ input
                        nvp->error_index = quote + 1;
                        nvp->error_code = 4;
                        nvp->error_message = "unterminated single quote";
                        return nvp;
                    }
                    nvp->data_length++;
                }
                continue;
            break;

        }

        // "record" the data
        nvp->data_length++;

    }

    // +1 for extra NULL pointer required by execv() and friends
    nvp->argv = calloc(nvp->argc + 1, sizeof(char *));
    nvp->data = calloc(nvp->data_length, 1);

    // SECOND PASS
    composing_argument = 0;
    quote = 0;

    int data_index = 0;
    int arg_index = 0;

    for (index = 0; index <= input_length; index++) {

        if (nargv_field_seperator(input[index])) {
            if (composing_argument) {
                composing_argument = 0;
                nvp->data[data_index++] = '\0';
            }
            continue;
        }

        if (! composing_argument) {
            nvp->argv[arg_index++] = (nvp->data + data_index);
            composing_argument = 1;
        }

        switch (input[index]) {

            /* back slash */
            case '\\':
                // If the sequence is not \' or \" or field seperator copy the backslash
                look_ahead = *(input+index+1);
                if (look_ahead == '"' || look_ahead == '\'' || nargv_field_seperator(look_ahead)) {
                    index++;
                } else {
                    nvp->data[data_index++] = input[index++];
                }
            break;

            /* double quote */
            case '"':
                while (input[++index] != '"') {
                    if (input[index] == '\\') {
                        look_ahead = *(input + index + 1);
                        if (look_ahead == '"') {
                            index++;
                        } else {
                            nvp->data[data_index++] = input[index++];
                        }
                    }
                    nvp->data[data_index++] = input[index];
                }
                continue;
            break;

            /* single quote */
            case '\'':
                /* copy single quoted data */
                while (input[++index] != '\'') {
                    nvp->data[data_index++] = input[index];
                }
                continue;
            break;

        }

        // "record" the data
        nvp->data[data_index++] = input[index];

    }

    return nvp;
}

#ifdef SOS_NARGVTEST
#include <stdio.h>
int main(int argc, char *argv[]) {

    int arg;
    char line_in[4096];

    while (fgets(line_in, 4096, stdin) == line_in) {
        SOS_NARGV *nargv = nargv_parse(line_in);
        if (nargv->error_code) {
            printf("\nSOS_NARGV parse error: %i: %s: at input column %i\n",
            nargv->error_code, nargv->error_message, nargv->error_index);
        } else {
            printf("\nSOS_NARGV Argument Count: %i\n", nargv->argc);
            printf("SOS_NARGV Data Length: %i\n\n", nargv->data_length);
            for (arg = 0; arg < nargv->argc; arg++) {
                printf("argument %i: %s\n", arg, nargv->argv[arg]);
            }
        }
        nargv_free(nargv);
    }

}
#endif
