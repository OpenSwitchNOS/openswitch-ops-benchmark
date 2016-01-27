#!/usr/bin/env python
########################################################################
# Copyright (C) 2015 Hewlett Packard Enterprise Development LP
# All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License. You may obtain
# a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.
########################################################################

# This script takes a group of CSV files and generates a XLSX workbook
# It receives as a parameter the path to the tests/ folder containing
# the CSV files generated with the banchmark tool
#
# Usage:
# $ python csv2xls.py <path to tests folder>

import os
import csv
import re
import sys
import xlsxwriter

scrptPth = os.path.dirname(sys.argv[1])
for root, dirs, files in os.walk(scrptPth):
    for folder in dirs:
        workbook = xlsxwriter.Workbook(folder + '_test.xlsx')

        for filename in os.listdir(scrptPth + '/' + folder):

            if filename.endswith('.csv'):

                csvfile=filename;
                sheet_name=re.split(r'/',csvfile);
                sheet_name=re.split(r'\.',sheet_name[-1]);
                curr_sheet = workbook.add_worksheet(sheet_name[0])

                with open(scrptPth + '/' + folder + '/' + filename, 'rb') as f:
                    reader = csv.reader(f)
                    for r, row in enumerate(reader):
                        for c, val in enumerate(row):
                            curr_sheet.write(r, c, val)

        workbook.close()
