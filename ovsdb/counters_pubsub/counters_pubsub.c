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

void
counters_pubsub_help(char *binary_name)
{
    printf("Test of subscription to counters\n"
           "Usage: %s counters-pubsub -n <number of updates>"
           " -w <number of workers> -m <number of rows> [-d delay] [-s]\n"
           "Do a counters requests, response retrieval cycle, using several\n"
           "subscribers, and one producer.\n"
           "Each subscriber do <number of updates> requests the producer, \n"
           "and the producer responds with <number of rows> rows of counters."
           "\nIn the test also a delay between counters requests can be set,\n"
           "with a delay in microseconds.\n"
           "If receives the -s flag then the counters are updated always\n",
           binary_name);
}

/**
 * Initialize the arrays of a "map(string->int)" to the given value
 * @param [in] value to be set in the arrays
 * @param [out] pointer to a array of strings (with values "0" to "<total>").
 * @param [out] pointer to a array of ints, to be set to value.
 * @param [in] size of the arrays
 */
static void
init_map_values(int64_t value, char **names, int64_t * values, size_t size)
{
    for (int i = 0; i < size; i++) {
        names[i] = xasprintf("counter-%d", i);
        values[i] = value;
    }
}

/**
 * Deallocates the memory allocated in the arrays, if necesary.
 * @param [out] pointer to a array of strings (with values "0" to "<total>").
 * @param [out] pointer to a array of ints, to be set to value.
 * @param [in] size of the arrays
 */
static void
free_map_values(char **names, int64_t * values, size_t size)
{
    for (int i = 0; i < size; i++) {
        free(names[i]);
    }
}

/**
 * Write to the OVSDB a request to update the counters.
 * @param [in] Pointer to the IDL
 * @param [in] Timestamp for the update counters request
 */
static void
make_counters_request(struct ovsdb_idl *idl, uint64_t current_time)
{
    enum ovsdb_idl_txn_status status;
    struct ovsdb_idl_txn *txn;
    const struct ovsrec_testcountersrequests *rec_request;

    do {
        txn = ovsdb_idl_txn_create(idl);
        rec_request = ovsrec_testcountersrequests_insert(txn);
        /* Saves the current time data */
        ovsrec_testcountersrequests_set_requestTimestamp(rec_request, current_time);
        ovsrec_testcountersrequests_set_responseTimestamp(rec_request, 0);
        /* Commit transaction */
        status = ovsdb_idl_txn_commit_block(txn);
        ovsdb_idl_txn_destroy(txn);
    } while (status != TXN_SUCCESS);
}

/**
 * Blocks until the IDL have synched (the sequence number changes).
 * @param [in] Pointer to an IDL instance
 * @param [in] Old sequence number
 * @returns The newest sequence number, after blocking.
 */
static unsigned int
wait_for_replica_sync(struct ovsdb_idl *idl, unsigned int old_seqno)
{
    unsigned int new_seqno;

    /* Wait for deletes being synced with our replica */
    while ((new_seqno = ovsdb_idl_get_seqno(idl)) == old_seqno) {
        ovsdb_idl_wait(idl);
        poll_block();
        ovsdb_idl_run(idl);
    }
    return new_seqno;
}

/**
 * Write the response time, the producer already write
 * a valid response.
 * @param [in] Pointer to a IDL instance
 * @param [in] Specifies if the function must update the counters or not
 * @param [in/out] OVSREC record of the current counters requests.
 */
static enum ovsdb_idl_txn_status
update_counters(struct ovsdb_idl *idl,
                bool update_counters,
                const struct ovsrec_testcountersrequests *rec_request)
{
    struct ovsdb_idl_txn *txn;
    const struct ovsrec_testcounters *rec_counter;
    enum ovsdb_idl_txn_status status;
    uint64_t timestamp = microseconds_now();
    char *names[NUMBER_OF_COUNTERS];
    int64_t values[NUMBER_OF_COUNTERS];

    do {
        txn = ovsdb_idl_txn_create(idl);
        if (update_counters) {
            init_map_values((int64_t) timestamp,
                            names, values, NUMBER_OF_COUNTERS);
            OVSREC_TESTCOUNTERS_FOR_EACH(rec_counter, idl) {
                ovsrec_testcounters_set_countersField(rec_counter,
                                                      names,
                                                      values,
                                                      NUMBER_OF_COUNTERS);
            }
        }
        ovsrec_testcountersrequests_set_responseTimestamp(rec_request,
                                                          microseconds_now());
        status = ovsdb_idl_txn_commit_block(txn);
        ovsdb_idl_txn_destroy(txn);
<<<<<<< HEAD
    } while (status != TXN_SUCCESS || status != TXN_UNCHANGED);
=======
    } while (status != TXN_SUCCESS && status != TXN_UNCHANGED);
>>>>>>> e2aff9d... new: usr: OVSDB benchmark tests

    if (update_counters) {
        free_map_values(names, values, NUMBER_OF_COUNTERS);
    }
    return status;
}

/**
 * Counters publisher/producer in this test.
 * @param [in] Benchmark configuration
 */
static void
counters_pubsub_producer(const struct benchmark_config *config)
{
    struct ovsdb_idl *idl;
    const struct ovsrec_testcountersrequests *rec_request;
    uint64_t request_time, last_request_time, response_time;
    int i;
    unsigned int seqno = 0;

    /* Do initialization */
    last_request_time = 0;
    if (!idl_default_initialization(&idl, config)) {
        perror("Can't initialize IDL. ABORTING");
        exit(-1);
    }
    ovsdb_idl_wait_for_replica_synched(idl);

    /* This producer expects workers * total_requests requests */
    i = 0;
    while (i < config->workers * config->total_requests) {
        OVSREC_TESTCOUNTERSREQUESTS_FOR_EACH(rec_request, idl) {
            request_time = rec_request->requestTimestamp;
            response_time = rec_request->responseTimestamp;
            if (response_time == 0) {
                /* Write counters and update response timestamp */
                update_counters(idl,
                                request_time < last_request_time ||
                                config->single_transaction,
                                rec_request);
                last_request_time = request_time;
                i++;
            }
        }
        seqno = wait_for_replica_sync(idl, seqno);
    }

    ovsdb_idl_destroy(idl);
}

