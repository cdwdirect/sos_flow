#!/usr/bin/python

import sys
import time
import os
import random
import re
from   ssos import SSOS

import pandas as pd

def printf(format, *args):
    sys.stdout.write(format % args)

def getSosRelationalTable(sos):

    sqlFieldNames = """ SELECT DISTINCT value_name FROM viewCombined;"""
    results, col_names = SOS.query(sqlFieldNames, sosHost, sosPort)
    selectedFields = dict()
    selectedFields['name'] = [el[0] for el in results]

    sqlValsToColsByRank  = """ """
    sqlValsToColsByRank += """ SELECT """
    sqlValsToColsByRank += """ comm_rank """
    for field_name in selectedFields['name']:
        sqlValsToColsByRank += """,GROUP_CONCAT( CASE WHEN """
        sqlValsToColsByRank += ' value_name LIKE "' + field_name + '" '
        sqlValsToColsByRank += ' THEN value END) AS "' + field_name + '" '

    sqlValsToColsByRank += """ FROM viewCombined """
    sqlValsToColsByRank += """ GROUP BY """
    sqlValsToColsByRank += """ relation_id """
    sqlValsToColsByRank += """;"""

    results, col_names = SOS.query(sqlValsToColsByRank, sosHost, sosPort)

    return (col_names, results)

if __name__ == "__main__":
    SOS = SSOS()

    sosHost = "localhost"
    sosPort = os.environ.get("SOS_CMD_PORT")

    printf("Initializing SOS: ...\b\b\b")
    SOS.init()
    printf("OK!\n")

    (columns, values) = getSosRelationalTable(SOS)

    df = pd.DataFrame(values, columns=columns)
    print df

    SOS.finalize();

    print "   ...DONE!"
    print
