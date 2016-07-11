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
 *
 * @file
 * Provides resource handling and results gathering for runnning tests.
 * The metatest runs the callbacks passed as parameter according to the
 * given configuration.
 */

#ifndef OVSDB_METATEST_WORKERS_H_
#define OVSDB_METATEST_WORKERS_H_

typedef void (*worker_function_t) (const struct benchmark_config *,
                                   int, struct sample_data *);

typedef void (*producer_function_t) (const struct benchmark_config *);

void do_metatest(struct benchmark_config *,
                 worker_function_t, char *, producer_function_t);

void do_metatest2(struct benchmark_config *,
                  worker_function_t, char *, producer_function_t);

#endif /* OVSDB_METATEST_WORKERS_H_ */
