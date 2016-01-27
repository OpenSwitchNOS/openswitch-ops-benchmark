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

#include "insert.h"
#include "common/common.h"
#include "common/metatest_workers.h"
#include "common/process_usage.h"

#include "ovsdb-idl.h"
#include "vswitch-idl.h"
#include "dirs.h"

void
insert_help(char *binary_name)
{
    printf("Insertion Test\n"
           "Usage: %s insert -w <workers> -n <quantity> [-c]\n"
           "Inserts <quantity> records, using <workers> process in "
           "parallel.\n"
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

/**
 * The worker will send requests as fast as possible against
 * the DB.
 * Receives the request number and  must send to the timing pipe the
 * elapsed time, the successful state and the value.
 *
 * @param [in]  config benchmark configuration structure
 * @param [in]  id worker identifier
 * @param [out] responses sample_data array to save the measurings
 */
static void
worker_for_insert_test(const struct benchmark_config *config, int id,
                       struct sample_data *responses)
{
    if (config->total_requests <= 0) {
        exit_with_error_message("Requests number must be greater than 0\n",
                                config);
    }
    if (config->workers <= 0) {
        exit_with_error_message("Workers number must be greater than 0\n",
                                config);
    }

    uint64_t epoch_time;
    int i;
    struct ovsdb_idl *idl;
    struct ovsdb_idl_txn *txn;
    struct ovsrec_test *test_record;
    pid_t ovsdb_pid = pid_from_file(OVSDB_SERVER_PID_FILE_PATH);
    struct process_stats stat_a, stat_b;
    struct process_stats *p_initial_stat, *p_end_stat;

    p_initial_stat = &stat_a;
    p_end_stat = &stat_b;

    /* Initializes IDL */
    if (!idl_default_initialization(&idl, config)) {
        perror("Can't initialize IDL. ABORTING");
        exit(1);
    }

    epoch_time = microseconds_now() - config->test_start_time;
    get_usage(ovsdb_pid, p_initial_stat);
    for (i = 0; i < config->total_requests; i++) {
        responses[i].worker_id = id;
        responses[i].start_time = microseconds_now() - config->test_start_time;

        /* START of the test */
        ovsdb_idl_run(idl);
        txn = ovsdb_idl_txn_create(idl);

        test_record = ovsrec_test_insert(txn);

        /* The insertion data */
        fill_test_row(test_record, i);

        /* Commit transaction */
        responses[i].status = ovsdb_idl_txn_commit_block(txn);

        ovsdb_idl_txn_destroy(txn);

        /* END of the test */
        responses[i].end_time = microseconds_now() - config->test_start_time;
        /* If the current epoch has elapsed more than 1s, and this worker has
         * the id 0 then save the OVSDB stats */
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
 * Runs the insertion tests using worker function
 *
 * @param [in] config is the benchmark configuration structure
 */
int
do_insert_test(struct benchmark_config *config)
{
    do_metatest(config, &worker_for_insert_test, "insert_test", NULL);
    return 0;
}
