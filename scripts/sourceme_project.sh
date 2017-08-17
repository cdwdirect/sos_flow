#!/bin/bash
if [ "x$PROJECT_BASE" == "x" ] ; then
	echo "Please set \$PROJECT_BASE to the correct location and try agian."
    kill -INT $$
fi
echo ""
echo "Experiment setup:"
echo ""
echo "   1. Source the [un]setenv.sh scripts."
echo "      ... unset SOS environment variables (if they exist)."
echo "      ... set SOS environment variables."
echo "   2. Launch the daemons."
echo "   3. Verify the database exists and is empty."
echo "      ... \$ showdb"
echo "   4. Verify that SOS is adding records to the database."
echo "      ... \$ \${SOS_BUILD_DIR}/bin/demo_app -i 1 -p 5 -m 25"
echo "      ... \$ showdb"
echo "   5. Clear the database and move to the experiment folder:"
echo "      ... \$ wipedb"
echo "      ... \$ cd \$SOS_ROOT/../sos_vpa_2017/build/."
echo "" 
read -p " --> Would you like to do all of these things now? [Y/n]: " -i Y -e reply
if [ "$reply" == "Y" ]
then
    killall -q sosd 
    source $PROJECT_BASE/unsetenv.sh
    source $PROJECT_BASE/setenv.sh
    evp.start.2
    sleep 2
    showdb
    ${SOS_BUILD_DIR}/bin/demo_app -i 1 -p 5 -m 25
    sleep 0.5
    echo ""
    echo ">>>> SOS Database:"
    echo ""
    sqlite3 ${SOS_WORK}/sosd.00000.db "SELECT COUNT(pub_guid) FROM viewCombined;" 
    echo " ^---- You should see 25 (records) here."
    wipedb
    echo ">>>> SOS Environment:"
    echo ""
    env | grep SOS
    echo ""
    echo ">>>> SOS Daemons (running):"
    echo ""
    ps -ely | grep sosd | grep -v grep
    echo "--------------------------------------------------------------------------------"
    cd ${PROJECT_BASE}
    echo ""
    echo "You are now in the \${PROJECT_BASE} directory:"
    echo ""
    echo -n "   "
    pwd
    echo ""
    echo "   TO RESET the databases........: \$ wipedb"
    echo "   TO SHUTDOWN the SOS runtime...: \$ evp.kill.2     (NOTE: before logout)"
    echo "   TO USE SOS's Python...........: \$SOS_PYTHON"
    echo "   TO FIND SOS's Python examples.: cd \${SOS_ROOT}/python"
    echo ""
    echo "Your environment should be configured to run SOSflow experiments!"
fi
echo ""
