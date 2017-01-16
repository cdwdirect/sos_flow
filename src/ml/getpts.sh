 sqlite3 -csv -separator 'TAB' -noheader $SOS_WORK/sosd.00000.db "SELECT val FROM tblVals;" | tr -d '"' | sed 's/, /\t/' > points.csv
