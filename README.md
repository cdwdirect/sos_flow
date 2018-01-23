![Alt](/ref/sosflow_masthead.png "SOSflow")

SOSflow provides a flexible, scalable, and programmable framework for
observation, introspection, feedback, and control of HPC applications.

# :green_book: Documentation: [SOSflow Wiki](https://github.com/cdwdirect/sos_flow/wiki)

---

The Scalable Observation System (SOS) performance model used by
SOSflow allows a broad set of online and in situ capabilities including
remote method invocation, data analysis, and visualization. SOSflow can
couple together multiple sources of data, such as application components
and operating environment measures, with multiple software libraries and
performance tools, efficiently creating holistic views of performance at
runtime. SOSflow can extend the functionality of HPC applications by
connecting and coordinating accessory components, such as in situ
visualization tools that are activated only when the primary application
is not performing compute-intensive work.

---

# :sos: Example Code

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

SOS example of "Hello, world!" using Python:

```Python
#!/usr/bin/env python

import os
from ssos import SSOS

def demonstrateSOS():
    SOS = SSOS()

    sos_host = "localhost"
    sos_port = os.environ.get("SOS_CMD_PORT")

    SOS.init()
    SOS.pack("somevar", SOS.STRING, "Hello, SOS.  I'm a python!")
    SOS.announce()
    SOS.publish()

    sql_string = "SELECT * FROM tblVals LIMIT 10000;"
    
    results, col_names = SOS.query(sql_string, sos_host, sos_port)
    
    print "Results:"
    print "    Output rows....: " + str(len(results))
    print "    Output values..: " + str(results)
    print "    Column count...: " + str(len(col_names)) 
    print "    Column names...: " + str(col_names)
    print ""

    SOS.finalize();


if __name__ == "__main__":
    demonstrateSOS()

```











