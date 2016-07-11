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

/**
 * @brief This metatest can receive any compatible worker function and
 * produce a test with workers, requests statistics summary, etc.
 * @param [in] config pointer to benchmark_config for this test.
 * @param [in] worker_function, implements a worker function.
 * @param [in] test_identifier a identifier of the test, to be used as SHM identifier.
 * @param [in] the function of the producer. Can be null.
 */
void
do_metatest(struct benchmark_config *config,
            worker_function_t worker_function,
            char *test_identifier, producer_function_t producer_function)
{
    pid_t pid;
    uint64_t total_requests;
    struct sample_data *responses;

    total_requests = config->workers * config->total_requests;

    /* Get shared memory */
    char *shmkey = xasprintf("/%s-%s", test_identifier,
                             config->process_identity);

    if ((config->responses_stats_shm_id = shm_open(shmkey,
                                                   O_CREAT | O_RDWR,
                                                   (mode_t) 0777)) < 0) {
        free(shmkey);
        perror("Can't reserve shared memory for statistics\n");
        exit(1);
    }
    /* Set the memory object's size */
    if (ftruncate(config->responses_stats_shm_id,
                  total_requests * sizeof (struct sample_data)) == -1) {
        free(shmkey);
        perror("Can't allocate shared memory for statistics\n");
        printf("\n%" PRIu64 "\n", total_requests * sizeof (struct sample_data));
        exit(1);
    }
    if ((responses = mmap(0, total_requests * sizeof (struct sample_data),
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED, config->responses_stats_shm_id, 0))
        == (struct sample_data *) MAP_FAILED) {
        free(shmkey);
        perror("Can't attach shared memory for statistics\n");
        exit(1);
    }

    /* Initialize the shared memory */
    memset(responses, 0, config->total_requests * config->workers *
           sizeof (struct sample_data));

    /* Clear the producer */
    config->test_start_time = nanoseconds_now();
    if (producer_function) {
        pid = fork();
        if (!pid) {
            (*producer_function) (config);
            exit(0);
        }
    }

    /* Create the workers */
    for (int i = 0; i < config->workers; ++i) {
        pid = fork();
        if (pid < 0) {
            perror("Can't create new worker.\n");
            exit(1);
        }
        if (!pid) {
            (*worker_function) (config,
                                i + 1, &responses[i * config->total_requests]);
            exit(0);
        }
    }

    /* Wait until all the workers end */
    for (int i = 0; i < config->workers; ++i) {
        wait(0);
    }

    /* Print the summary of the data */
    print_stats_header();
    /* Prints the stats without grouping by status */
    print_stats(config, responses, ALL_STATUSES);
    for (int i = 0; i <= TXN_ERROR; ++i) {
        /* Filters the responses by status */
        print_stats(config, responses, i);
    }

    /* Detach and free shared memory */
    close(config->responses_stats_shm_id);
    shm_unlink(shmkey);
    free(shmkey);

    if (producer_function) {
        wait(0);
    }
}

/* This metatest can receive any compatible 'worker_function' and
 * produce a test with workers, requests statistics summary, etc,
 * according to the given 'config'.
 * 'test_identifier' is used as SHM identifier. 'producer_function' can
 * be NULL.
 * This metatest launches several producers, and wait for acknowledgement
 * from the producers before starting the workers.
 *
 */
void
do_metatest2(struct benchmark_config *config,
            worker_function_t worker_function,
            char *test_identifier, producer_function_t producer_function)
{
    pid_t pid;
    uint64_t total_requests;
    struct sample_data *responses;

    total_requests = config->workers * config->total_requests;

    /* Get shared memory */
    char *shmkey = xasprintf("/%s-%s", test_identifier,
                             config->process_identity);

    if ((config->responses_stats_shm_id = shm_open(shmkey,
                                                   O_CREAT | O_RDWR,
                                                   (mode_t) 0777)) < 0) {
        free(shmkey);
        perror("Can't reserve shared memory for statistics\n");
        exit(1);
    }
    /* Set the memory object's size */
    if (ftruncate(config->responses_stats_shm_id,
                  total_requests * sizeof (struct sample_data)) == -1) {
        free(shmkey);
        perror("Can't allocate shared memory for statistics\n");
        printf("\n%" PRIu64 "\n",
               total_requests * sizeof (struct sample_data));
        exit(1);
    }
    if ((responses = mmap(0, total_requests * sizeof (struct sample_data),
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED, config->responses_stats_shm_id, 0))
        == (struct sample_data *) MAP_FAILED) {
        free(shmkey);
        perror("Can't attach shared memory for statistics\n");
        exit(1);
    }

    /* Initialize the shared memory */
    memset(responses, 0, config->total_requests * config->workers *
           sizeof (struct sample_data));

    /* Clear the producer */
    config->test_start_time = nanoseconds_now();
    if (producer_function) {
        for(int i = 0; i < config->producers; i++) {
            pid = fork();
            if (!pid) {
                (*producer_function) (config);
                exit(0);
            }
        }
        /* Wait for the producers to start*/
        close(config->start_chan[0]);
        for(int i = 0; i < config->producers; i++) {
            char buf[2];
            read(config->start_chan[1], &buf, 1);
        }
    }

    /* Create the workers */
    for (int i = 0; i < config->workers; ++i) {
        pid = fork();
        if (pid < 0) {
            perror("Can't create new worker.\n");
            exit(1);
        }
        if (!pid) {
            (*worker_function) (config,
                                i + 1, &responses[i * config->total_requests]);
            exit(0);
        }
    }

    /* Wait until all the workers end */
    for (int i = 0; i < config->workers + config->producers; ++i) {
        wait(0);
    }

    /* Print the summary of the data */
    print_stats_header();
    /* Prints the stats without grouping by status */
    print_stats(config, responses, ALL_STATUSES);
    for (int i = 0; i <= TXN_ERROR; ++i) {
        /* Filters the responses by status */
        print_stats(config, responses, i);
    }

    /* Detach and free shared memory */
    close(config->responses_stats_shm_id);
    shm_unlink(shmkey);
    free(shmkey);
}
