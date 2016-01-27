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

#ifndef OVSDB_BENCHMARK_INSERT_INDEX_TIME_H_
#define OVSDB_BENCHMARK_INSERT_INDEX_TIME_H_

#include "common/common.h"

int run_insert_index_tests(struct benchmark_config *configuration);
void insertion_index_time_help(char *binary_name);
void do_insertion_index_time(struct benchmark_config* configuration);

#endif /* insert-index-time.h */
