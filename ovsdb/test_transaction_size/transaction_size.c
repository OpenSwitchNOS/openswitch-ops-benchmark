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

/**
 * @file
 * Implementation of the transaction related performance tests.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <util.h>

#include "common/common.h"
#include "common/process_usage.h"

#include "vswitch-idl.h"
#include "openvswitch/vconn.h"
#include "dirs.h"
#include "ovsdb-idl.h"
#include "poll-loop.h"

/* Prints help information about this test.
 * 'binary_name' is the name of the executable file. */
void
transaction_size_help(char *binary_name)
{
    printf("Transaction Size Test\n"
           "Usage: %s transaction-size -n <quantity> [-s] [-c]\n"
           "Inserts <quantity> records, using a single big transaction (if -s"
           " flag is given), or <quantity> different transactions.\n"
           "The IDL cache can be disabled, using the flag -c.\n"
           "The output is a CSV printed to stdout, with the following"
           " columns:\n"
           "- Single transaction?:\n\t1 if using a single big transaction, 0 "
           "otherwise.\n"
           "- Number of records:\n\tNumber of records inserted."
           "- VSize:\n\tVirtual memory used by OVSDB Server, in bytes.\n"
           "- RSS:\n\tResident Set Size of OVSDB Server, in bytes.\n"
           "- User CPU %%:\n\tPercentage of CPU in user mode used by OVSDB "
           "Server during this second.\n"
           "- System CPU %%:\n\tPercentage of CPU in system mode used by "
           "OVSDB Server during this second.\n"
           "- Duration:\n\tDuration in microseconds of the test.\n",
           binary_name);
}

/**
 * @brief Insert n rows into the test table.
 * Depending of the value of /p single_transaction, this function inserts
 * /p rows into the Test table.
 *
 * @param [in] IDL instance for this test
 * @param [in] rows number of rows that will be inserted
 * @param [in] single_transaction flag for using a single transaction for all the
 * insertions. If false, a transaction will be used for each insertion.
 * @param [out] sample struct where the results will be written.
*/
static void
insert_into_test_table(struct ovsdb_idl *idl, int rows,
                       bool single_transaction,
                       struct sample_data *const sample,
                       struct benchmark_config *config)
{
    struct process_stats initial_stat;
    struct process_stats final_stat;

    pid_t ovsdb_pid = pid_of_ovsdb(config);

    struct ovsrec_test *rec;
    struct ovsdb_idl_txn *txn;
    enum ovsdb_idl_txn_status status;

    ovsdb_idl_run(idl);

    if (single_transaction) {
        /* Start timer and stats measurement */
        sample->start_time = nanoseconds_now();
        get_usage(ovsdb_pid, &initial_stat);

        txn = ovsdb_idl_txn_create(idl);

        /* Insert all the requested rows in the table */
        for (int i = 0; i < rows; ++i) {
            rec = ovsrec_test_insert(txn);
            fill_test_row(rec, i);
        }

        status = ovsdb_idl_txn_commit_block(txn);
        if (status != TXN_SUCCESS) {
            fprintf(stderr, "Error while committing transaction. rc = %d\n",
                    status);
        }

        ovsdb_idl_txn_destroy(txn);

        /* Stop timer and measure the CPU and memory usage */
        sample->end_time = nanoseconds_now();
        get_usage(ovsdb_pid, &final_stat);

    } else {
        /* Start timer */
        sample->start_time = nanoseconds_now();
        get_usage(ovsdb_pid, &initial_stat);

        for (int i = 0; i < rows; ++i) {
            txn = ovsdb_idl_txn_create(idl);

            rec = ovsrec_test_insert(txn);
            fill_test_row(rec, i);

            status = ovsdb_idl_txn_commit_block(txn);
            if (status != TXN_SUCCESS) {
                fprintf(stderr,
                        "Error while committing transaction. rc = %d\n",
                        status);
            }

            ovsdb_idl_txn_destroy(txn);
        }

        /* Stop timer and measure the CPU and memory usage */
        sample->end_time = nanoseconds_now();
        get_usage(ovsdb_pid, &final_stat);
    }

    /* Write the results in the sample structure */
    cpu_usage_pct(&final_stat, &initial_stat,
                  &sample->user_cpu_pct, &sample->system_cpu_pct);
    sample->vsize = final_stat.vsize;
    sample->rss = final_stat.rss;
}

/**
 * @brief run the transaction size test
 * Runs the transaction size test and writes the results to the
 * standard output.
 *
 * @param [in] config benchmark configuration structure
*/
void
do_transaction_size_test(struct benchmark_config *config)
{
    if (config->total_requests <= 0) {
        exit_with_error_message("Requests number/transaction size "
                                "must be greater than 0\n", config);
    }

    struct sample_data sample;
    struct ovsdb_idl *idl;

    /* Initialize the metadata for the IDL cache. */
    if (!idl_default_initialization(&idl, config)) {
        int rc = ovsdb_idl_get_last_error(idl);

        fprintf(stderr, "Connection to database failed: %s\n",
                ovs_retval_to_string(rc));
    }

    insert_into_test_table(idl, config->total_requests,
                           config->single_transaction, &sample, config);

    printf("%d,%" PRIu32 ",%lu,%ld,%f,%f,%" PRIu64 "\n",
           config->single_transaction ? 1 : 0, config->total_requests,
           sample.vsize, sample.rss,
           sample.user_cpu_pct, sample.system_cpu_pct,
           sample.end_time - sample.start_time);

    ovsdb_idl_destroy(idl);
}
