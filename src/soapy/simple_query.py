import os
import sqlite3

# name of the sqlite database file
sqlite_file = os.environ.get("SOS_WORK", ".") + "/sosd.1.db"
table_name = 'tblvals'   # name of the table to be queried


# Connecting to the database file
conn = sqlite3.connect(sqlite_file)
c = conn.cursor()


sql_statement = (" SELECT "
                 "   tblvals.row_id, tbldata.name, tblvals.val, tblvals.time_pack, tblvals.time_send, tblvals.time_recv "
                 " FROM "
                 "   (tblvals INNER JOIN tbldata ON tblvals.guid = tbldata.guid) "
                 " WHERE "
                 "   tblvals.guid "
                 "      IN (SELECT guid FROM tbldata WHERE tbldata.name LIKE '%power%') "
                 " AND "
                 "   tblvals.rowid < 300; ")


c.execute(sql_statement);

all_rows = c.fetchall()
print('1):', all_rows)


# Closing the connection to the database file
conn.close()

#end






#########
#  Reference:
#
#
#
# 1) Contents of all columns for row that match a certain value in 1 column
#c.execute('SELECT * FROM {tn} WHERE {cn}={cf}'.\
#          format(tn='tblvals', cn=col_name, cf=col_filter))
#
# 2) Value of a particular column for rows that match a certain value in column_1
#c.execute('SELECT ({coi}) FROM {tn} WHERE {cn}="Hi World"'.\
#        format(coi=column_2, tn=table_name, cn=column_2))
#all_rows = c.fetchall()
#print('2):', all_rows)
#
# 3) Value of 2 particular columns for rows that match a certain value in 1 column
#c.execute('SELECT {coi1},{coi2} FROM {tn} WHERE {coi1}="Hi World"'.\
#        format(coi1=column_2, coi2=column_3, tn=table_name, cn=column_2))
#all_rows = c.fetchall()
#print('3):', all_rows)
#
# 4) Selecting only up to 10 rows that match a certain value in 1 column
#c.execute('SELECT * FROM {tn} WHERE {cn}="Hi World" LIMIT 10'.\
#        format(tn=table_name, cn=column_2))
#ten_rows = c.fetchall()
#print('4):', ten_rows)
#
# 5) Check if a certain ID exists and print its column contents
#c.execute("SELECT * FROM {tn} WHERE {idf}={my_id}".\
#        format(tn=table_name, cn=column_2, idf=id_column, my_id=some_id))
#id_exists = c.fetchone()
#if id_exists:
#    print('5): {}'.format(id_exists))
#else:
#    print('5): {} does not exist'.format(some_id))
#
