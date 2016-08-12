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

#include<stdlib.h>

#include "record_update.h"
#include "common/metatest_workers.h"

#include "vswitch-idl.h"
#include "ovsdb-idl.h"

/* Prints help information about this test.
 * 'binary_name' is the name of the executable file. */
void
record_update_help(char *binary_name)
{
    printf("Update Records Test\n"
           "Usage: %s update -n <quantity> -w <workers> -m <records> "
           "[-a <updates per TXN, default=1>] [-c]\n"
           "Each worker update <quantity> rows, selected at random, from a "
           "set of <records> rows. The probability of two or more workers"
           "updating the same row can be set tweaking the -w and -m values.\n"
           "The IDL cache can be disabled, using the flag -c.\n"
           "The output is a CSV printed to stdout, with the following "
           "columns:\n"
           "- Status:\n\tThe status of the transactions aggregated in this "
           "row.\n"
           "\t -1: Aggregates all the responses.\n"
           "\t  0: TXN_UNCOMMITTED: Not yet committed or aborted.\n"
           "\t  1: TXN_UNCHANGED: Transaction didn't include any changes.\n"
           "\t  2: TXN_INCOMPLETE: Commit in progress, please wait.\n"
           "\t  3: TXN_ABORTED: ovsdb_idl_txn_abort() called.\n"
           "\t  4: TXN_SUCCESS: Commit successful.\n"
           "\t  5: TXN_TRY_AGAIN: Commit failed because a 'verify' operation"
           " reported an inconsistency, due to a network problem, or other"
           " transient failure.  Wait for a change, then try again.\n"
           "\t  6: TXN_NOT_LOCKED: Server hasn't given us the lock yet.\n"
           "\t  7: TXN_ERROR: Commit failed due to a hard error.\n"
           "- Second:\n\tThe second since the test start in which the "
           "responses aggregated finished.\n"
           "- Count:\n\tThe count of requests aggregated in this row.\n"
           "- Fail Count:\n\tThe number unsuccessful responses (status != 4)\n"
           "- Minimum Duration:\n\tDuration in microseconds of shortest "
           "request finished in this second.\n"
           "- Max Duration:\n\tDuration in microseconds of largest request "
           "finished in this second.\n"
           "- Duration Sum:\n\tSum of the duration (in microseconds) of all "
           "the requests finished in this second.\n"
           "- User CPU %%:\n\tPercentage of CPU in user mode used by OVSDB "
           "Server during this second.\n"
           "- System CPU %%:\n\tPercentage of CPU in system mode used by "
           "OVSDB Server during this second.\n"
           "- VSize:\n\tVirtual memory used by OVSDB Server, in bytes.\n"
           "- RSS:\n\tResident Set Size of OVSDB Server, in bytes.\n",
           binary_name);
}

/* Inserts records into DB, to be used for later update. */
static void
insert_records(struct benchmark_config *config)
{
    struct ovsdb_idl *idl;
    const struct ovsrec_test *rec;
    struct ovsrec_test* new_rec;
    struct ovsdb_idl_txn *txn;
    enum ovsdb_idl_txn_status status;

    /* Initialize the metadata for the IDL cache. */
    config->enable_cache = true;
    if (!idl_default_initialization(&idl, config)) {
        int rc = ovsdb_idl_get_last_error(idl);

        fprintf(stderr, "Connection to database failed: %s\n",
                ovs_retval_to_string(rc));
    }
    ovsdb_idl_run(idl);

    /* Insert the initial records to be updated */
    for (int i = 0; i < config->pool_size; ++i) {
        txn = ovsdb_idl_txn_create(idl);
        new_rec = ovsrec_test_insert(txn);
        fill_test_row(new_rec, i);
        status = ovsdb_idl_txn_commit_block(txn);
        if (status != TXN_SUCCESS) {
            fprintf(stderr, "Error while committing transaction. rc = %d\n",
                    status);
        }
        ovsdb_idl_txn_destroy(txn);
    }

    /* Gets the UUIDs of the inserted records */
    int i = 0;
    OVSREC_TEST_FOR_EACH(rec, idl) {
        struct uuid *dst = &(config->record_pool[i]);
        const struct uuid *src = &(rec->header_.uuid);

        memcpy(dst, src, sizeof (struct uuid));

        /* In case the DB has more records than pool_size then exit. */
        i++;
        if (i >= config->pool_size) {
            break;
        }
    }
}

