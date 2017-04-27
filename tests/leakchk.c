#include <mcheck.h>
static void prepare(void) __attribute__((constructor));
static void prepare(void)
{
        mtrace();
}
