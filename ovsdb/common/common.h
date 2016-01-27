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
 * Provides access to common functionality and definitions used in several
 * tests.
 */

#ifndef OVSDB_BENCHMARK_COMMON_H_
#define OVSDB_BENCHMARK_COMMON_H_

#include "vswitch-idl.h"
#include "process_usage.h"

/**
 * Tests supported by benchmark
 */
enum benchmark_method {
    METHOD_NONE,
    METHOD_INSERT,
    METHOD_INSERT_MEMORY_USAGE,
    METHOD_INSERTION_TIME,
    METHOD_INSERTION_INDEX_TIME,
    METHOD_TRANSACTION_SIZE,
    METHOD_UPDATE,
    METHOD_PUBSUB,
    METHOD_COUNTERS
};

/**
 * Contains all the information that is measured in the test
*/
struct sample_data {
    uint64_t start_time;
    uint64_t end_time;
    int worker_id;
    int status;
    double user_cpu_pct;
    double system_cpu_pct;
    long unsigned int vsize;
    long int rss;
};

struct stats_data {
    int count;                  /* Total count of requests done (successful 54
                                 * * and fail) */
    int fail_count;             /* Failed requests */
    int current_epoch;          /* Current second, rounded to the floor */
    uint64_t min_duration;      /* Minimal duration in this second */
    uint64_t sum_duration;      /* Sum of the durations in this second */
    uint64_t max_duration;      /* Max duration in this secod */
    double user_cpu_pct;        /* User CPU% usage of ovsdb */
    double system_cpu_pct;      /* System CPU% usage of ovsdb */
    long unsigned int vsize;    /* virtual memory size in bytes of ovsdb */
    long unsigned int rss;      /* resident memory size in bytes of ovsdb */
};


struct local_stats_data {
    int         iteration;      /* Number of iteration whose results are related to */
    int         rows_to_insert; /* Number of rows that are going to be inserted by iteration */
    int         rows_read;      /* Rows read after inserts */
    uint64_t    read_time;      /* Time taken to read all rows in the iteration */
    uint64_t    write_time;     /* Time taken to write the rows in the iteration */
    bool        succeded;       /* Result of iteration. True if succeded, otherwise False */
};


/**
 *  Contains the configuration for one benchmark
 */
struct benchmark_config {
    enum benchmark_method method;       /* Test to be executed */
    uint64_t test_start_time;   /* Start time in microseconds of the test */
    uint64_t test_end_time;     /* End time in microseconds of the test */
    int responses_stats_shm_id; /* ID of the shared memory for saving the
                                 * stats */
    int workers;                /* Total of workers */
    int total_requests;         /* Total of requests */
    int pool_size;              /* Size of the pool of records, when doing
                                 * updates */
    bool single_transaction;    /* A big transaction or a lot of small
                                 * transactions */
    bool enable_cache;          /* Specifies if the program uses the IDL cache
                                 * (uses more RAM) or not */
    int delay;                  /* Delay in usecs between publications */
    uint64_t time_window;       /* Duration between CPU/RAM measuring */
    char *ovsdb_pid_path;       /* Path to the file with the pid of OVSDB */
    char *ovsdb_socket;         /* Connection path to OVSDB */

    struct uuid *record_pool;   /* Array with rows previously created */
    int argc;                   /* Number of arguments to the program */
    char **argv;                /* Arguments to the program */
};

/**
 * Don't group the results by returned value
 */
#define ALL_STATUSES -1

void print_usage(char *);
void exit_with_error_message(char *, const struct benchmark_config *);
void init_benchmark(struct benchmark_config *config);
void destroy_benchmark(const struct benchmark_config *config);
void fill_test_row(const struct ovsrec_test *, long num);
void fill_test_index_row(const struct ovsrec_testindex* p_rec, int p_num);
uint64_t microseconds_now(void);
void swap_stats(struct process_stats *, struct process_stats *);
void reset_stats(struct stats_data *);
void print_stats(struct benchmark_config *, struct sample_data *, int);
void print_epoch_stats(const struct stats_data *, int);
void update_stats(struct stats_data *, struct sample_data *);
void print_stats_header(void);
void idl_init(struct ovsdb_idl **, const struct benchmark_config *);
bool idl_default_initialization(struct ovsdb_idl **, const struct benchmark_config *);
void ovsdb_idl_wait_for_replica_synched (struct ovsdb_idl*);
int compare_responses(const void *a, const void *b);
bool valid_response(const struct sample_data *response);
void print_usage(char *binary_name);
int clear_table(char* name);

#endif /* OVSDB_BENCHMARK_COMMON_H_ */
