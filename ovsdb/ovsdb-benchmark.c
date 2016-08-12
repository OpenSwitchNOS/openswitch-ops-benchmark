/* Copyright (C) 2015, 2016 Hewlett Packard Enterprise Development LP
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
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include "common/common.h"
#include "counters_waitmon/counters_waitmon.h"
#include "counters_pubsub/counters_pubsub.h"
#include "test_insert/insert.h"
#include "test_insertion_time/insert-index-time.h"
#include "test_insertion_time/insert-time.h"
#include "test_memory_usage/memory_usage.h"
#include "test_transaction_size/transaction_size.h"
#include "test_modify/record_update.h"
#include "test_pubsub/pubsub.h"

/* Prints usage help for each benchmark method, or for the benchmark
 * tool itself. */
void
show_help(enum benchmark_method method, char *binary_name)
{
    switch (method) {
    case METHOD_INSERT:
        insert_help(binary_name);
        break;
    case METHOD_INSERT_MEMORY_USAGE:
        memory_usage_help(binary_name);
        break;
    case METHOD_INSERTION_INDEX_TIME:
        insertion_index_time_help(binary_name);
        break;
    case METHOD_INSERTION_TIME:
        insertion_time_help(binary_name);
        break;
    case METHOD_TRANSACTION_SIZE:
        transaction_size_help(binary_name);
        break;
    case METHOD_UPDATE:
        record_update_help(binary_name);
        break;
    case METHOD_PUBSUB:
        pubsub_help(binary_name);
        break;
    case METHOD_COUNTERS:
        counters_pubsub_help(binary_name);
        break;
    case METHOD_WAITMON:
        counters_waitmon_help(binary_name);
        break;
    default:
        print_usage(binary_name);
        exit(1);
        break;
    }
}

/* This program is the main OVSDB benchmark tool.
 * All test can be launch from this program. */
int
main(int argc, char **argv)
{
    struct benchmark_config configuration;

    init_benchmark(&configuration);
    int option;
    bool display_help = false;
    char *path;

    configuration.argc = argc;
    configuration.argv = argv;

    while ((option = getopt(argc, argv, "d:p:o:n:w:g:m:i:sch")) != -1) {
	char c = (char)option;
        switch (c) {
        case 'i':
            configuration.process_identity = strdup(optarg);
            break;
        case 'd':
            configuration.delay = atoi(optarg);
            break;
        case 'p':
            free(configuration.ovsdb_socket);
            path = xasprintf("%s", optarg);
            configuration.ovsdb_socket = path;
            break;
        case 'o':
            free(configuration.ovsdb_pid_path);
            path = xasprintf("%s", optarg);
            configuration.ovsdb_pid_path = path;
            break;
        case 'n':
            configuration.total_requests = atoi(optarg);
            break;
        case 'a':
            configuration.requests_per_txn = atoi(optarg);
            break;
        case 'w':
            configuration.workers = atoi(optarg);
            break;
        case 'm':
            configuration.pool_size = atoi(optarg);
            break;
        case 'c':
            configuration.enable_cache = false;
            break;
        case 's':
            configuration.single_transaction = true;
            break;
        case 'g':
            configuration.producers = atoi(optarg);
            break;
        case 'h':
            display_help = true;
            break;
        case '?':
            if (optopt == 'n' || optopt == 'w' || optopt == 'm'
                || optopt == 'p' || optopt == 'o' || optopt == 'd') {
                fprintf(stderr, "Option '-%c' requires an argument", optopt);
                print_usage(argv[0]);
                destroy_benchmark(&configuration);
                exit(1);
            }
            break;
        default:
            printf("Received invalid argument '%c'\n", c);
            print_usage(argv[0]);
            destroy_benchmark(&configuration);
            exit(1);
        }
    }
    /* The first parameter (the name of the test) does not requiere a flag */
    if (optind < argc) {
        if (strcmp("insert", argv[optind]) == 0) {
            /* INSERT method requires: r: number of requests w: number of
             * workers c: enable cache flag (off by default) */
            configuration.method = METHOD_INSERT;
        } else if (strcmp("insert-index", argv[optind]) == 0) {
             /* Supports number of requests and cache flag */
             configuration.method = METHOD_INSERTION_INDEX_TIME;
        } else if (strcmp("insert-without-index", argv[optind]) == 0) {
             /* Supports number of requests and cache flag */
             configuration.method = METHOD_INSERTION_TIME;
        } else if (strcmp("memory-usage", argv[optind]) == 0) {
            /* Supports number of requests and cache flag */
            configuration.method = METHOD_INSERT_MEMORY_USAGE;
        } else if (strcmp("transaction-size", argv[optind]) == 0) {
            /* Transaction-size requires: -i number of insertions -s single
             * transaction (flag) */
            configuration.method = METHOD_TRANSACTION_SIZE;
        } else if (strcmp("update", argv[optind]) == 0) {
            configuration.method = METHOD_UPDATE;
        } else if (strcmp("pubsub", argv[optind]) == 0) {
            /* Pub sub test */
            configuration.method = METHOD_PUBSUB;
        } else if (strcmp("counters", argv[optind]) == 0) {
            configuration.method = METHOD_COUNTERS;
        } else if (strcmp("waitmon", argv[optind]) == 0) {
            configuration.method = METHOD_WAITMON;
        }else {
            exit_with_error_message("Method given not supported\n",
                                    &configuration);
        }
        optind++;
    }

    if (display_help) {
        show_help(configuration.method, argv[0]);
        destroy_benchmark(&configuration);
        exit(0);
    } else {
        switch (configuration.method) {
        case METHOD_INSERT:
            do_insert_test(&configuration);
            break;
        case METHOD_INSERT_MEMORY_USAGE:
            do_memusage_test(&configuration);
            break;
        case METHOD_INSERTION_TIME:
            do_insertion_time(&configuration);
            break;
        case METHOD_INSERTION_INDEX_TIME:
            do_insertion_index_time(&configuration);
            break;
        case METHOD_TRANSACTION_SIZE:
            do_transaction_size_test(&configuration);
            break;
        case METHOD_UPDATE:
            configuration.enable_cache = true;
            do_record_update_test(&configuration);
            break;
        case METHOD_PUBSUB:
            do_pubsub_test(&configuration);
            break;
        case METHOD_COUNTERS:
            do_counters_pubsub_test(&configuration);
            break;
        case METHOD_WAITMON:
            do_counters_waitmon_test(&configuration);
            break;
        default:
            print_usage(argv[0]);
            if (!display_help) {
                destroy_benchmark(&configuration);
                exit(1);
            }
            break;
        }
    }
    destroy_benchmark(&configuration);
}
