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
echo "   6. Finish setting up the environment for use."
echo "" 
read -p " --> Would you like to do all of these things now? [Y/n]: " -i Y -e reply
if [ "$reply" == "Y" ]
then
    killall -q sosd 
    source /usr/workspace/wsa/pavis/third_party/toss3_gcc-4.9.3/unsetenv.sh
    source /usr/workspace/wsa/pavis/third_party/toss3_gcc-4.9.3/setenv.sh
    #export SOS_READ_SYSTEM_STATUS="true"
    evp.start.2
    sleep 2
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
    echo ""
    echo ">>>> Setting up the environment for VisIt:"
    source /usr/tce/packages/dotkit/init.sh
    use visit
    echo ""
    echo "--------------------------------------------------------------------------------"
    #cd ${PROJECT_BASE}/sos_vpa_2017/build
    #echo ""
    #echo "You are now in the \${PROJECT_BASE}/sos_vpa_2017/build directory:"
    echo ""
    echo -n "   "
    pwd
    echo ""
    echo "   TO RESET the databases........: \$ wipedb"
    echo "   TO SHUTDOWN the SOS runtime...: \$ evp.kill.2     (NOTE: before logout)"
    echo "   TO USE SOS's Python...........: #!/\${SOS_PYTHON}"
    echo "   TO FIND SOS's Python examples.: cd \${SOS_ROOT}/python"
    echo ""
    echo "Your environment should be configured to run SOSflow experiments!"
fi
echo ""
