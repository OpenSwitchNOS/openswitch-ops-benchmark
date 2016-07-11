/* Copyright (C) 2016 Hewlett Packard Enterprise Development LP
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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <util.h>

#include "common/common.h"
#include "common/metatest_workers.h"
#include "common/process_usage.h"

#include "ovsdb-idl.h"
#include "vswitch-idl.h"
#include "poll-loop.h"
#include "dirs.h"

/* Number of counters to insert in the counters map column. */
#define NUMBER_OF_COUNTERS 256

/* Prints help information about this test.
 * 'binary_name' is the name of the executable file. */
void
counters_waitmon_help(char *binary_name)
{
    printf("Test of subscription to counters using wait monitoring\n"
           "Usage: %s waitmon -n <number of updates>"
           " -w <number of workers> -m <number of rows> [-d delay] [-s]\n"
           "Do a counters requests, using several subscribers, and one\n"
           "producer.\n"
           "Each subscriber do <number of updates> requests the producer, \n"
           "and the producer responds with <number of rows> rows of counters."
           "\nIn the test also a delay between counters requests can be set,\n"
           "with a delay in microseconds.\n"
           "If receives the -s flag then the publisher don't modify the DB.\n",
           binary_name);
}

/* Write to the OVSDB a request to update the counters. */
static enum ovsdb_idl_txn_status
waitmon_make_counters_request(struct ovsdb_idl *idl)
{
    enum ovsdb_idl_txn_status status;
    struct ovsdb_idl_txn *txn;

    txn = ovsdb_idl_txn_create(idl);
    struct ovsdb_idl_txn_wait_unblock *wait_req;
    wait_req = ovsdb_idl_txn_create_wait_until_unblock(&ovsrec_table_test, 5000);
    ovsdb_idl_txn_wait_until_unblock_add_column(wait_req, &ovsrec_test_col_stringField);
    ovsdb_idl_txn_wait_until_unblock_add_row(wait_req, (struct ovsdb_idl_row*) ovsrec_test_first(idl));
    ovsdb_idl_txn_add_wait_until_unblock(txn, wait_req);
    /* Commit transaction */
    status = ovsdb_idl_txn_commit_block(txn);
    ovsdb_idl_txn_destroy(txn);
    return status;
}

/* Write the response time, the producer already write a valid response.
 * 'update_counters' specifies if the function must update the counters or not.
 * Updates the record of the current counters requests ('rec_request'). */
static enum ovsdb_idl_txn_status
waitmon_update_counters(struct ovsdb_idl *idl,
                        const struct ovsrec_test *rec_request)
{
    struct ovsdb_idl_txn *txn;
    enum ovsdb_idl_txn_status status;

    do {
        txn = ovsdb_idl_txn_create(idl);
        ovsrec_test_set_boolField(rec_request, true);
        ovsrec_test_set_stringField(rec_request, "published data");
        ovsrec_test_set_numericField(rec_request, nanoseconds_now());
        status = ovsdb_idl_txn_commit_block(txn);
        ovsdb_idl_txn_destroy(txn);
    } while (status != TXN_SUCCESS && status != TXN_UNCHANGED);

    return status;
}

/* Counters publisher/producer in this test. */
static void
waitmon_counters_producer(const struct benchmark_config *config)
{
    struct ovsdb_idl *idl;
    int i, count;
    struct ovsrec_test *row;

    /* Do initialization */
    if (!idl_default_initialization(&idl, config)) {
        perror("Can't initialize IDL. ABORTING");
        exit(-1);
    }

    enum ovsdb_idl_wait_monitor_status status = ovsdb_idl_wait_monitor_status(idl);
    struct ovsdb_idl_wait_monitor_request wait_req;
    ovsdb_idl_wait_monitor_create_txn(idl, &wait_req);
    ovsdb_idl_wait_monitor_add_column(&wait_req, &ovsrec_table_test, &ovsrec_test_col_stringField);
    ovsdb_idl_wait_monitor_add_column(&wait_req, &ovsrec_table_test, &ovsrec_test_col_numericField);
    ovsdb_idl_wait_monitor_add_column(&wait_req, &ovsrec_table_test, &ovsrec_test_col_boolField);
    if (!ovsdb_idl_wait_monitor_send_txn(&wait_req)) {
        printf("Aborting: wait monitor send txn failed\n");
        exit(-1);
    }
    while(ovsdb_idl_wait_monitor_status(idl) == status) {
        ovsdb_idl_wait(idl);
        poll_block();
    }

    ovsdb_idl_wait_for_replica_synched(idl);

    /* Start the workers */
    close(config->start_chan[1]);
    write(config->start_chan[0], "1", 1);

    /* This producer expects workers * total_requests requests */
    count = 0;
    i = 0;
    poll_immediate_wake_at(NULL);
    while (i < config->workers * config->total_requests) {
        ovsdb_idl_wait(idl);
        poll_block();
        ovsdb_idl_run(idl);
        struct ovsdb_idl_wait_update *req, *next;
        WAIT_UPDATE_FOR_EACH_SAFE(req, next, idl) {
            if (strcmp(req->table->class->name, "Test")) {
                printf("Expected Test, received \"%s\"\n", req->table->class->name);
            }
            for (int k = 0; k < req->rows_n; k++) {
                row = CONST_CAST(struct ovsrec_test *,
                                 ovsrec_test_get_for_uuid(idl, &req->rows[k]));
                if (!row) {
                    printf("Row "UUID_FMT" not found\n", UUID_ARGS(&req->rows[k]));
                } else {
                    if (!config->single_transaction) {
                        waitmon_update_counters(idl, row);
                    }
                }
            }
            ovsdb_idl_wait_unblock(idl, req);
            ovsdb_idl_wait_update_destroy(req);
            count++;
            i++;
        }
    }

    ovsdb_idl_destroy(idl);
}

