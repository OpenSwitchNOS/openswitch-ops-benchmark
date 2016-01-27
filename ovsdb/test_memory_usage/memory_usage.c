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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "common/common.h"
#include "common/process_usage.h"

#include "ovsdb-idl.h"
#include "vswitch-idl.h"
#include "dirs.h"

void
memory_usage_help(char *binary_name)
{
    printf("Memory Usage Test\n"
           "Usage: %s memory-test -n <quantity> [-c]\n"
           "Inserts <quantity> records, reading the OVSDB Server RAM and CPU"
           "usage after each request.\n"
           "The IDL cache can be disabled, using the flag -c.\n"
           "The output is a CSV printed to stdout, with one row per request, "
           "and the following columns:\n"
           "- Duration:\n\tDuration in microseconds of the request.\n"
           "- Number of request:\n\tSequence number of the request.\n"
           "- Status:\n\tThe status returned by the transaction:\n"
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
           "- User CPU %%:\n\tPercentage of CPU in user mode used by OVSDB "
           "Server during this second.\n"
           "- System CPU %%:\n\tPercentage of CPU in system mode used by "
           "OVSDB Server during this second.\n"
           "- VSize:\n\tVirtual memory used by OVSDB Server, in bytes.\n"
           "- RSS:\n\tResident Set Size of OVSDB Server, in bytes.\n",
           binary_name);
}

static void
print_memusage_output_header(void)
{
    printf("duration,num,status,ucpu,scpu,vsize,rss\n");
}

void
print_response_stats(int i, const struct sample_data *response)
{
    printf("%" PRId64 ",%d,%d,%f,%f,%lu,%ld\n",
           response->end_time - response->start_time,
           i,
           response->status,
           response->user_cpu_pct,
           response->system_cpu_pct, response->vsize, response->rss);
}

/**
 * This test insert a big number of rows in the OVSDB,
 * recording the memory usage at each insertion.
 *
 * @param[in] config: Pointer to configuration data.
 */
void
do_memusage_test(struct benchmark_config *config)
{
    if(config->total_requests <= 0) {
        exit_with_error_message("Requests number must be greater than 0\n",
                                config);
    }

    int ovsdb_pid;
    struct process_stats stat_a, stat_b;
    struct process_stats *p_ovsdb_stat_ini, *p_ovsdb_stat_end;
    struct ovsdb_idl *idl;
    struct ovsdb_idl_txn *txn;
    struct ovsrec_test *test_record;
    struct sample_data response;

    /* Initializes IDL */
    if (!idl_default_initialization(&idl, config)) {
        perror("Can't initialize IDL. ABORTING");
        exit(1);
    }

    /* Initializes stats configuration */
    ovsdb_pid = pid_from_file(OVSDB_SERVER_PID_FILE_PATH);
    p_ovsdb_stat_ini = &stat_a;
    p_ovsdb_stat_end = &stat_b;
    get_usage(ovsdb_pid, p_ovsdb_stat_ini);

    /* Do the requests */
    print_memusage_output_header();
    for (int i = 0; i < config->total_requests; i++) {
        response.start_time = microseconds_now();

        /* START of the test */
        ovsdb_idl_run(idl);
        txn = ovsdb_idl_txn_create(idl);

        test_record = ovsrec_test_insert(txn);

        /* The insertion data */
        fill_test_row(test_record, i);

        /* Commit transaction */
        response.status = ovsdb_idl_txn_commit_block(txn);

        ovsdb_idl_txn_destroy(txn);

        /* END of the test */
        response.end_time = microseconds_now();

        /* Update the OVSDB server data */
        get_usage(ovsdb_pid, p_ovsdb_stat_end);
        cpu_usage_pct(p_ovsdb_stat_ini, p_ovsdb_stat_end,
                      &(response.user_cpu_pct), &(response.system_cpu_pct));
        response.vsize = p_ovsdb_stat_end->vsize;
        response.rss = p_ovsdb_stat_end->rss;
        print_response_stats(i + 1, &response);
        swap_stats(p_ovsdb_stat_ini, p_ovsdb_stat_end);
    }

    ovsdb_idl_destroy(idl);
}
