# README

The Benchmark module contains tests that exercises and measures several
components of OpenSwitch.

At this moment, Benchmark module contains tests for:

- OVSDB

The benchmark module can be used from the CLI, or using the website, if
enabled. The web interface displays the results as a charts, with its
correspondant information.

## OVSDB Benchmark

It contains several tests to measure the OVSDB Server and C IDL behavior
in some specific scenarios. The available tests are:

- Insertion
- Insertion using indexes
- Insertion without indexes
- Transaction size
- Update
- Memory usage
- PubSub
- Counters

The benchmark process collects different statistics about the system
performance while running the test. The collected statistics will
depend on the test nature, and measure factors like:
responses timing (min, max, avg per second), resulting status of the
requests, CPU usage and RAM usage.

###  Insertion

Each W workers write N rows to the OVSDB. The delay between requests
and the usage of the cache can be tuned. This test collects statistics
after each row has been added to the table.

### Insertion without indexes

This test is used to measure the time required to insert and read each
row added to the table.

### Insertion using indexes

This test is used to measure the time required to insert and read each
row added to the table, given that each row is indexed.

###  Memory usage

Inserts N records to OVSDB, measuring the RAM usage between each
insertion.

###  Transaction size

Tests the timing between writing a single transaction with a lot of
records, or writing a lot of small transactions of a single record.

###  Update

Each W workers updates one of M rows (at random) to the OVSDB N times.
The delay between requests and the usage of the cache can be tuned.

###  PubSub

The test has one producer (writes timestamps) and W subscribers, that
register the elapsed time between the writing timestamp and the read
timestamp.

###  Counters

Measures the time between a subscriber requests a counter update and
the same subscriber reads the response.

The test includes several trips from IDL to OVSDB.


## OVSDB Benchmark code download and build

Download OpenSwitch as usual. Remove all the cloned repos in `src/`,
using `make devenv_rm`.

Checkout the `feature/benchmark` in ops-build, and clone the required repos:

```
cd yocto
git checkout feature/benchmark
cd -
make devenv_add ops-benchmark
make devenv_add ops
make devenv_add ops-openvswitch
```

Check that the repostory ops is in the branch `feature/benchmark`. That should
have happen automatically, but it not then checkout that branch.

Then build the image as usual.

## Export image and install benchmark
```
make export_docker_image ops
docker run --privileged -v /tmp:/tmp -v /dev/log:/dev/log -v \
/sys/fs/cgroup:/sys/fs/cgroup -h h1 --name h1 ops /sbin/init  &
```
Enter the switch terminal using
```
docker exec -ti h1 bash
```
If it is requierd to install the benchmark tool binaries, in another
terminal, get docker IP:
```
docker inspect --format '{{ .NetworkSettings.IPAddress }}' h1
```
and install the benchmark:
```
make benchmark-deploy root@<Docker IP>
```

It may be required to remove an older SSH key in case another virtual
switch has been previously loaded using the same IP
```
ssh-keygen -f "/home/<user>/.ssh/known_hosts" -R <Docker IP>
```
---

## How to use the benchmark tool

The usage of the benchmarking tool is given by the following command:
```
ovsdb-benchmark [-h] [method] [-n #] [-w #] [-c #] [-m #] [-s] \
                [-o path] [-p path]
```

Methods:
```
| Method                | Description

'insert'                Inserts -n rows using -w parallel workers
'insert-index'          Inserts rows using indexed columns. -n
                          transactions of -m operations(rows)
'insert-without-index'  Inserts rows. -n transactions of -m operations
                          (rows) one big transaction
'memory-usage'          Inserts -n rows, monitoring RAM usage
'transaction-size'      Insert -n rows, using -n requests or 1 (if -s)
'update'                Updates -m records, -n times per worker.
'pubsub'                Measures the time between publishing and
                          receiving data
'counters'              Measures the time between requesting counters
                          and retrieving them
```

The meaning of the options on the command are:
```
| Option  | Description

-n        Number of operations. Apply for all tests
-w        Number of workers. Apply for insert, update and pubsub
            operations
-m        Size of the pool of records. Apply for insert-index,
            insert-without-index and counters.
-s        Single transaction. Apply for transaction-size
-o        Path to OVSDB PID file. Apply to all
-p        Path to OVSDB socket. Apply to all
-c        Disable the cache. By default the tests configure the IDL to
            synchronize the changes to the Test table, this parameter
            disables it. Does not apply for pubsub and counters
```

### Usage examples

Get help for a particular test module it is required to use the ‘-h’
options and the module name as shown:
```
ovsdb-benchmark -h ‘memory-usage’
```
Usage examples for the different tests are shown below
```
ovsdb-benchmark insert -n100 -w3
ovsdb-benchmark insert-index -n100 -m10
ovsdb-benchmark insert-without-index -n100 -m10
ovsdb-benchmark memory-usage -n10
ovsdb-benchmark transaction-size -n100 -s
ovsdb-benchmark update -n100 -w3
ovsdb-benchmark pubsub -n10 -w10
ovsdb-benchmark counters -w10 -n3 -m5
```
    -c option can be used with any test to disable the cache

if -p option is not specified the file used for the database socket is:
```
/var/run/openvswitch/db.sock
```

### Other useful benchmark commands

The commands presented here are not always necessary and are primarily
needed as miscellaneous tools.

Delete the database
```
ovsdb-client  transact '["OpenSwitch", {"op":"delete","table":"Test","where":[]},{"op":"commit","durable":true}]'
```

Compact the database
```
ovs-appctl -t  ovsdb-server ovsdb-server/compact
```

Restart OVSDB server. Useful after a memory intensive test.
```
systemctl restart ovsdb-server
```