/* The worker will send requests as fast as possible against the DB.
 * Receives the request number and  must send to the timing pipe the
 * elapsed time, the successful state and the value. */
static void
worker_for_update_test(const struct benchmark_config *config, int id,
                       struct sample_data *responses)
{
    uint64_t epoch_time;
    int i, j;
    struct ovsdb_idl *idl;
    struct ovsdb_idl_txn *txn;
    pid_t ovsdb_pid = pid_of_ovsdb(config);
    struct process_stats stat_a, stat_b;
    struct process_stats *p_initial_stat, *p_end_stat;
    const struct uuid *selected_uuid;
    const struct ovsrec_test *test_record;

    p_initial_stat = &stat_a;
    p_end_stat = &stat_b;

    /* Initializes IDL */
    if (!idl_default_initialization(&idl, config)) {
        perror("Can't initialize IDL. ABORTING");
        exit(1);
    }

    /* Run ovsdb_idl_run until IDL returns pool_size rows */
    ovsdb_idl_wait_for_replica_synched(idl);

    /* Begin of the test */
    epoch_time = nanoseconds_now() - config->test_start_time;
    get_usage(ovsdb_pid, p_initial_stat);
    for (i = 0; i < config->total_requests; i++) {
        responses[i].worker_id = id;
        responses[i].start_time = nanoseconds_now() - config->test_start_time;

        for (j = 0; j < config->requests_per_txn; j++) {
            /* Select record to be modified */
            selected_uuid = &config->record_pool[rand() % config->pool_size];

            ovsdb_idl_run(idl);

            test_record = ovsrec_test_get_for_uuid(idl, selected_uuid);
            txn = ovsdb_idl_txn_create(idl);

            if (test_record == NULL) {
                fprintf(stderr, "null record for selected uuid\n");
            }

            /* update test */
            ovsrec_test_set_boolField(test_record, (bool) (i % 2));
            ovsrec_test_set_enumField(test_record,
                                      OVSREC_TEST_ENUMFIELD_ENUMVALUE1);
            ovsrec_test_set_numericField(test_record,
                                         id * config->total_requests +
                                         i * config->requests_per_txn + j);
            ovsrec_test_set_stringField(test_record, "row");
        }

        /* Commit transaction */
        responses[i].status = ovsdb_idl_txn_commit_block(txn);

        ovsdb_idl_txn_destroy(txn);

        /* END of the test */
        responses[i].end_time = nanoseconds_now() - config->test_start_time;
        /* If the current epoch has elapsed more than 1s, and this worker has
         * the id 1 then save the OVSDB stats */
        if (id == 1
            && responses[i].end_time - epoch_time > config->time_window) {
            epoch_time = responses[i].end_time;
            get_usage(ovsdb_pid, p_end_stat);
            cpu_usage_pct(p_end_stat, p_initial_stat,
                          &(responses[i].user_cpu_pct),
                          &(responses[i].system_cpu_pct));
            responses[i].vsize = p_end_stat->vsize;
            responses[i].rss = p_end_stat->rss;
            swap_stats(p_initial_stat, p_end_stat);
        }
    }
    /* Saves the last usage */
    get_usage(ovsdb_pid, p_end_stat);
    cpu_usage_pct(p_end_stat, p_initial_stat,
                  &(responses[i].user_cpu_pct),
                  &(responses[i].system_cpu_pct));
    responses[i].vsize = p_end_stat->vsize;
    responses[i].rss = p_end_stat->rss;

    /* Receives done notification, and stops the process */
    ovsdb_idl_destroy(idl);
}

/**
 * This test modifies the content of a given number of rows in
 * the table and measures the time required for the operation
 *
 * @param[in] config: Pointer to configuration data.
 */
void
do_record_update_test(struct benchmark_config *config)
{
    struct uuid *record_pool = calloc(config->pool_size,
                                      sizeof (struct uuid));

    /* Initialize the metadata for the IDL cache. */
    config->record_pool = record_pool;
    insert_records(config);

    /* Do the test */
    do_metatest(config, &worker_for_update_test, "update_test", NULL);
}
