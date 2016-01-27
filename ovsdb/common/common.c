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

#include <limits.h>
#include <sys/time.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "process_usage.h"

#include "ovsdb-idl.h"
#include "vswitch-idl.h"
#include "dirs.h"
#include "poll-loop.h"

/**
 * @brief Initializes a struct benchmark_config to
 * safe defaults, ready to be filled with the input
 * of user.
 * @param [int] pointer to a benchmark_config struct.
 */
void
init_benchmark(struct benchmark_config *config)
{
    char *remote = xasprintf("unix:%s/db.sock", ovs_rundir());
    char *pidpath = xasprintf("%s", OVSDB_SERVER_PID_FILE_PATH);

    config->ovsdb_socket = remote;      /* Default OVSDB socket */
    config->ovsdb_pid_path = pidpath;   /* Default OVSDB PID file */
    config->method = METHOD_NONE;       /* By default do nothing */
    config->workers = -1;       /* Worker is a mandatory argument */
    config->total_requests = -1;        /* Requests is a mandatory argument */
    config->pool_size = -1;     /* Record pool size is mandatory */
    config->single_transaction = false; /* Tiny transactions by default */
    config->time_window = 1000000;      /* One second */
    config->enable_cache = true;        /* Turn on cache by default */
    config->delay = 0;          /* 0 delay by default */
    config->record_pool = NULL; /* None by default */
}

/**
 * @brief Frees the resources allocated in a struct benchmark_config
 * @param [int] pointer to a benchmark_config struct.
 */
void
destroy_benchmark(const struct benchmark_config *config)
{
    free(config->ovsdb_socket);
    free(config->ovsdb_pid_path);
    if (config->record_pool) {
        free(config->record_pool);
    }
    return;
}

/**
 * @brief Fill a ovsrec_test structure with dummy fixed data.
 * This functions fills a ovsrec_test structured fields.
 * This procedure is useful when we want data to be written in
 * the database but, we are do not care about the data itself.
 * Note: this only works if all the fields of the table can be
 * equals, i.e. they are not keys.
 *
 * @param [out] rec pointer to the structure that will be filled.
*/
void
fill_test_row(const struct ovsrec_test *rec, long num)
{
    ovsrec_test_set_stringField(rec, "row");
    ovsrec_test_set_numericField(rec, num);
    ovsrec_test_set_enumField(rec, OVSREC_TEST_ENUMFIELD_ENUMVALUE1);
    ovsrec_test_set_boolField(rec, true);
}

/**
 * @brief Fill a ovsrec_testindex structure with dummy fixed data.
 * This functions fills a ovsrec_testindex structured fields.
 * This procedure is useful when we want data to be written in
 * the database are do not care about the data itself.
 *
 * @param [in] p_num is the unique number index to assign the index column.
 * @param [out] rec pointer to the structure that will be filled.
*/
void
fill_test_index_row(const struct ovsrec_testindex* p_rec, int p_num)
{
    char chr[20] = "";
    sprintf(chr, "row %d", p_num);
    ovsrec_testindex_set_stringField(p_rec, chr);
    ovsrec_testindex_set_numericField(p_rec, p_num);
    ovsrec_testindex_set_enumField(p_rec, OVSREC_TESTINDEX_ENUMFIELD_ENUMVALUE1);
    ovsrec_testindex_set_boolField(p_rec, true);
}


/**
 * Get time in microseconds
 */
uint64_t
microseconds_now(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000ull + tv.tv_usec;
}

/**
 * Swap statistics structure to ensure the last statistics are the
 * initial for the following test
 *
 * @param [in,out] a is a pointer to the first statistics structure
 * @param [in,out] b is a pointer to the first statistics structure
 */
void
swap_stats(struct process_stats *a, struct process_stats *b)
{
    struct process_stats *c;

    c = a;
    a = b;
    b = c;
}

/**
 * Set default statistics values
 *
 * @param [in,out] stats is a pointer to the statistics structure
 */
void
reset_stats(struct stats_data *stats)
{
    stats->count = 0;
    stats->fail_count = 0;
    stats->sum_duration = 0;
    stats->min_duration = UINT64_MAX;
    stats->max_duration = 0;
}

/**
 * Update the total statistics structure using the data from the last test
 *
 * @param [in,out] stats is a pointer to the overall statistics structure
 * @param [in] response is a pointer to the current statistics structure
 */
void
update_stats(struct stats_data *stats, struct sample_data *response)
{
    uint64_t duration;

    duration = response->end_time - response->start_time;
    stats->count++;
    stats->sum_duration += duration;
    stats->min_duration =
        stats->min_duration < duration ? stats->min_duration : duration;
    stats->max_duration =
        duration > stats->max_duration ? duration : stats->max_duration;
    if (response->status != TXN_SUCCESS) {
        stats->fail_count++;
    }
}

/**
 * @brief Prints the summary, grouped by returned value, of all the requests
 * performed.
 *
 * @param [in] config is a pointer to the configuration structure
 * @param [in] responses is a pointer to the test results
 * @param [in] group_by defines which IDL response status to print. Use 0 - 7
 *             to print a specific status or ALL_STATUSES to prit all the
 *             results
 */