/**
 * The worker will try to read rows from the DB. The numericField is the
 * time (in microseconds) at row modification. With that the program can
 * calculate how many microseconds has passed since the last database
 * update.
 * @param [in] Benchmark configuration
 * @param [in] Worker ID
 * @param [out] struct sample_data array to save the resulting timings.
 */
static void
worker_for_counters_pubsub_test(const struct benchmark_config *config, int id,
                                struct sample_data *responses)
{
    uint64_t epoch_time;
    int i, response_index;
    unsigned int seqno;
    struct ovsdb_idl *idl;
    pid_t ovsdb_pid = pid_from_file(OVSDB_SERVER_PID_FILE_PATH);
    struct process_stats stat_a, stat_b;
    struct process_stats *p_initial_stat, *p_end_stat;
    const struct ovsrec_testcounters *rec_counter;
    const struct ovsrec_testcountersrequests *rec_request;
    uint64_t current_time, request_time;
    bool write_stats = false;

    p_initial_stat = &stat_a;
    p_end_stat = &stat_b;
    seqno = 0;
    request_time = 0;

    /* Initializes IDL */
    if (!idl_default_initialization(&idl, config)) {
        perror("Can't initialize IDL. ABORTING");
        exit(-1);
    }
    ovsdb_idl_wait_for_replica_synched(idl);

    epoch_time = microseconds_now() - config->test_start_time;
    get_usage(ovsdb_pid, p_initial_stat);

    /* The program is waiting for total_requests counters updates */
    for (response_index = 0;
         response_index < config->total_requests;
         ++response_index) {
        write_stats = true;
        /* Ask for a update on the counters table */
        current_time = microseconds_now() - config->test_start_time;
        responses[response_index].start_time = current_time;
        make_counters_request(idl, current_time);

        /* Wait until something changed in the replica */
        do {
            seqno = wait_for_replica_sync(idl, seqno);
            OVSREC_TESTCOUNTERSREQUESTS_FOR_EACH(rec_request, idl) {
                request_time = rec_request->requestTimestamp;
                if (request_time == current_time) {
                    /* The timestamp of the response at the producer
                     * can be read at rec_request->responseTimestamp.
                     */
                    break;
                }
            }
        } while (request_time != current_time);

        /* Read the counters */
        uint64_t accumulated_time;

        OVSREC_TESTCOUNTERS_FOR_EACH(rec_counter, idl) {
            for (i = 0; i < rec_counter->n_countersField; i++) {
                accumulated_time += rec_counter->value_countersField[i];
            }
        }

        responses[response_index].end_time = microseconds_now() -
            config->test_start_time;
        responses[response_index].worker_id = id;
        responses[response_index].status = TXN_SUCCESS;
        if (responses[response_index].end_time -
            epoch_time > config->time_window) {
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
        i = config->total_requests - 1;
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

/**
 * Initializes the OVSDB test tables to perform this test.
 * @param [in] Benchmark configuration
 */
void
<<<<<<< HEAD
initialize_counters_pubsub_test(const struct benchmark_config *config)
=======
initialize_counters_pubsub_test(struct benchmark_config *config)
>>>>>>> e2aff9d... new: usr: OVSDB benchmark tests
{
    enum ovsdb_idl_txn_status retval;
    struct ovsdb_idl_txn *txn;
    struct ovsdb_idl *idl;
    const struct ovsrec_testcounters *rec_counter;

    /* Clears the test tables using ovsdb-client */
<<<<<<< HEAD
    clear_table("TestCounters");
    clear_table("TestCountersRequests");
=======
    clear_test_tables(config);
>>>>>>> e2aff9d... new: usr: OVSDB benchmark tests

    /* Initializes IDL */
    if (!idl_default_initialization(&idl, config)) {
        perror("Can't initialize IDL. ABORTING");
        exit(-1);
    }
    ovsdb_idl_wait_for_replica_synched(idl);

    /* Generate the map values */
    char *names[NUMBER_OF_COUNTERS];
    int64_t values[NUMBER_OF_COUNTERS];

    init_map_values(0, names, values, NUMBER_OF_COUNTERS);

    /* Insert default records */
    txn = ovsdb_idl_txn_create(idl);
    for (int i = 0; i < config->pool_size; ++i) {
        rec_counter = ovsrec_testcounters_insert(txn);
        ovsrec_testcounters_set_countersField(rec_counter,
                                              names,
                                              values, NUMBER_OF_COUNTERS);
    }
    retval = ovsdb_idl_txn_commit_block(txn);
    if (retval != TXN_SUCCESS) {
        /* Its not strictly necessary to clean the records. */
        exit_with_error_message("Can't delete existing records. Aborting",
                                config);
    }
    ovsdb_idl_txn_destroy(txn);
    free_map_values(names, values, NUMBER_OF_COUNTERS);
    ovsdb_idl_destroy(idl);
}

/**
 * Counters Pubsub Test entry point
 * @param [in] Benchmark configuration
 */
void
do_counters_pubsub_test(struct benchmark_config *config)
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

    config->enable_cache = true;
    initialize_counters_pubsub_test(config);
    do_metatest(config,
                &worker_for_counters_pubsub_test,
                "pubsub_test", &counters_pubsub_producer);
}
