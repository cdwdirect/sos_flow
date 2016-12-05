

# Link into the two databases...

attach database "sosd.0.db" as db0;
attach database "sosd.1.db" as db1;



# Find Missing Records between db0 and db1...

SELECT v0.rowid, v0.guid, v0.val
FROM db0.tblvals AS v0
WHERE NOT EXISTS (
   SELECT v1.rowid, v1.guid, v1.val
   FROM db1.tblvals AS v1
   WHERE v1.guid = v0.guid AND v1.time_pack = v0.time_pack);




# ----------
# TEMPLATES to Find all values not present in the second database:

SELECT A.ABC_ID, A.VAL WHERE NOT EXISTS 
   (SELECT * FROM B WHERE B.ABC_ID = A.ABC_ID AND B.VAL = A.VAL)
or

SELECT A.ABC_ID, A.VAL WHERE VAL NOT IN 
    (SELECT VAL FROM B WHERE B.ABC_ID = A.ABC_ID)
or

SELECT A.ABC_ID, A.VAL LEFT OUTER JOIN B 
    ON A.ABC_ID = B.ABC_ID AND A.VAL = B.VAL WHERE B.VAL IS NULL
    




#
#  GET all the CPU averages from the database...
#
#

SELECT tblvals.row_id, tbldata.name, tblvals.val, tblvals.time_pack, tblvals.time_send, tblvals.time_recv
FROM (tblvals INNER JOIN tbldata ON tblvals.guid = tbldata.guid)
WHERE tblvals.guid
      IN (SELECT guid FROM tbldata WHERE tbldata.name LIKE "cpu%");

#
#


#
#  Get all records of a type where any record of that type has a value...
#
SELECT
    tblPubs.comm_rank AS comm_rank,
    tblPubs.prog_name AS prog_name,
    tblNatures.text   AS prog_nature,
    tblData.name      AS val_name,
     tblVals.guid      AS val_guid,
    tblVals.val       AS value,
    tblMoods.text     AS val_mood,
   (tblVals.time_recv - tblvals.time_send)
                      AS db_insert_latency
FROM
    tblVals
    LEFT JOIN tblData  ON tblVals.guid        = tblData.guid
    LEFT JOIN tblPubs  ON tblData.pub_guid    = tblPubs.guid
    LEFT JOIN tblEnums AS tblNatures
                          ON ( tblNatures.type LIKE "NATURE"
                           AND tblPubs.meta_nature = tblNatures.enum_val )
    LEFT JOIN tblEnums AS tblMoods
                          ON ( tblMoods.type LIKE "MOOD"
                           AND tblVals.meta_mood = tblMoods.enum_val )
WHERE tblVals.guid IN
     ( SELECT DISTINCT tblVals.guid
       FROM tblVals
       WHERE tblVals.meta_mood = 3 )
   AND
      tblVals.rowid > 910000
   AND
      tblVals.rowid < 930000;



# Join across 3 tables...
#
#
SELECT
    tblPubs.comm_rank AS comm_rank,
    tblPubs.prog_name AS prog_name,
    tblNatures.text   AS prog_nature,
    tblData.name      AS val_name,
    tblVals.guid      AS val_guid,
    tblVals.val       AS value,
    tblMoods.text     AS val_mood,
   (tblVals.time_recv - tblvals.time_send)
                      AS db_insert_latency
FROM
    tblVals
    LEFT JOIN tblData  ON tblVals.guid        = tblData.guid
    LEFT JOIN tblPubs  ON tblData.pub_guid    = tblPubs.guid
    LEFT JOIN tblEnums AS tblNatures
                          ON ( tblNatures.type LIKE "NATURE"
                           AND tblPubs.meta_nature = tblNatures.enum_val )
    LEFT JOIN tblEnums AS tblMoods
                          ON ( tblMoods.type LIKE "MOOD"
                           AND tblVals.meta_mood = tblMoods.enum_val )
WHERE
     tblVals.rowid IN (1, 239024, 923223);


#
#  UPDATE query...
#

UPDATE tblVals SET meta_mood = 3 WHERE rowid = 200;

