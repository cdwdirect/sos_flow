#
#   USAGE:   python ./example.py [loop_count]
#
#   See also:   ./trace_example.sh [loop_count]
#

import time
import sys
from ssos_python import ffi, lib


def demonstrateSSOSPython():
    print "Initializing SSOS..."
    lib.SSOS_init()
    is_online_flag = ffi.new("int*")
    is_online_flag[0] = 0
    lib.SSOS_is_online(is_online_flag)
    while (is_online_flag[0] < 1):
        lib.SSOS_is_online(is_online_flag)
        print "   ..."
        time.sleep(0.5)

    print "Initializing variables for calling the API..."
    entry_name = ffi.new("char[]", "my_value")
    entry_type = lib.SSOS_TYPE_STRING
    entry_addr = ffi.new("char[]",       "Hello, SOS! Greetings from Python.")

    print "Packing, announcing, publishing..."
    lib.SSOS_pack(entry_name, entry_type, entry_addr)
    lib.SSOS_announce()
    lib.SSOS_publish()

    if (len(sys.argv) > 1):
        count = int(0)
        count_max = int(sys.argv[1])

        print "   Packing " + sys.argv[1] + " integer values in a loop..."

        loop_name = ffi.new("char[]", "loop_val")
        loop_type = lib.SSOS_TYPE_INT
        loop_addr = ffi.new("int*");

        loop_addr[0] = count
        count = count + 1
        lib.SSOS_pack(loop_name, loop_type, loop_addr)
        lib.SSOS_announce()
        lib.SSOS_publish()

        while (count < count_max):
            loop_addr[0] = count
            count = count + 1
            lib.SSOS_pack(loop_name, loop_type, loop_addr)

        print "   Publishing the values..."
        lib.SSOS_publish()
        print "      ...OK!"


    print "Finalizing..."
    lib.SSOS_finalize();
    print ""
    print "   ...DONE!"
    print ""


if __name__ == "__main__":
    demonstrateSSOSPython()