void
print_stats(struct benchmark_config *config,
            struct sample_data *responses, int group_by)
{
    struct stats_data stats;
    uint64_t total_requests = config->workers * config->total_requests;
    int response_epoch;
    bool has_pending_stats = false;

    /* Sort all the responses by end time */
    qsort(responses, total_requests, sizeof (struct sample_data),
          compare_responses);

    /* Do the summary of the data */
    reset_stats(&stats);
    stats.current_epoch = 0;

    /* The stats are separated by config.time_window (e.g. one second) */
    for (int i = 0; i < total_requests; ++i) {
        if (valid_response(&responses[i]) &&
            ((group_by == ALL_STATUSES) || responses[i].status == group_by)) {
            has_pending_stats = true;
            response_epoch = responses[i].end_time / config->time_window;
            if (responses[i].vsize > 0) {
                stats.user_cpu_pct = responses[i].user_cpu_pct;
                stats.system_cpu_pct = responses[i].system_cpu_pct;
                stats.vsize = responses[i].vsize;
                stats.rss = responses[i].rss;
            }
            if (response_epoch != stats.current_epoch) {
                print_epoch_stats(&stats, group_by);
                has_pending_stats = false;
                reset_stats(&stats);
                stats.current_epoch = response_epoch;
            }

            /* Updates stats */
            update_stats(&stats, &responses[i]);
        }
    }
    if (has_pending_stats) {
        print_epoch_stats(&stats, group_by);
    }
}

/**
 * Prints the epoch (second) stats for some worker
 *
 * @param [in] stats is a pointer to the statistics structure
 * @param [in] group_by defines the blocks of results to be displayed
 */
void
print_epoch_stats(const struct stats_data *stats, int groupby)
{
    if (stats->count > 0) {
        printf("%d,%d,"
               "%d,%d,"
               "%" PRIu64 ",%" PRIu64 ",%" PRIu64 ","
               "%lf,"
               "%lf,%lf,"
               "%lu,%ld\n", groupby, stats->current_epoch, stats->count,
               stats->fail_count, stats->min_duration, stats->max_duration,
               stats->sum_duration,
               (double) (stats->sum_duration) / (double) (stats->count),
               stats->user_cpu_pct, stats->system_cpu_pct, stats->vsize,
               stats->rss);
    }
}

/**
 * Prints the statistics header
 */
void
print_stats_header(void)
{
    printf("status,second,"
           "count,"
           "fail_count,"
           "min_duration,max_duration,duration_sum,"
           "avg_duration," "ucpu,scpu," "vsize,rss" "\n");
}

/**
 * @brief Initialize the IDL and register the require tables
 * and columns.
 * It allows to disable the cache usage, and to set the remote path.
 * @param [in/out] Pointer to save a new IDL
 * @param [in] Pointer to benchmark configuration
 */
void
idl_init(struct ovsdb_idl **idl, const struct benchmark_config *config)
{
    *idl = ovsdb_idl_create(config->ovsdb_socket, &ovsrec_idl_class, true, true);
    if (config->enable_cache) {
        ovsdb_idl_add_table(*idl, &ovsrec_table_test);
        ovsdb_idl_add_column(*idl, &ovsrec_test_col_stringField);
        ovsdb_idl_add_column(*idl, &ovsrec_test_col_numericField);
        ovsdb_idl_add_column(*idl, &ovsrec_test_col_enumField);
        ovsdb_idl_add_column(*idl, &ovsrec_test_col_boolField);
        if(config->method == METHOD_COUNTERS)
        {
            ovsdb_idl_add_table(*idl, &ovsrec_table_testcounters);
            ovsdb_idl_add_column(*idl, &ovsrec_testcounters_col_countersField);
            ovsdb_idl_add_column(*idl, &ovsrec_testcounters_col_stringField);
            ovsdb_idl_add_column(*idl, &ovsrec_testcounters_col_numericField);

            ovsdb_idl_add_table(*idl, &ovsrec_table_testcountersrequests);
            ovsdb_idl_add_column(*idl, &ovsrec_testcountersrequests_col_stringField);
            ovsdb_idl_add_column(*idl, &ovsrec_testcountersrequests_col_requestTimestamp);
            ovsdb_idl_add_column(*idl, &ovsrec_testcountersrequests_col_responseTimestamp);
        }
    }
}

/**
 * Initialize the IDL with the default arguments, and returns if
 * the initialization was sucessful or not.
 *
 * @param [in/out] Pointer to the new IDL
 * @param [in] Pointer to benchmark configuration
 */
bool
idl_default_initialization(struct ovsdb_idl **idl,
                           const struct benchmark_config *config)
{
    /* Initializes IDL */
    ovsrec_init();
    idl_init(idl, config);
    ovsdb_idl_run(*idl);
    /* Checks if the IDL is alive */
    return ovsdb_idl_is_alive(*idl);
}

/**
 * Run ovsdb_idl_run until the IDL has loaded all the data saved at
 * OVSDB, at the moment of the IDL creation.
 *
 * @param [in/out] Pointer to IDL structure
 */
