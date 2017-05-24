#!/bin/bash

module use /projects/tau/packages/Modules/modulefiles
module load python/2.7.13
module swap intel intel/17.0.2

if [ "x$PYTHONPATH" == "x" ] ; then
  export PYTHONPATH=$SOS_ROOT/src/python
else
  export PYTHONPATH=$SOS_ROOT/src/python:$PYTHONPATH
fi

if [ "x$LD_LIBRARY_PATH" == "x" ] ; then
  export LD_LIBRARY_PATH=$SOS_ROOT/src/python
else
  export LD_LIBRARY_PATH=$SOS_ROOT/src/python:$LD_LIBRARY_PATH
fi


