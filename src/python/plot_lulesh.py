#!/usr/workspace/wsa/pavis/third_party/toss3_gcc-4.9.3/python/bin/python

import sys
import subprocess
import time
import os
from mpl_toolkits.mplot3d import Axes3D
import matplotlib.cm as cm
from matplotlib.colors import Normalize
import matplotlib.pyplot as plt
import numpy as np
from ssos import SSOS
import vtk_writer

visit_path="/usr/gapps/visit/current/linux-x86_64-toss3/lib/site-packages/"
sys.path.append(visit_path)
from visit import *
import visit

def queryAndPlot():
    SOS = SSOS()

    print("Initializing SOS...")
    SOS.init()
    print("DONE init SOS...")
    sql_string = """
    SELECT MAX(frame) FROM viewCombined WHERE viewCombined.value_name LIKE "lulesh.time"
    ;
    """
    results, col_names = SOS.query(sql_string,
            "localhost",
            os.environ.get("SOS_CMD_PORT"))

    max_cycle = int(results[0][0])
    print("Max cycle: " + str(max_cycle))

    #####
    #
    #  Get the list of field names for non-string values.
    #
    sql_string = """
    SELECT
    DISTINCT value_name
    FROM viewCombined
    WHERE value_type != 3
    AND frame = """ + str(max_cycle) + """
    ;
    """
    results, col_names = SOS.query(sql_string,
            "localhost",
            os.environ.get("SOS_CMD_PORT"))
    numeric_fields = dict()
    numeric_fields['name'] = [el[0] for el in results]
    name_count = len(numeric_fields['name'])
    print(str(name_count) + " unique names.")
    #
    #####


    filenames = []
    for c in range(0, (max_cycle + 1)):
      print("******* CYCLE " +str(c)+" *********")
      #####
      #
      #  Compose a query with the unique numeric fields as columns:
      #
      sql_string  = """ """
      sql_string += """ SELECT """
      sql_string += """ comm_rank """
      # sql_string += """,frame """
      for field_name in numeric_fields['name']:
          sql_string += """,GROUP_CONCAT( CASE WHEN """
          sql_string += ' value_name LIKE "' + field_name + '" '
          sql_string += ' THEN value END) AS "' + field_name + '" '
      sql_string += """, GROUP_CONCAT( CASE WHEN """
      sql_string += ' value_name LIKE "lulesh.coords" '
      sql_string += ' THEN value END) AS "lulesh.coords" '
      sql_string += """ FROM viewCombined """
      sql_string += " WHERE frame = " + str(c) + " "
      sql_string += """ GROUP BY """
      sql_string += """ comm_rank """
      # sql_string += """,frame """
      sql_string += """;"""
      #print("Composite SQL statement: ")
      #print(sql_string)
      #print("")
      #print("Running composite query...")
      results, col_names = SOS.query(sql_string,
              "localhost",
              os.environ.get("SOS_CMD_PORT"))
      #print("")
      #
      #  Print out the results:
      #
      #print("==========")
      #for col in col_names:
      #    print(str(col) + " ")
      #print("==========")
      #for row in results:
      #    for col_index in range(len(row)):
      #        print(str(col_names[col_index]) + ": " + str(row[col_index]))
      #    print("----------")
      #print("==========")
      #
      #####


      #####
      #
      #  Build an attribute dictionary of the values.
      #
      attr = dict()
      attr['comm_rank']  =  [el[0] for el in results]

      position = 1
      for field_name in numeric_fields['name']:
          attr[field_name] = [el[position] for el in results]
          #print(str(field_name) + " in position " + str(position) + " = " + str(attr[field_name]))
          position += 1
      res_coords = [el[position] for el in results]
      #print("lulesh.coords in position " + str(position) + " = " + str(res_coords))

      for field_name in numeric_fields['name']:
          rank = 0
          for this_ranks_value in attr[field_name]:
              print("comm_rank(" + str(rank) + ")." + field_name + " = " + this_ranks_value)
              rank += 1

      rank_max = len(attr['comm_rank'])
      coords = list()
      coords = [el.split() for el in res_coords]
      #print(str(attr))
      dset = vtk_writer.vtk_hex_data_set()
      dset.clear()
      dset.set_cycle(c)
      for rank in range(rank_max):
          fields = {}
          for field_name in numeric_fields['name']:
              fields[field_name] = attr[field_name][rank]
          fields["rank"] = rank
          hex_coords = [None]*24
          xpt = [None]*8
          ypt = [None]*8
          zpt = [None]*8
          cpt = [None]*8
          for i in range(24):
            hex_coords[i] = float(coords[rank][i])
          dset.add_hex(hex_coords, fields, rank)
      dset.write_vtk_file()
      filenames.append(dset.get_file_name())

    vtk_writer.write_visit_file(filenames)

    visit.AddArgument("-par")
    visit.Launch()
    OpenDatabase("dataset.visit")
    AddPlot("Pseudocolor", "rank")
    AddPlot("Mesh", "mesh")
    DrawPlots()
    # loop through times
    tsNames = GetWindowInformation().timeSliders
    for ts in tsNames:
      SetActiveTimeSlider(ts)
    for state in list(range(TimeSliderGetNStates()))[::10] + [0]:
        SetTimeSliderState(state)
    print("Setting share_power permissions on the newly created VTK files...")
    subprocess.call("$PROJECT_BASE/share_power .", shell=True)
    print("")
    print("Sleeping for 100 seconds...")
    print("")
    time.sleep(100)

    SOS.finalize();
    print("   ...DONE!")
    print("")
    #############

if __name__ == "__main__":
    queryAndPlot()
    #############



