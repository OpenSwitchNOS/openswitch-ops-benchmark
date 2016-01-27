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
  * A benchmark for testing insertion of rows with the use of index
 * columns in table.
 */

#include "insert-index-time.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>

#include "vswitch-idl.h"
#include "openvswitch/vconn.h"
#include "dirs.h"
#include "ovsdb-idl.h"
#include "poll-loop.h"
#include "common/common.h"

/**
 * Checks for required info before launch the test
 *
 * @param [in] configuration structure with benchmark configuration
 */
void do_insertion_index_time(struct benchmark_config *configuration) {
    if (configuration->total_requests <= 0) {
        exit_with_error_message("Requests number must be greater than 0\n",
                configuration);
    }
    if (configuration->pool_size <= 0) {
        exit_with_error_message("Record pool size must be greater than 0\n",
                configuration);
    }
    run_insert_index_tests(configuration);
}


void
insertion_index_time_help(char *p_binary_name)
{
    printf( "Insert Time with Index Test\n"
            "Usage: %s [-n <transactions>] -m <records per transaction> [-h]\n"
            "Creates <transactions> transactions that inserts <records per transaction> records.\n"
            "Output CSV:\n"
            "iteration number, read time, insertion time, error code\n",
            p_binary_name);
}

/**
 * Inserts "rows" number of rows in TestIndex table
 *
 * @param [in] p_idl: Pointer to OVSDB idl structure.
 * @param [out] stats: pointer to structure used to collect stats data
 */
static void
insert_into_test_index_table(struct ovsdb_idl *p_idl, struct local_stats_data *stats)
{
    uint64_t startTime, endTime;
    enum ovsdb_idl_txn_status rc;
    char unique_row_number[20];
    struct ovsrec_testindex* rec;
    struct ovsdb_idl_txn* txn;

    ovsdb_idl_run(p_idl);

    startTime = microseconds_now();
    txn = ovsdb_idl_txn_create(p_idl);

    for (int i = 0; i < stats->rows_to_insert; ++i) {
        sprintf(unique_row_number, "%d%05d", stats->iteration,i);
        rec = ovsrec_testindex_insert(txn);
        fill_test_index_row(rec, atol(unique_row_number));
    }

    rc = ovsdb_idl_txn_commit_block(txn);
    ovsdb_idl_txn_destroy(txn);
    endTime = microseconds_now();
    stats->write_time = endTime-startTime;
    stats->succeded = rc == TXN_SUCCESS ? true: false;

}

/*
 * @brief Reads all records from table TestIndex.
 *
 * @param[in] p_idl: Pointer to OVSDB idl structure.
 * @param[out] stats: pointer to structure used to collect stats data
 */
static void
read_test_index_table(struct ovsdb_idl *p_idl, struct local_stats_data *stats)
{
    const struct ovsrec_testindex* rec;
    char *stringField __attribute__((unused));
    char *enumField __attribute__((unused));
    int numericField __attribute__((unused));
    int records_read;
    bool boolField __attribute__((unused));

    uint64_t start_time, end_time;

    records_read = 0;
    start_time = microseconds_now();
    OVSREC_TESTINDEX_FOR_EACH(rec, p_idl) {
        stringField  = rec->stringField;
        numericField = rec->numericField;
        enumField    = rec->enumField;
        boolField    = rec->boolField;
        records_read++;
    }
    end_time = microseconds_now();
    stats->rows_read = records_read;
    stats->read_time = end_time - start_time;

}

/*
 * Runs the test for inserting/reading from table TestIndex when using
 * index columns.
 *
 * @param[in] p_conf: Pointer to configuration data.
 *
 * Output format:
 *   iteration, rows to insert, rows read, read time, write time, error code
 */
int
run_insert_index_tests(struct benchmark_config *p_conf)
{
    struct ovsdb_idl *idl;
    struct local_stats_data *stats;

    /* Clear the test table */
<<<<<<< HEAD
    clear_table("TestIndex");
=======
    clear_test_tables(p_conf);
>>>>>>> e2aff9d... new: usr: OVSDB benchmark tests

    /* Initializes IDL */
    ovsrec_init();
    idl = ovsdb_idl_create(p_conf->ovsdb_socket, &ovsrec_idl_class,
            false, true);
    ovsdb_idl_add_table(idl, &ovsrec_table_testindex);
    ovsdb_idl_add_column(idl, &ovsrec_testindex_col_stringField);
    ovsdb_idl_add_column(idl, &ovsrec_testindex_col_numericField);
    ovsdb_idl_add_column(idl, &ovsrec_testindex_col_enumField);
    ovsdb_idl_add_column(idl, &ovsrec_testindex_col_boolField);

    /* Wait for replica to be ready */
    ovsdb_idl_wait_for_replica_synched(idl);

    /* Create needed data collector */
    if( !(stats = malloc(p_conf->total_requests * sizeof(struct stats_data))) ) {
        perror( "Error allocating memory to collect statistics\n ");
        exit(1);
    }

    printf("iteration,rows_inserted,rows_read,"
           "read_time,write_time,status\n");

    for (int iteration=0; iteration < p_conf->total_requests; iteration++){
        stats[iteration].iteration = iteration+1;
        stats[iteration].rows_to_insert = p_conf->pool_size;
        insert_into_test_index_table(idl, &stats[iteration]);
        read_test_index_table(idl, &stats[iteration]);
        printf("%d,%d,%d,%"PRIu64",%"PRIu64",%s\n",
                stats[iteration].iteration,
                stats[iteration].rows_to_insert,
                stats[iteration].rows_read,
                stats[iteration].read_time,
                stats[iteration].write_time,
                stats[iteration].succeded ? "Success":"Fail");
    }

    free(stats);

    ovsdb_idl_destroy(idl);
    return 0;
}