/* The worker 'id' will try to read rows from the DB. The numericField is the
 * time (in microseconds) at row modification. With that the program can
 * calculate how many microseconds has passed since the last database
 * update. The resulting timings are saved in 'responses'. */
static void
waitmon_worker_for_counters_test(const struct benchmark_config *config, int id,
                                struct sample_data *responses)
{
    uint64_t epoch_time;
    int response_index;
    struct ovsdb_idl *idl;
    pid_t ovsdb_pid = pid_of_ovsdb(config);
    struct process_stats stat_a, stat_b;
    struct process_stats *p_initial_stat, *p_end_stat;
    uint64_t current_time;
    bool write_stats = false;

    p_initial_stat = &stat_a;
    p_end_stat = &stat_b;

    /* Initializes IDL */
    if (!idl_default_initialization(&idl, config)) {
        perror("Can't initialize IDL. ABORTING");
        exit(-1);
    }
    ovsdb_idl_wait_for_replica_synched(idl);

    epoch_time = nanoseconds_now() - config->test_start_time;
    get_usage(ovsdb_pid, p_initial_stat);

    /* The program is waiting for total_requests counters updates */
    for (response_index = 0;
         response_index < config->total_requests;
         ++response_index) {
        write_stats = true;
        /* Ask for a update on the counters table */
        current_time = nanoseconds_now() - config->test_start_time;
        responses[response_index].start_time = current_time;
        responses[response_index].status = waitmon_make_counters_request(idl);
        /* Read the counters */
        /* Do something */

        /* Collect statistics */
        responses[response_index].end_time = nanoseconds_now() -
                                             config->test_start_time;
        responses[response_index].worker_id = id;
        if (responses[response_index].end_time - epoch_time
            > config->time_window) {
            write_stats = false;
            epoch_time = responses[response_index].end_time;
            get_usage(ovsdb_pid, p_end_stat);
            cpu_usage_pct(p_end_stat, p_initial_stat,
                          &(responses[response_index].user_cpu_pct),
                          &(responses[response_index].system_cpu_pct));
            responses[response_index].vsize = p_end_stat->vsize;
            responses[response_index].rss = p_end_stat->rss;
            swap_stats(p_initial_stat, p_end_stat);
        }
        usleep(config->delay);
    }

    /* Saves the last usage */
    if (write_stats) {
        get_usage(ovsdb_pid, p_end_stat);
        cpu_usage_pct(p_end_stat, p_initial_stat,
                      &(responses[response_index].user_cpu_pct),
                      &(responses[response_index].system_cpu_pct));
        responses[response_index].vsize = p_end_stat->vsize;
        responses[response_index].rss = p_end_stat->rss;
    }

    /* Receives done notification, and stops the process */
    ovsdb_idl_destroy(idl);
}

/* Initializes the OVSDB test tables to perform this test. */
void
waitmon_initialize_counters_test(const struct benchmark_config *config)
{
    enum ovsdb_idl_txn_status retval;
    struct ovsdb_idl_txn *txn;
    struct ovsdb_idl *idl;
    const struct ovsrec_test *rec_counter;

    /* Clears the tes
     * t tables using ovsdb-client */
    clear_table("Test");

    /* Initializes IDL */
    if (!idl_default_initialization(&idl, config)) {
        perror("Can't initialize IDL. ABORTING");
        exit(-1);
    }
    ovsdb_idl_wait_for_replica_synched(idl);

    /* Generate the map values */
    //char *names[NUMBER_OF_COUNTERS];
    //int64_t values[NUMBER_OF_COUNTERS];

    //waitmon_init_map_values(0, names, values, NUMBER_OF_COUNTERS);

    /* Insert default records */
    txn = ovsdb_idl_txn_create(idl);
    for (int i = 0; i < config->pool_size; ++i) {
        rec_counter = ovsrec_test_insert(txn);
        ovsrec_test_set_stringField(rec_counter, "hello world");
        ovsrec_test_set_boolField(rec_counter, false);
        ovsrec_test_set_numericField(rec_counter, 0);
    }
    retval = ovsdb_idl_txn_commit_block(txn);
    if (retval != TXN_SUCCESS) {
        /* Its not strictly necessary to clean the records. */
        exit_with_error_message("Can't delete existing records. Aborting",
                                config);
    }
    ovsdb_idl_txn_destroy(txn);
    //waitmon_free_map_values(names, values, NUMBER_OF_COUNTERS);
    ovsdb_idl_destroy(idl);
}

/* Counters Pubsub Test entry point */
void
do_counters_waitmon_test(struct benchmark_config *config)
{
    if (config->total_requests <= 0) {
        exit_with_error_message("Requests number must be greater than 0\n",
                                config);
    }

    if (config->workers <= 0) {
        exit_with_error_message("Workers must greater than 0\n", config);
    }

    if (config->pool_size <= 0) {
        exit_with_error_message
            ("Counters rows pool size must greater than 0\n", config);
    }

    if (config->delay < 0) {
        exit_with_error_message("Delay must be equal or greater than 0\n",
                                config);
    }

    config->producers = config->producers ? config->producers : 1;

    config->enable_cache = true;
    pipe(config->start_chan);
    pipe(config->done_chan);
    waitmon_initialize_counters_test(config);
    do_metatest2(config,
                &waitmon_worker_for_counters_test,
                "waitmon_test", &waitmon_counters_producer);
}
