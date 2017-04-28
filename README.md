![Alt](/ref/sosflow_masthead.png "SOSflow")

The Scalable Observation System for Scientific Workflows (SOSflow)
is a runtime service and client API designed for low-overhead
online capture of performance data and user-defined events from
many sources within massively parallel HPC systems.

Data gathered into SOSflow is annotated with context and meaning,
and is stored in a way that can be searched in situ with dynamic queries
in real-time, as well as being aggregated for global perspective and
long-term archival.

## Compatibility

The primary components of SOSflow are developed and tested in various
modern Linux systems and OS X.

SOSflow has the following requirements:
* C Compiler (C99 or newer)
* POSIX Threads
* Message Passing Interface (MPI)
* SQLite 3
* CMake 2.8+

## Downloading

Open a Terminal and issue the following command to pull down
the latest copy of SOSflow into a new folder named "sos_flow":

    git clone https://github.com/cdwdirect/sos_flow.git

The folder that is created and contains the cloned repository
will be referred to as [SOSROOT] for the remainder of this
document. Use the relative or fully-qualified path to this
folder wherever you see [SOSROOT] in the text or example
commands below.

The "master" repository branch is the default and contains the
latest stable source code.

## Configuration

See the [SOSROOT]/CMakeLists.txt file's OPTION settings for
various ways to build SOSflow.  The default settings should
be set up to use MPI as a the means of communicating between
instances of the SOSflow runtime environment daemons, but
there are other options available such as EVPath.

Make sure to adjust these settings prior to running the
configuration scripts when building SOSflow.

## Environment

Configurations for various target environments are included
in the [SOSROOT]/hosts folder. SOSflow provides scripts to
help with configurtion, build, and running examples. These
scripts use relative pathing based on environment variables.

To set up the environment for a traditional Linux operating
system like Ubuntu or RHEL, run the following command:

    source [SOSROOT]/hosts/linux/setenv.sh

You can see what environment parameters were established
by typing this into your terminal:

    env | grep SOS

You should see an environment variable named $SOS_ROOT that
has the full path that we are using here as [SOSROOT]. Now
you should be ready to build and run SOSflow.

## Compilation

A configuration script is provided to create a build folder
and drive CMake's Makefile generation process. To run the
configuration script for a generic Linux environmet, type the
following command into your Terminal:

    [SOSROOT]/scripts/configure.sh -c linux

The -c option will instruct the script to clean up any
existing build folder for that target.  Running the script
with the "linux" option will create a folder named
"build-linux" inside the [SOSROOT] folder. Go into this
folder and type "make":

    cd [SOSROOT]/build-linux
    make

This should complete successfully if there are no missing
required libraries. Get in touch with the SOSflow AUTHORS
if you have any issues at this step.

## Testing

To start up a single-node test configuration of the SOSflow
runtime environment, enter the following command at your
Terminal:

    cd [SOSROOT]/build-linux
    ../scripts/mpi.start.2 &

The ampersand will cause the daemons to launch but to run
as background processes. After a few seconds you can press
your Return (ENTER) key several times and see the command
prompt ready for new commands.

When following these directions, using the provided example
scripts as above, the working directory for SOSflow will be
the same place where you built and launched the daemons.
You should see several new files in that folder, such as:

    sosd.00000.db
    sosd.00000.lock
    sosd.00000.key
    . . .

To run the demonstration / test application for a generic
Linux environment, enter the following command at your
terminal:

    cd [SOSROOT]/build-linux
    ./bin/demo_app -i 100 -m 2000000 -p 200 -d 100000

This will execute the demonstration / stress-test app that
comes bundled with SOSflow.  It will create a publication
handle with 200 elements, it will publish new values into
all 200 of them 100 times between each time it publishes
them up to the in situ daemon, and it will ensure a delay
of at least 1/10th of a second between attempts to connect
and publish the values to the SOSflow runtime daemon.

The demo_app application is itself an MPI application that
can be launched multiple times in situ to test out the
throughput and latency of information moving through the
system, as well as the total capacity of the daemon to
handle various workloads.

