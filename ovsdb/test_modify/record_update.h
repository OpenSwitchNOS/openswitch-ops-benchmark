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
 * The "record update" test do a number of updates over a smaller set
 * of rows. Several workers select a record at random, so the probability
 * of a record being updated by more than one worker can be set.
 */
#ifndef OVSDB_BENCHMARK_RECORD_UPDATE_H_
#define OVSDB_BENCHMARK_RECORD_UPDATE_H_

#include "common/common.h"

void record_update_help(char *binary_name);
void do_record_update_test(struct benchmark_config *);

#endif /* OVSDB_BENCHMARK_RECORD_UPDATE_H_ */
