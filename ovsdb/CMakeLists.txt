# Copyright (C) 2015 Hewlett Packard Enterprise Development LP
# All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License. You may obtain
# a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.

cmake_minimum_required (VERSION 2.8.1)

PROJECT(ovsdb-benchmark)

# Find dependencies using pkg-config
include(FindPkgConfig)
pkg_check_modules(OVSCOMMON libovscommon)
pkg_check_modules(OVSDB libovsdb)

# Compiler flags
add_definitions(-std=gnu99)

######## Build and include settings ########
include_directories(
	${PROJECT_BINARY_DIR}
	${PROJECT_SOURCE_DIR}
	${OVSCOMMON_INCLUDE_DIRS}
)

file(GLOB SOURCES
	"ovsdb-benchmark.c"
	"common/*.c"
	"test_insert/*.c"
	"test_transaction_size/*.c"
	"test_memory_usage/*.c"
	"test_modify/*.c"
	"test_pubsub/*.c"
	"counters_pubsub/*.c"
	"test_insertion_time/*.c"
)

file(GLOB MINIMALSOURCES
	"minimal-example.c"
)

add_executable(
	ovsdb-benchmark
	${SOURCES}
)

add_executable(
	minimal-example
	${MINIMALSOURCES}
)

target_link_libraries(ovsdb-benchmark
	rt
	${OVSDB_LIBRARIES}
	${OVSCOMMON_LIBRARIES}
)

target_link_libraries(minimal-example
	${OVSDB_LIBRARIES}
	${OVSCOMMON_LIBRARIES}
)

# Install the daemon on bin directory
INSTALL(TARGETS ovsdb-benchmark
	RUNTIME DESTINATION bin
)
