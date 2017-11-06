#!/bin/bash
#
#
#
if [ "x$SOS_ENV_SET" == "x" ] ; then
	echo "Please set up your SOS environment."
    kill -INT $$
fi
#
if ls $SOS_EVPATH_MEETUP/sosd.*.key 1> /dev/null 2>&1
then
    THIS_RANK=0
    for keyfile in $SOS_EVPATH_MEETUP/sosd.*.key ; do
        EVP_PORT="$(~/local/bin/attr_dump $(cat $keyfile) | grep -F IP_PORT | cut -f9 -d ' ')"
        HOST_IP4="$(~/local/bin/attr_dump $(cat $keyfile) | grep -F IP_ADDR | cut -f9 -d ' ')"
        echo "$THIS_RANK $HOST_IP4 $SOS_CMD_PORT   (evpath_port:$EVP_PORT)"
        let "THIS_RANK++"
    done
else
    echo ""
    echo "There are no aggregator key files to display in \$SOS_EVPATH_MEETUP ..."
    echo ""
    echo -n "    "
    env | grep -F SOS_EVPATH_MEETUP
    echo ""
fi

