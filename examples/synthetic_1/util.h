#pragma once

#include <stdbool.h>

#ifdef SOS_HAVE_TAU

/*
 * If we have TAU, then we want to make sure TAU is fully enabled, and
 * we want to disable the SOS initialization and finalization, because
 * they will be handled by TAU.
 */
#define PROFILING_ON
#include <TAU.h>
#define SOS_FINALIZE()                  // do nothing

#else /* SOS_HAVE_TAU not defined */

/* If we aren't using TAU, define some macros so that
 * we don't have to sprinkle ifdefs in the code.
 */
#define TAU_USER 0
#define TAU_DEFAULT 0
#define TAU_INIT(...)                   // do nothing
#define TAU_PROFILE_TIMER(...)          // do nothing
#define TAU_PROFILE_START(...)          // do nothing
#define TAU_PROFILE_STOP(...)           // do nothing
#define TAU_PROFILE_SET_NODE(...)       // do nothing
#define SOS_FINALIZE()  SOS_finalize(_runtime)

#endif /* SOS_HAVE_TAU */
