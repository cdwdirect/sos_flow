SELECT
  value_guid,
  CASE WHEN value_name LIKE "lulesh.cycle" THEN value END AS cycle,
  CASE WHEN value_name LIKE "lulesh.time" THEN value END as time,
  CASE WHEN value_name LIKE "lulesh.dtime" THEN value END as dtime 
  FROM viewCombined
  LIMIT 30;   



SELECT
 	v.value_guid,
 	v.frame,
 	CASE WHEN s.value_name LIKE "lulesh.cycle" THEN s.value END AS cycle,
 	CASE WHEN s.value_name LIKE "lulesh.time" THEN s.value END as time,
 	CASE WHEN s.value_name LIKE "lulesh.dtime" THEN s.value END as dtime 

FROM viewCombined v
JOIN viewCombined s
  ON v.value_guid = s.value_guid
 AND v.frame = s.frame

GROUP BY
	v.value_guid,
	v.frame

LIMIT 30;   




SELECT
 	v.value_guid,
 	v.frame,
 	CASE WHEN s.value_name LIKE "lulesh.cycle" THEN s.value END AS cycle,
 	CASE WHEN s.value_name LIKE "lulesh.time" THEN s.value END as time,
 	CASE WHEN s.value_name LIKE "lulesh.dtime" THEN s.value END as dtime 

FROM viewCombined v
JOIN viewCombined s
  ON v.value_guid = s.value_guid
 AND v.frame = s.frame

GROUP BY
	v.value_guid,
	v.frame

LIMIT 30;   




    SELECT
 	    v.value_guid AS guid,
        v.frame AS frame,
        GROUP_CONCAT(vCycle.value AS cycle,
        GROUP_CONCAT(vTime.value AS time,
        GROUP_CONCAT(vDTime.value AS dtime
    
    FROM
        viewCombined v
        LEFT JOIN (SELECT * FROM viewCombined WHERE value_name LIKE "lulesh.cycle") vCycle
            ON  vCycle.value_guid = v.value_guid
            AND vCycle.frame = v.frame
        LEFT JOIN (SELECT * FROM viewCombined WHERE value_name LIKE "lulesh.time") vTime
            ON  vTime.value_guid = v.value_guid
            AND vTime.frame = v.frame
        LEFT JOIN (SELECT * FROM viewCombined WHERE value_name LIKE "lulesh.dtime") vDTime
            ON  vDTime.value_guid = v.value_guid
            AND vDTime.frame = v.frame

    GROUP BY
        cycle
    ;


