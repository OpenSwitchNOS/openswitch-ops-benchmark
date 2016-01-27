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

#include "poll-loop.h"
#include "ovsdb-idl.h"
#include "vswitch-idl.h"
#include "dirs.h"

void
pubsub_help(char *binary_name)
{
    printf("Update Records Test\n"
           "Usage: %s pubsub -w <subscribers> -n <max_test_count> [-c]\n"
           "Do a pubsub with several subscribers.", binary_name);
}

/**
 * Insert and publish data
 *
 * @param [in] configuration in the benchmark configuration structure
 */
static void
pubsub_producer(const struct benchmark_config *configuration)
{
    struct ovsdb_idl *idl;
    struct ovsdb_idl_txn *txn;
    enum ovsdb_idl_txn_status status;
    char *record_id;
    const struct ovsrec_test *rec;

    /* Clears the test tables using ovsdb-client */
    clear_table("Test");

    /* Do initialization */
    idl_default_initialization(&idl, configuration);
    ovsdb_idl_wait_for_replica_synched(idl);

    /* Do publications */
    for (int i = 0; i < configuration->total_requests; ++i) {
        ovsdb_idl_run(idl);
        do {
            txn = ovsdb_idl_txn_create(idl);

            rec = ovsrec_test_insert(txn);

            /* The insertion data */
            asprintf(&record_id, "%d", i);
            ovsrec_test_set_stringField(rec, record_id);
            ovsrec_test_set_numericField(rec, microseconds_now() -
                                         configuration->test_start_time);
            ovsrec_test_set_enumField(rec, OVSREC_TEST_ENUMFIELD_ENUMVALUE1);
            ovsrec_test_set_boolField(rec, true);

            /* Commit transaction */
            status = ovsdb_idl_txn_commit_block(txn);

            ovsdb_idl_txn_destroy(txn);
        } while (status != TXN_SUCCESS);

        usleep(configuration->delay);
    }

    ovsdb_idl_destroy(idl);
}

/**
 * The worker will try to read rows from the DB. The numericField is the
 * time (in microseconds) at row modification. With that the program can
 * calculate how many microseconds has passed since the last database
 * update.
 *
 * @param [in] config Benchmark configuration
 * @param [in] id Worker ID
 * @param [out] responses sample_data array to save the measurings
 */
static void
worker_for_pubsub_test(const struct benchmark_config *config, int id,
                       struct sample_data *responses)
{
    uint64_t epoch_time;
    uint64_t current_time;
    int i;
    unsigned int old_seqno, new_seqno;
    int records_read;
    struct ovsdb_idl *idl;
    pid_t ovsdb_pid = pid_from_file(OVSDB_SERVER_PID_FILE_PATH);
    struct process_stats stat_a, stat_b;
    struct process_stats *p_initial_stat, *p_end_stat;
    const struct ovsrec_test *ovs_test;
    bool write_stats;

    p_initial_stat = &stat_a;
    p_end_stat = &stat_b;
    old_seqno = 0;
    new_seqno = ovsdb_idl_get_seqno(idl);

    /* Initializes IDL */
    if (!idl_default_initialization(&idl, config)) {
        perror("Can't initialize IDL. ABORTING");
        exit(-1);
    }
    ovsdb_idl_wait_for_replica_synched(idl);

    epoch_time = microseconds_now() - config->test_start_time;
    get_usage(ovsdb_pid, p_initial_stat);
    records_read = 0;
    do {
        write_stats = true;
        ovsdb_idl_run(idl);
        new_seqno = ovsdb_idl_get_seqno(idl);
        if (old_seqno != new_seqno) {
            old_seqno = new_seqno;
            current_time = microseconds_now() - config->test_start_time;
            OVSREC_TEST_FOR_EACH(ovs_test, idl) {
                i = atoi(ovs_test->stringField);
                /* If start_time is zero then this record hasn't been read
                 * before. */
                if (responses[i].start_time == 0) {
                    responses[i].worker_id = id;
                    responses[i].start_time = ovs_test->numericField;
                    responses[i].end_time = current_time;
                    responses[i].status = TXN_SUCCESS;
                    if (id == 1 && (responses[i].end_time -
                                    epoch_time > config->time_window)) {
                        write_stats = false;
                        epoch_time = responses[i].end_time;
                        get_usage(ovsdb_pid, p_end_stat);
                        cpu_usage_pct(p_end_stat, p_initial_stat,
                                      &(responses[i].user_cpu_pct),
                                      &(responses[i].system_cpu_pct));
                        responses[i].vsize = p_end_stat->vsize;
                        responses[i].rss = p_end_stat->rss;
                        swap_stats(p_initial_stat, p_end_stat);
                    }
                    records_read++;
                }
            }
        } else {
            ovsdb_idl_wait(idl);
            poll_block();
        }
    } while (records_read < config->total_requests);

    /* Saves the last usage */
    if (write_stats) {
        i = config->total_requests - 1;
        get_usage(ovsdb_pid, p_end_stat);
        cpu_usage_pct(p_end_stat, p_initial_stat,
                      &(responses[i].user_cpu_pct),
                      &(responses[i].system_cpu_pct));
        responses[i].vsize = p_end_stat->vsize;
        responses[i].rss = p_end_stat->rss;
    }

    /* Receives done notification, and stops the process */
    ovsdb_idl_destroy(idl);
}

/**
 * Runs the Publisher/Subscriber tests after checking for validity
 * of the configuration provided
 *
 * @param config benchmark configuration
 */
void
do_pubsub_test(struct benchmark_config *config)
{
    if (config->total_requests <= 0) {
        exit_with_error_message("Requests number must be greater than 0\n",
                                config);
    }

    if (config->workers <= 0) {
        exit_with_error_message("Workers number must be greater than 0\n",
                                config);
    }

    if (config->delay < 0) {
        exit_with_error_message("Delay must be equal or greater than 0\n",
                                config);
    }
    config->enable_cache = true;
    do_metatest(config, &worker_for_pubsub_test, "pubsub_test",
                &pubsub_producer);
}
