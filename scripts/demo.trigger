#!/bin/bash
#
./evp.cleanall
export SOS_HANDLE="test_trigger_msgs"
export SOS_PAYLOAD="Hellooo"

echo "Starting AGGREGATOR 0..."
env SOS_CMD_PORT=20690 sosd -l 10 -a 3 -w `pwd` -k 0 -r aggregator &
echo "Starting AGGREGATOR 1..."
env SOS_CMD_PORT=20691 sosd -l 10 -a 3 -w `pwd` -k 1 -r aggregator &
echo "Starting AGGREGATOR 2..."
env SOS_CMD_PORT=20692 sosd -l 10 -a 3 -w `pwd` -k 2 -r aggregator &
echo "Waiting for 3 seconds..."
sleep 3
echo "Starting LISTENERS..."
for myport in `seq 22500 22509`;
do
    let "myrank = $myport - 22500 + 3"
    env SOS_CMD_PORT=$myport sosd -l 10 -a 3 -w `pwd` -k $myrank -r listener &
done
echo "All listeners are now online (ports 22500-22509)"
echo ""
echo ""
sleep 5

echo "Starting test_trigger (1 per listener)"
for myport in `seq 22500 22509`;
do
    env SOS_CMD_PORT=$myport ./test_trigger &
done

sleep 5

echo "Sending triggers to aggregators"

echo "Sending triggers to aggregator 20690"
env SOS_CMD_PORT=20690 sosd_trigger -h $SOS_HANDLE -p SOS_PAYLOAD

sleep 2
echo "Sending triggers to aggregator 20691"
env SOS_CMD_PORT=20691 sosd_trigger -h $SOS_HANDLE -p SOS_PAYLOAD

sleep 2
echo "Sending triggers to aggregator 20692"
env SOS_CMD_PORT=20692 sosd_trigger -h $SOS_HANDLE -p SOS_PAYLOAD

sleep 2

SLEEPKILL="n"
read -e -p "Do you want to sleep and then shut down the daemons? (y/N): " -i "n" SLEEPKILL
if [ "$SLEEPKILL" == "y" ]; then
    DELAYMAX=20
    read -e -p "How many seconds should elapse before bringing down the daemons? (20): " -i "20" DELAYMAX
    echo "Sleeping for $DELAYMAX seconds while they run..."
    for i in `seq 1 $DELAYMAX`;
    do
        echo -n "$i / $DELAYMAX ["
        for x in `seq 1 10`;
        do
            echo -n "."
            sleep 0.09
        done
        echo "]"
    done
    echo ""
    echo "Bringing down the listeners..."
    for myport in `seq 22500 22509`;
    do
        env SOS_CMD_PORT=$myport sosd_stop
    done
    killall -p test_trigger
    echo "All listeners have stopped.  (The stop signal should propagate to the aggregators.)"
else
    echo ""
    echo "Leaving script.  Daemons and demo_app are likely still running..."
    echo ""
    ps
    echo ""
    echo "To bring down all the listeners, run this command:"
    echo ""
    echo -E "    for myport in \`seq 22500 22509\`; do env SOS_CMD_PORT=\$myport \$SOS_BUILD_DIR/bin/sosd_stop; done; \$SOS_ROOT/scripts/showdb;"
    echo ""  
fi