/* Copyright (C) 2015 Hewlett Packard Enterprise Development LP
 * All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 * @file
 * Provides access to functions and definitions needed for measuring the CPU
 * and memory usage of a given process.
 */

#ifndef OVSDB_BENCHMARK_PROCESS_USAGE_H_
#define OVSDB_BENCHMARK_PROCESS_USAGE_H_

/** Path to the OVSDB server pid file */
#define OVSDB_SERVER_PID_FILE_PATH "/var/run/openvswitch/ovsdb-server.pid"

/** Flag for reading the stats from the current program */
#define SELF_STATS -9999

/* Contains process execution information. */
struct process_stats {
    unsigned long long utime;   /* User-mode CPU time */
    unsigned long long stime;   /* Kernel-mode CPU time */
    unsigned long long cutime;  /* Cumulative utime of proc and children */
    unsigned long long cstime;  /* Cumulative stime of proc and children */
    unsigned long long start_time;      /* Start time since UNIX epoch
                                         * (seconds) */
    unsigned long int vsize;    /* Virtual memory size in bytes */
    long int rss;               /* Real memory size in bytes */
    unsigned long long total_time;      /* USER_HZ spent in user mode */
    int pid;                    /* PID of process */
    char state;                 /* State of process */
};

int pid_of(const char *);
int pid_from_file(const char *);
int get_usage(int pid, struct process_stats *);
void cpu_usage_pct(const struct process_stats *,
                   const struct process_stats *, double *, double *);

#endif /* OVSDB_BENCHMARK_PROCESS_USAGE_H_ */
