#include <stddef.h>

typedef struct SOS_NARGV {
    char **argv, *data, *error_message;
    int argc, data_length, error_index, error_code;
} SOS_NARGV;

void SOS_nargv_free(NARGV* props);
void SOS_nargv_ifs(const char *nifs);
NARGV *SOS_nargv_parse(const char *input);
