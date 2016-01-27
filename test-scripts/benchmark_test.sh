#!/bin/bash
########################################################################
# Copyright (C) 2015 Hewlett Packard Enterprise Development LP
# All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License. You may obtain
# a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.
########################################################################

# This script should be used to test the ovsdb-benchmark tool and should
# be copied to the switch file system in order to be executed. It would
# be commonly placed at /tmp and the results will be a tests/ folder with
# a structure like:
#
#     tests
#     ├── counters
#     ├── insert
#     ├── insert-index
#     ├── insert-without-index
#     ├── memory-usage
#     ├── pubsub
#     ├── transaction-size
#     └── update
#
# where each subfolder should contain the CSV files with the result of
# each test performed with different configurations.

# Cleans the table used for the test and compacts the database
clean_test(){

    ovsdb-client transact '["OpenSwitch", {"op":"delete","table":"'$1'","where":[]},{"op":"commit","durable":true}]';
    ovs-appctl -t ovsdb-server ovsdb-server/compact;
    sleep 5s;
}

# delete undesired lines on CSV log files
clean_files(){

    FILES=`find tests -type f -name '*.csv'`
    for file in $FILES
    do
        sed -i '/"count"/d' ${file}
    done
}

# Function to perform the insertion test.
# tests[] and workers[] arrays should be modified for the desired
# tests to log.
# i-th test parameters are the i-th element on each of the configuration
# arrays
insert(){

    test_name="insert"
    test_table="Test"
    clean_test ${test_table};
    test_folder="tests/"${test_name}
    mkdir -p ${test_folder}

    tests=('50000' '50000' '50000' '1000' '1000' '1000' '1000' \
             '100'   '100'   '100'  '100'  '100')
    workers=(  '1'     '5'    '10'   '10'   '25'   '50'   '75' \
             '100'   '250'   '500'  '750' '1000')

    if [ ${#tests[@]} -eq ${#workers[@]} ];
    then
        for (( i=0; i<${#tests[@]}; i++))
        do
            if [ "${1}" == "-c" ];
            then
                filebase=`echo ${test_folder}"/n"${tests[$i]}"_w"${workers[$i]}"_nocache"`;
                logfile=`echo ${filebase}".csv"`;
                errfile=`echo ${filebase}".err"`;
                ovsdb-benchmark ${test_name} -n${tests[$i]} -w${workers[$i]} -c 1>${logfile} 2>${errfile};
                clean_test ${test_table};
            else
                filebase=`echo ${test_folder}"/n"${tests[$i]}"_w"${workers[$i]}`;
                logfile=`echo ${filebase}".csv"`;
                errfile=`echo ${filebase}".err"`;
                ovsdb-benchmark ${test_name} -n${tests[$i]} -w${workers[$i]} 1>${logfile} 2>${errfile};
                clean_test ${test_table};
            fi
        done
    else
        echo ${testname}"test failed: arguments size not correct"
    fi
}

# Function to perform the insertion test and collect read/write statistics
# tests[] and pool[] arrays should be modified for the desired tests to log.
# i-th test parameters are the i-th element on each of the configuration
# arrays
insert_index(){

    test_name="insert-index"
    test_table="TestIndex"
    clean_test ${test_table};
    test_folder="tests/"${test_name}
    mkdir -p ${test_folder}

    tests=('100'  '250'  '500' '1000' '1500' '2000' '2500' '3000')
    pool=('1000' '1000' '1000' '1000' '1000' '1000' '1000' '1000')

    if [ ${#tests[@]} -eq ${#pool[@]} ];
    then
        for (( i=0; i<${#tests[@]}; i++))
        do
            if [ "${1}" == "-c" ];
            then
                filebase=`echo ${test_folder}"/n"${tests[$i]}"_m"${pool[$i]}"_nocache"`;
                logfile=`echo ${filebase}".csv"`;
                errfile=`echo ${filebase}".err"`;
                ovsdb-benchmark ${test_name} -n${tests[$i]} -m${pool[$i]} -c 1>${logfile} 2>${errfile};
                clean_test ${test_table};
            else
                filebase=`echo ${test_folder}"/n"${tests[$i]}"_m"${pool[$i]}`;
                logfile=`echo ${filebase}".csv"`;
                errfile=`echo ${filebase}".err"`;
                ovsdb-benchmark ${test_name} -n${tests[$i]} -m${pool[$i]} 1>${logfile} 2>${errfile};
                clean_test ${test_table};
            fi
        done
    else
        echo ${testname}"test failed: arguments size not correct"
    fi
}

# Function to perform the insertion test and collect read/write statistics
# using indexed columns
# tests[] and pool[] arrays should be modified for the desired tests to log.
# i-th test parameters are the i-th element on each of the configuration
# arrays
insert_without_index(){

    test_name="insert-without-index"
    test_table="Test"
    clean_test ${test_table};
    test_folder="tests/"${test_name}
    mkdir -p ${test_folder}

    tests=('100'  '250'  '500' '1000' '1500' '2000' '2500' '3000')
    pool=('1000' '1000' '1000' '1000' '1000' '1000' '1000' '1000')

    if [ ${#tests[@]} -eq ${#pool[@]} ];
    then
        for (( i=0; i<${#tests[@]}; i++))
        do
            if [ "${1}" == "-c" ];
            then
                filebase=`echo ${test_folder}"/n"${tests[$i]}"_m"${pool[$i]}"_nocache"`;
                logfile=`echo ${filebase}".csv"`;
                errfile=`echo ${filebase}".err"`;
                ovsdb-benchmark ${test_name} -n${tests[$i]} -m${pool[$i]} -c 1>${logfile} 2>${errfile};
                clean_test ${test_table};
            else
                filebase=`echo ${test_folder}"/n"${tests[$i]}"_m"${pool[$i]}`;
                logfile=`echo ${filebase}".csv"`;
                errfile=`echo ${filebase}".err"`;
                ovsdb-benchmark ${test_name} -n${tests[$i]} -m${pool[$i]} 1>${logfile} 2>${errfile};
                clean_test ${test_table};
            fi
        done
    else
        echo ${testname}"test failed: arguments size not correct"
    fi
}

# Function to perform row insertion and collect statistics related to
# memory usage.
# tests[] array should be modified for the desired tests to log.
# i-th test parameters are the i-th element on each of the configuration
# arrays
memory_usage(){

    test_name="memory-usage"
    test_table="Test"
    clean_test ${test_table};
    test_folder="tests/"${test_name}
    mkdir -p ${test_folder}

    tests=('500000')

    for (( i=0; i<${#tests[@]}; i++))
    do
        if [ "${1}" == "-c" ];
        then
            filebase=`echo ${test_folder}"/n"${tests[$i]}"_nocache"`;
            logfile=`echo ${filebase}".csv"`;
            errfile=`echo ${filebase}".err"`;
            ovsdb-benchmark ${test_name} -n${tests[$i]} -c 1>${logfile} 2>${errfile};
            clean_test ${test_table}
        else
            filebase=`echo ${test_folder}"/n"${tests[$i]}`;
            logfile=`echo ${filebase}".csv"`;
            errfile=`echo ${filebase}".err"`;
            ovsdb-benchmark ${test_name} -n${tests[$i]} 1>${logfile} 2>${errfile};
            clean_test ${test_table}
        fi
    done
}

# Function to perform row insertion and collect statistics related to
# memory usage.
# tests[] array should be modified for the desired tests to log.
# i-th test parameters are the i-th element on each of the configuration
# arrays
#
# In this particular test the same tests are run using one and multiple
# transactions
transaction_size(){

    test_name="transaction-size"
    test_table="Test"
    clean_test ${test_table};
    test_folder="tests/"${test_name}
    mkdir -p ${test_folder}

    tests=('1' '10' '100' '1000' '10000' '20000' '30000' '40000' \
           '50000' '60000' '70000' '80000' '90000' '100000' '200000' \
           '300000' '400000' '500000')

    for (( i=0; i<${#tests[@]}; i++))
    do
        if [ "${1}" == "-c" ];
        then
            # run the test using multiple transactions
            filebase=`echo ${test_folder}"/n"${tests[$i]}"_nocache"`;
            logfile=`echo ${filebase}".csv"`;
            errfile=`echo ${filebase}".err"`
            ovsdb-benchmark ${test_name} -n${tests[$i]} ${single_transaction} -c 1>${logfile} 2>${errfile};
            clean_test ${test_table};

            # repeat each test using single transaction
            filebase=`echo ${test_folder}"/n"${tests[$i]}"_single_nocache"`;
            logfile=`echo ${filebase}".csv"`;
            errfile=`echo ${filebase}".err"`
            ovsdb-benchmark ${test_name} -n${tests[$i]} ${single_transaction} -s -c 1>${logfile} 2>${errfile};
            clean_test ${test_table};
        else
            # run the test using multiple transactions
            filebase=`echo ${test_folder}"/n"${tests[$i]}`;
            logfile=`echo ${filebase}".csv"`;
            errfile=`echo ${filebase}".err"`
            ovsdb-benchmark ${test_name} -n${tests[$i]} ${single_transaction} 1>${logfile} 2>${errfile};
            clean_test ${test_table};

            # repeat each test using single transaction
            filebase=`echo ${test_folder}"/n"${tests[$i]}"_single"`;
            logfile=`echo ${filebase}".csv"`;
            errfile=`echo ${filebase}".err"`
            ovsdb-benchmark ${test_name} -n${tests[$i]} ${single_transaction} -s 1>${logfile} 2>${errfile};
            clean_test ${test_table};
        fi
    done
}

# Function to perform row update test
# tests[] and workers[] arrays should be modified for the desired
# tests to log.
# i-th test parameters are the i-th element on each of the configuration
# arrays
update(){

    test_name="update"
    test_table="Test"
    clean_test ${test_table};
    test_folder="tests/"${test_name}
    mkdir -p ${test_folder}

    tests=('10000')
    workers=('100')
    pool=('100')

    if [ ${#tests[@]} -eq ${#workers[@]} ];
    then
        for (( i=0; i<${#tests[@]}; i++))
        do
            if [ "${1}" == "-c" ];
            then
                filebase=`echo ${test_folder}"/n"${tests[$i]}"_w"${workers[$i]}"_m"${pool[$i]}"_nocache"`;
                logfile=`echo ${filebase}".csv"`;
                errfile=`echo ${filebase}".err"`
                ovsdb-benchmark ${test_name} -n${tests[$i]} -w${workers[$i]} -m${pool[$i]} -c 1>${logfile} 2>${errfile};
                clean_test ${test_table};
            else
                filebase=`echo ${test_folder}"/n"${tests[$i]}"_w"${workers[$i]}"_m"${pool[$i]}`;
                logfile=`echo ${filebase}".csv"`;
                errfile=`echo ${filebase}".err"`
                ovsdb-benchmark ${test_name} -n${tests[$i]} -w${workers[$i]} -m${pool[$i]} 1>${logfile} 2>${errfile};
                clean_test ${test_table};
            fi
        done
    else
        echo ${testname}"test failed: arguments size not correct"
    fi
}

# Function to perform publisher/subscriber test
# tests[] and workers[] arrays should be modified for the desired
# tests to log.
# delay: delay time in microseconds
# i-th test parameters are the i-th element on each of the configuration
# arrays
pubsub(){

    test_name="pubsub"
    test_table="Test"
    clean_test ${test_table}
    test_folder="tests/"${test_name};
    mkdir -p ${test_folder}

    tests=('5000' '5000' '5000' '5000' '5000' '5000' '5000' '5000' '5000' '5000' '5000')
    workers=( '1'    '5'   '10'   '15'   '20'   '25'   '30'   '35'   '40'   '43'   '46')
    delay=('0' '5000' '10000');

    if [ ${#tests[@]} -eq ${#workers[@]} ];
    then
        for (( k=0; k<${#delay[@]}; k++))
        do
            for (( i=0; i<${#tests[@]}; i++))
            do
                filebase=`echo ${test_folder}"/n"${tests[$i]}"_w"${workers[$i]}"_d"${delay[$k]}`;
                logfile=`echo ${filebase}".csv"`;
                errfile=`echo ${filebase}".err"`
                ovsdb-benchmark ${test_name} -n${tests[$i]} -w${workers[$i]} -d${delay[$k]} 1>${logfile} 2>${errfile};
                clean_test ${test_table};
            done
        done
    else
        echo ${testname}"test failed: arguments sizes not correct"
    fi
}

# Function to perform the publisher/subscriber counter retrieval
# tests[] and workers[] arrays should be modified for the desired
# tests to log
# delay: delay time in microseconds
# i-th test parameters are the i-th element on each of the configuration
# arrays
counters(){

    test_name="counters";
    clean_test "Test";
    clean_test "TestCounters";
    clean_test "TestCountersRequests";
    test_folder="tests/"${test_name}
    mkdir -p ${test_folder}

    tests=('1000' '1000' '10000' '10000' '50000' '50000' '100000' '100000' '1000' '100')
    workers=( '1'    '4'     '1'     '4'     '1'     '4'      '1'      '4'    '4'   '8')
    pool=(  '512'  '512'   '512'   '512'   '512'   '512'    '512'    '512' '2000' '256')
    delay=('10000');

    if [ ${#tests[@]} -eq ${#workers[@]} ] && [ ${#tests[@]} -eq ${#pool[@]} ];
    then
        for (( k=0; k<${#delay[@]}; k++))
        do
            for (( i=0; i<${#tests[@]}; i++))
            do
                filebase=`echo ${test_folder}"/n"${tests[$i]}"_w"${workers[$i]}"_m"${pool[$i]}"_d"${delay[$k]}`;
                logfile=`echo ${filebase}".csv"`;
                errfile=`echo ${filebase}".err"`
                ovsdb-benchmark ${test_name} -n${tests[$i]} -w${workers[$i]} -m${pool[$i]} -d${delay[$k]} 1>${logfile} 2>${errfile};
                clean_test "TestCounters";
                clean_test "TestCountersRequests";
            done
        done
    else
        echo ${testname}"test failed: arguments size not correct"
    fi
}

echo "##################################################################"
echo "### OVSDB Benchmark Tool ###"
echo ""

# clean test tables
clean_test "Test";
clean_test "TestCounters";
clean_test "TestCountersRequests";

cache_disable="-c";

# Insert test
insert ""
insert ${cache_disable}

# Insert reading test
insert_index ""
insert_index ${cache_disable}

# Insert indexed test
insert_without_index ""
insert_without_index ${cache_disable}

# Transaction size
transaction_size ""
transaction_size ${cache_disable}

# Update rows
update ""
update ${cache_disable}

# Memory usage test.
# Activate these tests if required and set the correct value before
# uncommenting this test, it could hang your machine!
#memory_usage ""

# Publisher/subscriber test
pubsub ""

# Publisher/subscriber counters test
counters ""

# clean output files
clean_files

# restart OVSDB server
systemctl restart ovsdb-server;
