#!/usr/bin/env python
# file "qryfile.py"
#
#   USAGE:   python ./qryfile.py <sos_db_filename>
#

import sys
import time
import os
import pprint as pp
from ssos import SSOS

def demonstrateSOS():
    SOS = SSOS()

    #print("Initializing SOS...")
    #SOS.init()

    sql_string = "SELECT * FROM viewCombined LIMIT 10000;"

    print("Querying FILE(" + sql_file + "): ")
    print("    " + sql_string)
    results, col_names = SOS.queryFile(sql_string, sql_file)
    print("Results:")
    print("    Output rows....: " + str(len(results)))
    print("    Output values..: " + str(results))
    print("    Column count...: " + str(len(col_names)))
    print("    Column names...: ")# + str(col_names)    #pp.pprint(col_names)
    pp.pprint(col_names)
    print("")
    print("Finalizing...")

    #SOS.finalize();
    print("   ...DONE!")
    print("")

if __name__ == "__main__":
    demonstrateSOS()




