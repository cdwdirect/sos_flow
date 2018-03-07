#!/usr/workspace/wsa/pavis/third_party/toss3_gcc-4.9.3/python/bin/python

import sys
import time
import os
from mpl_toolkits.mplot3d import Axes3D
import matplotlib.cm as cm
from matplotlib.colors import Normalize
import matplotlib.pyplot as plt
import numpy as np
from ssos import SSOS


def queryAndPlot():
    SOS = SSOS()

    print "Initializing SOS..."
    SOS.init()

    #####
    #
    #  Get the maximum simulation frame.
    #
    #sql_string = """
    #SELECT
    #MAX(frame)
    #FROM viewCombined
    #WHERE viewCombined.value_name LIKE "lulesh.time"
    #;
    #"""
    #results, col_names = SOS.query(sql_string,
    #        "localhost",
    #        os.environ.get("SOS_CMD_PORT"))
    #max_cycle = int(results[0][0])
    #print "Max cycle: " + str(max_cycle)
    #
    #####


    #####
    #
    #  Get the list of field names for non-string values.
    #
    #  Removed:  AND frame = """ + str(max_cycle) + """
    #
    sql_string = """
    SELECT
    DISTINCT value_name
    FROM viewCombined
    WHERE value_type NOT LIKE "SOS_VAL_TYPE_STRING"
    ;
    """
    results, col_names = SOS.query(sql_string,
            "localhost", 
            os.environ.get("SOS_CMD_PORT"))
    print "Field names:"
    for field_name in results:
        print "    " + str(field_name)
    attr = dict()
    attr['value_name'] =  [el[0] for el in results]
    name_count = len(attr['value_name'])
    print str(name_count) + " unique names."
    #
    #####


    #####
    #
    #  Compose a query with those unique fields as columns in the results.
    #
    #  Removed: sql_string += " WHERE frame = " + str(max_cycle) + " "
    #
    sql_string  = """ """
    sql_string += """ SELECT """
    sql_string += """ comm_rank """
    sql_string += """,frame """
    for field_name in attr['value_name']:
        sql_string += """,GROUP_CONCAT( CASE WHEN """
        sql_string += ' value_name LIKE "' + field_name + '" '
        sql_string += ' THEN value END) AS "' + field_name + '" '
    sql_string += """ FROM viewCombined """
    sql_string += """ GROUP BY """
    sql_string += """ comm_rank """
    sql_string += """,frame """
    sql_string += """;"""
    print "Composite SQL statement: "
    print sql_string
    print ""
    print "Running composite query..."
    results, col_names = SOS.query(sql_string,
            "localhost",
            os.environ.get("SOS_CMD_PORT"))
    print ""
    #
    #  Print out the results:
    #
    print "=========="
    for col in col_names:
        print str(col) + " "
    print "=========="
    for row in results:
        for col_index in range(len(row)):
            print str(col_names[col_index]) + ": " + str(row[col_index])
        print "----------"
    print "=========="
    #
    #####

    SOS.finalize();
    print "   ...DONE!"
    print 
    #############
  
if __name__ == "__main__":
    queryAndPlot()
    #############