void
ovsdb_idl_wait_for_replica_synched(struct ovsdb_idl *idl)
{
    ovsdb_idl_run(idl);
    while (!ovsdb_idl_has_ever_connected(idl)) {
        ovsdb_idl_wait(idl);
        poll_block();
        ovsdb_idl_run(idl);
    }
}

/**
 * Compare two struct sample_data, by end_time. This function is used
 * with qsort to sort in ascending time.
 *
 * @param [in] a is pointer to test data structure
 * @param [in] b is pointer to test data structure
 */
int
compare_responses(const void *a, const void *b)
{
    const struct sample_data *pa = (const struct sample_data *) a;
    const struct sample_data *pb = (const struct sample_data *) b;

    return pa->end_time - pb->end_time;
}

/**
 * Returns if a response is indeed a response or a zeroed struct. All the
 * workers ID are > 1, then if that value is 0 then the request hasn't been
 * filled.
 *
 * @param [in] response is pointer to test data structure
 */
bool
valid_response(const struct sample_data * response)
{
    return response->worker_id != 0;
}

/**
 * Prints error messages to stderr and exits
 *
 * @param [in] msg is the message to be displayed
 * @param [in] configuration is the test configuration structure
 */
void
exit_with_error_message(char *msg,
                        const struct benchmark_config *configuration)
{
    fprintf(stderr, msg, configuration->argv[optind]);
    print_usage(configuration->argv[0]);
    destroy_benchmark(configuration);
    exit(1);
}

/**
 * Prints main help for benchmark tool
 *
 * @param [in/out] binary_name is the name of the benchmark binary
 */
void
print_usage(char *binary_name)
{
    printf("Usage:\n"
           "%s [-h] [method] [-n #] [-w #] [-c #] [-m #] [-s] [-o path] [-p path] \n"
           "-h\tShows help. With -m prints specific help for method.\n"
           "method\tSpecifies the test to be performed. Can be:\n"
           "  \t'insert': Inserts -n rows using -w parallel workers.\n"
           "  \t'memory-usage': Inserts -n rows, monitoring RAM usage.\n"
           "  \t'transaction-size': Insert -n rows, using -n requests or 1 (if -s)\n"
           "  \t'update': Updates -m records, -n times per worker\n."
           "  \t'insert-index': Inserts rows using indexed columns. -n transactions of -m operations(rows)\n"
           "  \t'insert-without-index': Inserts rows. -n transactions of -m operations(rows)\n"
           " one big transaction.\n"
           "  \t'pubsub': Measures the time between publishing and receiving data\n"
           "  \t'counters': Measures the time between requesting counters and retrieving them\n"
           "-n\tNumber of operations. Apply for all tests.\n"
           "-w\tNumber of workers. Apply for insert.\n"
           "-m\tSize of the pool of records. Apply for update.\n"
           "-s\tSingle transaction. Apply for single-transaction.\n"
           "-c\tDisable the cache. By default the tests configure the IDL to "
           "synchronize the changes to the Test table, this parameter disables"
           " it. Apply to all.\n"
           "-o\tPath to OVSDB PID file. Apply to all.\n"
           "-p\tPath to OVSDB socket. Apply to all.\n", binary_name);
}

/**
 * Clear all the test tables. It iterates on each table to delete each row
 * until the table is empty.
 *
 * @param [in/out] benchmark test configuration
 */
int
clear_test_tables(struct benchmark_config *config)
{
    struct ovsdb_idl *idl;
    struct ovsdb_idl_txn *txn;
    const struct ovsrec_test *ovs_test;
    const struct ovsrec_testcounters *ovs_testcounters;
    const struct ovsrec_testcountersrequests *ovs_testcountersrequests;
    const struct ovsrec_testindex *ovs_testindex;
    int status;

    /* Initializes IDL */
    if (!idl_default_initialization(&idl, config)) {
        perror("Can't initialize IDL. ABORTING");
        exit(-1);
    }
    ovsdb_idl_wait_for_replica_synched(idl);
    ovsdb_idl_run(idl);

    txn = ovsdb_idl_txn_create(idl);

    /* Delete all tables row by row */
    OVSREC_TEST_FOR_EACH(ovs_test, idl) {
        ovsrec_test_delete(ovs_test);
    }
    OVSREC_TESTCOUNTERS_FOR_EACH(ovs_testcounters, idl) {
        ovsrec_testcounters_delete(ovs_testcounters);
    }
    OVSREC_TESTCOUNTERSREQUESTS_FOR_EACH(ovs_testcountersrequests, idl) {
        ovsrec_testcountersrequests_delete(ovs_testcountersrequests);
    }
    OVSREC_TESTINDEX_FOR_EACH(ovs_testindex, idl) {
        ovsrec_testindex_delete(ovs_testindex);
    }

    /* Commit transaction */
    status = ovsdb_idl_txn_commit_block(txn);
    ovsdb_idl_txn_destroy(txn);

    if (status != TXN_SUCCESS && status != TXN_UNCHANGED) {
        exit_with_error_message("Can't delete existing test tables. Aborting",
                                config);
        exit(-1);
    }

    ovsdb_idl_destroy(idl);
    return status;
}
