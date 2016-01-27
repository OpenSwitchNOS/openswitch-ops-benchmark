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
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "process_usage.h"

/**
 * @brief Gets pid from file.
 * Retrieves a process id from a pidfile.
 * @param [in] path location of the pidfile to read.
 * @return process id of the process or -1 if an error occured.
 */
int
pid_from_file(const char *const path)
{
    FILE *fd;
    int pid;

    fd = fopen(path, "r");
    fscanf(fd, "%d", &pid);

    if (pclose(fd) != 0) {
        return -1;
    }

    return pid;
}

/**
 * Obtains the PID of a process
 *
 * @param [in] process name
 */
int
pid_of(const char *process_name)
{
    FILE *fd;
    char *command;
    int pid;

    asprintf(&command, "pidof %s", process_name);
    fd = popen(command, "r");
    free(command);
    fscanf(fd, "%d", &pid);
    if (0 != pclose(fd)) {
        return -1;
    }
    return pid;
}

/**
 * @brief Gets the stats for a process using its PID.
 * If PID is SELF_STATS, the stats of the current process are read
 * from /proc/self/stat. For any other PID the stats will be read
 * from /proc/<pid>/stat.
 *
 * For invalid pids, i.e. pids < 0, all the stats will be returned with
 * zeroes as values.
 *
 * @param pid [in] process id
 * @param stats [out] structure where the results
 * will be written.
 * @return  0 on success, non 0 on error.
 */
int
get_usage(int pid, struct process_stats *stats)
{
    FILE *fd;
    char *path;

    if (pid == SELF_STATS) {
        asprintf(&path, "/proc/self/stat");
    } else if (pid >= 0) {
        asprintf(&path, "/proc/%d/stat", pid);
    } else {
        /* Invalid PID. Return zeroed struct */
        memset(stats, 0, sizeof (struct process_stats));
        stats->pid = pid;
        return -1;
    }

    if ((fd = fopen(path, "r")) == NULL) {
        free(path);
        perror("Can't open /proc/pid/stats");
        return -1;
    }

    free(path);
    fscanf(fd, "%d %*s %c %*d %*d %*d %*d %*d %*u "
           "%*u %*u %*u %*u %llu %llu %lld %lld"
           "%*d %*d %*d %*d %llu %lu %ld",
           &stats->pid, &stats->state, &stats->utime, &stats->stime,
           &stats->cutime, &stats->cstime, &stats->start_time, &stats->vsize,
           &stats->rss);
    fclose(fd);
    stats->rss *= sysconf(_SC_PAGESIZE);
    path = "/proc/stat";
    if ((fd = fopen(path, "r")) == NULL) {
        perror("Can't open /proc/stat");
        return -1;
    }
    long unsigned int cpu_time[10];

    memset(&cpu_time[0], 0, sizeof (cpu_time));
    fscanf(fd, "%*s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", &cpu_time[0],
           &cpu_time[1], &cpu_time[2], &cpu_time[3], &cpu_time[4],
           &cpu_time[5], &cpu_time[6], &cpu_time[7], &cpu_time[8],
           &cpu_time[9]);
    stats->total_time = 0;
    for (int i = 0; i < 10; ++i) {
        stats->total_time += cpu_time[i];
    }
    fclose(fd);
    return 0;
}

/**
 * Obtains the percent of CPU in user and system mode for the
 * measured PID.
 *
 * @param [in] final is the final process statistics structure
 * @param [in] initial is the initial process statistics structure
 * @param [out] user_cpu is the CPU percent used for user space
 * @param [out] system_cpu is the CPU percent used for system space
 */
void
cpu_usage_pct(const struct process_stats *final,
              const struct process_stats *initial, double *user_cpu,
              double *system_cpu)
{
    const unsigned long long time = final->total_time - initial->total_time;

    if (time == 0) {
        *user_cpu = *system_cpu = 0.0;
        return;
    }
    const int numprocs = sysconf(_SC_NPROCESSORS_ONLN);

    *user_cpu = 100.0 * numprocs * ((final->cutime + final->utime)
                                    - (initial->cutime +
                                       initial->utime)) / (double) time;
    *system_cpu = 100.0 * numprocs * ((final->cstime + final->stime)
                                      - (initial->cstime +
                                         initial->stime)) / (double) time;
}
