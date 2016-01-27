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

#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ovsdb-idl.h"
#include "vswitch-idl.h"
#include "dirs.h"
#include "poll-loop.h"


/**
 * Run ovsdb_idl_run until the IDL has loaded all the data saved at
 * OVSDB, at the moment of the IDL creation.
 */
void
ovsdb_idl_wait_for_replica_synched (struct ovsdb_idl* idl)
{
    ovsdb_idl_run(idl);
    while(!ovsdb_idl_has_ever_connected(idl)) {
        ovsdb_idl_wait(idl);
        poll_block();
        ovsdb_idl_run(idl);
    }
}

/**
 * @brief Initialize the IDL and register the require tables
 * and columns.
 * It allows to disable the cache usage, and to set the remote path.
 */
bool
idl_init(struct ovsdb_idl **idl, char *remote, bool use_cache)
{
    *idl = ovsdb_idl_create(remote, &ovsrec_idl_class, false, true);
    if (use_cache) {
        ovsdb_idl_add_table(*idl, &ovsrec_table_test);
        ovsdb_idl_add_column(*idl, &ovsrec_test_col_stringField);
        ovsdb_idl_add_column(*idl, &ovsrec_test_col_numericField);
        ovsdb_idl_add_column(*idl, &ovsrec_test_col_enumField);
        ovsdb_idl_add_column(*idl, &ovsrec_test_col_boolField);
    }
    ovsdb_idl_run(*idl);
    /* Checks if the IDL is alive */
    return ovsdb_idl_is_alive(*idl);
}

/**
 * This program shows a minimal example of how to read data from OVSDB
 * using the IDL.
 * Note that to this program run IS MANDATORY TO DEFINE OPS in the
 * compilation flags or inside the code, otherwise the IDL will read garbage.
 */
int
main(int argc, char **argv)
{
    char *remote = xasprintf("unix:%s/db.sock", ovs_rundir());
    if(remote == NULL){
        perror("Remote is null. ABORTING.");
        exit(-1);
    }
    struct ovsdb_idl *idl;
    const struct ovsrec_test *rec;
    ovsrec_init();
    if (!idl_init(&idl, remote, true)) {
        perror("Can't initialize IDL. ABORTING.");
        free(remote);
        exit(-1);
    }
    free(remote);

    /* Run ovsdb_idl_run until IDL returns pool_size rows */
    ovsdb_idl_wait_for_replica_synched (idl);

    OVSREC_TEST_FOR_EACH(rec, idl)
    {
       printf("STRUCT boolField: %d enumField: %s numericField: %" PRId64 " stringField: %s\n",
            rec->boolField, rec->enumField, rec->numericField, rec->stringField
            );
    }

    ovsdb_idl_destroy(idl);
}