## Basic Concepts 

The SOS daemon is running and listening to a port on its node
as specified by the $SOS_CMD_PORT environment variable.
Communication between the SOS client library and the in
situ daemon takes place over the same local TCP/IP socket.

Though the SOS daemon can be compiled as an MPI application,
and the SOS client library can be linked into and used by
and MPI application, the interactions between SOS clients
and the SOS daemon do not use any MPI resources and are
entirely transparent to the MPI functionality of existing
codes.

The core data structure of SOS is the publication handle, or
"pub".  A pub contains all of the information that is
packed into it with the SOS_pack(...) API calls, along with
a set of relevant metadata about that information and the
context it originated from.

Information packed into a pub handle can be thought of as
a key/value pair, except that there is a large amount of
context and metadata affiliated with the value, and that the
history (and timing) of updates to the value are completely
preserved by the client, runtime daemon, and the persistent
databases. This allows for the tracking and analysis of the
evolution of a  system's holistic state, assuming asynchronous
overlapping components and algorithms interacting in real-
time.

The unique key for a value that has been packed into a pub
is the name of that value as a text string. In the example
provided below this is "examplevalue". This key has a many-to-
one relationship with every instance of the value that was
packed into the publication handle.

It is important to call SOS_announce(...) on a publication
handle if it is possible that new unique value names were
added to it. This supports future use cases where SOS may
behave as a true publish-subscribe system rather than the more
scalable and efficient push/pull model it presently uses.

The SOS client library is reentrant and thread-safe, and can
be invoked from multiple components or threads within the same
application binary. Because it is thread safe, it is advised
that independent libraries or components within an application,
or any non-coupled threads, create their own publication handles
to avoid sharing mutexes. After information has been submitted
to the SOS runtime, it can later be searched for and aggregated
on the PID of the source application, so independent pub
handles can be coorelated as belonging to the same actual
source binary.

Once values are sent up to the in situ SOS runtime daemon
they will move through various asynchronous queues for
analysis, migration off-node, and injection into a queryable
in situ SQL store. The SQL store resides in the $SOS_WORK
path, or if that is "." it will be found in the folder
where the SOS daemon's were invoked.

At this time the default SQL store is SQLite3. The use of
the SQL store is beyond the scope of this initial document,
but several examples are provided in the [SOSROOT]/src
and [SOSROOT]/src/soapy directories.

A Python module is available that provides support for
making calls to the SOS API natively within Python scripts.
Instructions for building and an example of using this
module are found in the [SOSROOT]/src/python directory.

## Example Code

The following C source provides a minimal example of using
the SOSflow runtime to capture values:
```C
#include <stdio.h>
#include "sos.h"

int main(int argc, char **argv) {

    // Initialize the client, registering it with the SOS runtime.
    // In an MPI application, this is usually called immediately
    // after the MPI_Init(...) call.
    SOS_runtime *sos = NULL;
    SOS_init(&argc, &argv, &sos,
        SOS_ROLE_CLIENT, SOS_RECIEVES_NO_FEEDBACK, NULL);

    SOS_pub *pub = NULL;
    SOS_pub_create(sos, &pub, "demo",
        SOS_NATURE_CREATE_OUTPUT); 

    int someInteger = 256;
    SOS_pack(pub, "examplevalue", SOS_VAL_TYPE_INT, &someInteger);

    SOS_announce(pub);
    SOS_publish(pub);

    for (someInteger = 1024; someInteger <= 2048; someInteger++) {
        // All these pack'ed values will accumulate within the
        // client until the next SOS_publish(...) is called on the
        // publication handle.
        SOS_pack(pub, "examplevalue",
            SOS_VAL_TYPE_INT, &someInteger);
    }

    SOS_publish(pub);

    // This is called at the end of an application, when it will no
    // longer be contributing to the SOS environment or responding
    // to feedback directives from SOS.
    // In an MPI application, the client usually will call this
    // immediately before the call to MPI_finalize().
    SOS_finalize(sos);

    return 0;
}
```














