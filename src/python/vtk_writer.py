#!/usr/workspace/wsa/pavis/third_party/toss3_gcc-4.9.3/python/bin/python
import sys
visit_path="/usr/gapps/visit/current/linux-x86_64-toss3/lib/site-packages/"
sys.path.append(visit_path)
from visit import *
import visit
import time

class vtk_hex_data_set:
  __points = []
  __cell_fields = {}
  __rank_ids = []
  __size = 0
  __cycle = 0
  __filename = ""  
  def print_self(self):
    print "total cells"
    print (self.__size)
    print "coords:"
    print(self.__points)
    print "fields:"
    print(self.__cell_fields)
    print "rank_ids:"
    print(self.__rank_ids)

  def clear(self):
    self.__points = []
    self.__cell_fields = {}
    self.__rank_ids = []
    self.__size = 0
    self.__cycle = 0
    self.__filename = ""  

  def set_cycle(self, cycle):
    self.__cycle = cycle
  
  def get_file_name(self):
    return self.__filename

  def add_hex(self, coords, field_vals, rank_id):
    # make sure that we have 8 x 3 coords
    assert len(coords) == 24
    # make sure we have a fields
    assert len(field_vals) != 0 
    # make sure we always have the same number of fields
    # for each hex
    if(len(self.__cell_fields) != 0):
      assert len(field_vals) == len(self.__cell_fields)
    else:
      # init keys for first insertion
      for field in field_vals:
        self.__cell_fields[field] = []

    for c in coords:
      self.__points.append(c)
    
    for field_name in field_vals:
      self.__cell_fields[field_name].append(field_vals[field_name])
    
    self.__rank_ids.append(rank_id)

    self.__size = self.__size + 1

  def write_vtk_file(self):
    filename = "data_set." + str(self.__cycle) + ".vtk"
    self.__filename = filename
    f = open(filename, "wb")
    f.write("# vtk DataFile Version 3.0\n")
    f.write("vtk output\n")
    f.write("ASCII\n")
    f.write("DATASET UNSTRUCTURED_GRID\n")
    f.write("\n")
    num_coords = self.__size * 8
    f.write("POINTS " + str(num_coords) + " float\n")
    # 
    # write out the points
    #
    for c in range(0,num_coords):
      line = str(self.__points[c * 3 + 0]) 
      line = line + " " + str(self.__points[c * 3 + 1])
      line = line + " " + str(self.__points[c * 3 + 2]) + "\n"
      f.write(line)
    f.write("\n")
    # 
    # write out the cell conn 
    #
    f.write("CELLS " + str(self.__size) + " " + str(self.__size * 9) +"\n")
    for i in range(0,self.__size):
       f.write("8 ") # num indices per hex 
       for j in range(0,8):
         if(j != 7):
           f.write(str( i * 8 + j) + " ")
         else:
           f.write(str( i * 8 + j) + "\n")
    f.write("\n")
    # 
    # write out the cell types all hexes (12) 
    #
    f.write("CELL_TYPES " + str(self.__size) +"\n")
    for c in range(0,self.__size):
      f.write("12\n")
    # 
    # write out all the fields 
    #
    f.write("CELL_DATA " + str(self.__size) + "\n")
    for field in self.__cell_fields:
      vals = self.__cell_fields[field]
      f.write("\n");
      f.write("SCALARS " + field  + " float 1\n")
      f.write("LOOKUP_TABLE default\n")
      for c in range(0,self.__size):
        f.write(str(vals[c]) + "\n")
    #
    # write out rank ids
    #
    f.write("SCALARS rank float 1\n")
    f.write("LOOKUP_TABLE default\n")
    for c in range(0,self.__size):
      f.write(str(self.__rank_ids[c]) + "\n")
    f.close()

def write_visit_file(filenames):
  f = open("dataset.visit", "wb")
  size = len(filenames)
  for name in filenames:
    print name
    f.write(name + "\n")
  f.close()

def generate_hex(data_set, rank):
  coords = []
  coords.append(rank)
  coords.append(rank)
  coords.append(0)

  coords.append(rank + 1)
  coords.append(rank)
  coords.append(0)

  coords.append(rank + 1)
  coords.append(rank + 1)
  coords.append(0)

  coords.append(rank)
  coords.append(rank + 1)
  coords.append(0)

  coords.append(rank)
  coords.append(rank)
  coords.append(1)

  coords.append(rank + 1)
  coords.append(rank)
  coords.append(1)

  coords.append(rank + 1)
  coords.append(rank + 1)
  coords.append(1)

  coords.append(rank)
  coords.append(rank + 1)
  coords.append(1)

  
  fields = {}
  fields["f1"] = rank * 0.1
  fields["f2"] = rank * 0.2
  fields["f3"] = rank * 0.3
  data_set.add_hex(coords, fields, rank)

#data = vtk_hex_data_set()
#for i in range(1, 5):
#  generate_hex(data, i)
#
##data.print_self()
#data.write_vtk_file();

#visit.Launch()
#OpenDatabase("data_set.0.vtk")
#AddPlot("Pseudocolor", "rank")
#DrawPlots()
#time.sleep(100)
