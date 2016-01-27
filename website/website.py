#!/usr/bin/env python
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

import site
import os.path
import sys
sys.path, remainder = sys.path[:1], sys.path[1:]
site.addsitedir(os.path.join(os.path.dirname(__file__), "lib"))
sys.path.extend(remainder)

import flask
import json
import subprocess
from flask import Flask, request, redirect, url_for, \
     abort, render_template, flash, make_response
app = Flask(__name__)
app.config.from_object(__name__)
DEBUG = True
SECRET_KEY = 'A0Zr9as8j/3yX R~XHH!jmN]ibe3LWX/,?RT'
TXN_VALUES = ["UNCOMMITTED", "UNCHANGED", "INCOMPLETE",
           "ABORTED", "SUCCESS", "TRY_AGAIN",
            "NOT_LOCKED", "ERROR"]
COLORS = ['black', 'blue', 'orange', 'red', 'green', 'magenta', 'teal', 'red']

def clean_ovsdb(reset=False):
    subprocess.Popen(["docker", "exec", "h1",
                      "ovsdb-client", "transact",
        '["OpenSwitch", {"op":"delete","table":"Test","where":[]},{"op":"commit","durable":true}]']).wait()
    subprocess.Popen(["docker", "exec", "h1",
        "ovs-appctl", "-t",  "ovsdb-server", "ovsdb-server/compact"]).wait()
    if reset:
        subprocess.Popen(["docker", "exec", "h1",
            "systemctl", "restart", "ovsdb-server"]).wait()


@app.route('/')
def benchmark_home():
    return render_template('home.html')

@app.route('/js/<path:filename>')
def js_static(filename):
    return flask.send_from_directory(app.root_path + '/js/', filename)

@app.route('/css/<path:filename>')
def css_static(filename):
    return flask.send_from_directory(app.root_path + '/css/', filename)

# OVSDB Benchmarks
@app.route('/ovsdb/insert', methods=['GET', 'POST'])
def ovsdb_insert():
    if request.method == 'POST':
        try:
            clean_ovsdb()
            quantity = request.form['quantity']
            workers  = request.form['workers']
            usecache = request.form['cache']
            # Save the configuration:
            with open('results/ovsdb-insert.json', 'w') as cfg_file:
                json.dump({
                    'quantity': quantity,
                    'workers': workers,
                    'usecache': usecache
                }, cfg_file)
            with open('results/ovsdb-insert.csv', "w") as output:
                subprocess.Popen(["docker", "exec", "h1", "ovsdb-benchmark", "insert", "-w", str(workers), "-n", str(quantity)],
                                stdout=output).wait()
        except:
            flash("Error processing the test. Please check the arguments and try again.")
        finally:
            clean_ovsdb()
            return redirect(url_for('ovsdb_insert'))
    else:
        try:
            with open("results/ovsdb-insert.json") as cfg_file:
                cfg = json.load(cfg_file)
            return render_template('ovsdb_insert.html', results=cfg)
        except:
            return render_template('ovsdb_insert.html')

@app.route('/ovsdb/update', methods=['GET', 'POST'])
def ovsdb_update():
    if request.method == 'POST':
        try:
            clean_ovsdb()
            quantity = request.form['quantity']
            workers  = request.form['workers']
            poolsize = request.form['poolsize']
            usecache = request.form['cache']
            # Save the configuration:
            with open('results/ovsdb-update.json', 'w') as cfg_file:
                json.dump({
                    'quantity': quantity,
                    'workers': workers,
                    'usecache': usecache,
                    'poolsize': poolsize
                }, cfg_file)
            with open('results/ovsdb-update.csv', "w") as output:
                subprocess.Popen(["docker", "exec", "h1", "ovsdb-benchmark",
                                "update",
                                "-w", str(workers),
                                "-n", str(quantity),
                                "-m", str(poolsize)],
                                stdout=output).wait()
        except:
            flash("Error processing the test. Please check the arguments and try again.")
        finally:
            clean_ovsdb()
            return redirect(url_for('ovsdb_update'))
    else:
        try:
            with open("results/ovsdb-update.json") as cfg_file:
                cfg = json.load(cfg_file)
            return render_template('ovsdb_update.html', results=cfg)
        except:
            return render_template('ovsdb_update.html')

@app.route('/ovsdb/pubsub', methods=['GET', 'POST'])
def ovsdb_pubsub():
    if request.method == 'POST':
        try:
            clean_ovsdb()
            quantity = request.form['quantity']
            workers  = request.form['workers']
            usecache = request.form['cache']
            # Save the configuration:
            with open('results/ovsdb-pubsub.json', 'w') as cfg_file:
                json.dump({
                    'quantity': quantity,
                    'workers': workers,
                    'usecache': usecache
                }, cfg_file)
            with open('results/ovsdb-pubsub.csv', "w") as output:
                subprocess.Popen(["docker", "exec", "h1", "ovsdb-benchmark",
                                "pubsub",
                                "-w", str(workers),
                                "-n", str(quantity)],
                                stdout=output).wait()
        except:
            flash("Error processing the test. Please check the arguments and try again.")
        finally:
            clean_ovsdb()
            return redirect(url_for('ovsdb_pubsub'))
    else:
        try:
            with open("results/ovsdb-pubsub.json") as cfg_file:
                cfg = json.load(cfg_file)
            return render_template('ovsdb_pubsub.html', results=cfg)
        except:
            return render_template('ovsdb_pubsub.html')

@app.route('/ovsdb/counters', methods=['GET', 'POST'])
def ovsdb_counters():
    if request.method == 'POST':
        try:
            clean_ovsdb()
            quantity = request.form['quantity']
            workers  = request.form['workers']
            poolsize = request.form['poolsize']
            # Save the configuration:
            with open('results/ovsdb-counters.json', 'w') as cfg_file:
                json.dump({
                    'quantity': quantity,
                    'workers': workers,
                    'poolsize': poolsize
                }, cfg_file)
            with open('results/ovsdb-counters.csv', "w") as output:
                subprocess.Popen(["docker", "exec", "h1", "ovsdb-benchmark",
                                "counters",
                                "-w", str(workers),
                                "-n", str(quantity),
                                "-m", str(poolsize)],
                                stdout=output).wait()
        except:
            flash("Error processing the test. Please check the arguments and try again.")
        finally:
            clean_ovsdb()
            return redirect(url_for('ovsdb_counters'))
    else:
        try:
            with open("results/ovsdb-counters.json") as cfg_file:
                cfg = json.load(cfg_file)
            return render_template('ovsdb_counters.html', results=cfg)
        except:
            return render_template('ovsdb_counters.html')

@app.route('/ovsdb/transaction-size', methods=['GET', 'POST'])
def ovsdb_transaction_size():
    if request.method == 'POST':
        try:
            clean_ovsdb()
            quantity = request.form['quantity']
            # Save the configuration:
            with open('results/ovsdb-transaction-size.json', 'w') as cfg_file:
                json.dump({'test': "transaction-size"}, cfg_file)
            with open('results/ovsdb-transaction-size.csv', "w") as output:
                output.write("single_transaction,requests,vsize,rss,ucpu,scpu,duration\n")
                output.flush()
                requests = [i for i in [1, 10, 100, 1000,
                            10000, 20000, 30000, 40000, 50000,
                            60000, 70000, 80000, 90000,
                            100000, 200000, 300000, 400000, 500000,
                            600000, 700000, 800000, 900000, 1000000]
                            if i <= quantity]
                for i in requests:
                    # One transaction per insert
                    subprocess.Popen(["docker", "exec", "h1", "ovsdb-benchmark",
                                "transaction-size",
                                "-n", str(i)],
                                stdout=output).wait()
                    clean_ovsdb()
                    # Single transaction test
                    subprocess.Popen(["docker", "exec", "h1", "ovsdb-benchmark",
                                "transaction-size",
                                "-n", str(i), "-s"],
                                stdout=output).wait()
                    clean_ovsdb()
        except:
            flash("Error processing the test. Please check the arguments and try again.")
        finally:
            clean_ovsdb()
            return redirect(url_for('ovsdb_transaction_size'))
    else:
        try:
            with open("results/ovsdb-transaction-size.json") as cfg_file:
                cfg = json.load(cfg_file)
            return render_template('ovsdb_transaction-size.html', results=cfg)
        except:
            return render_template('ovsdb_transaction-size.html')

def read_results(test):
    import csv
    with open("results/%s.csv" % test, 'r') as file:
        return [row for row in csv.DictReader(file)]

def positions(elements):
    return list(xrange(len(elements)))

@app.route("/plot/<plot_type>/<test>.png")
def generate_plot(plot_type=None, test=None):
    """
    Generates a plot from the data saved in the folder results.
    """
    # First try to read the results file:
    data = read_results(test)

    from matplotlib.backends.backend_agg import FigureCanvasAgg as FigureCanvas
    from matplotlib.figure import Figure
    import StringIO
    import numpy as np

    fig=Figure()
    ax=fig.add_subplot(111)
    if plot_type is None:
        return ""
    elif plot_type == "throughtput-latency":
        for i, value in enumerate(TXN_VALUES):
            ax.scatter([row['avg_duration'] for row in data if row['status'] == str(i)],
                       [row['count'] for row in data if row['status'] == str(i)],
                       label=value, color=COLORS[i])
        ax.set_xlabel("Average Response Time (microseconds)");
        ax.set_ylabel("Responses per Second");
        ax.set_ylim(bottom=0.)
        ax.set_xlim(left=0.)
    elif plot_type == "responses":
        ax.plot([row['min_duration'] for row in data if row['status'] == "-1"], 'k-', label="Min Duration")
        ax.plot([row['max_duration'] for row in data if row['status'] == "-1"], 'b-', label="Max Duration")
        ax.plot([row['avg_duration'] for row in data if row['status'] == "-1"], 'g-', label="Avg Duration")
        ax.set_xlabel('Time (s)')
        # Make the y-axis label and tick labels match the line color.
        ax.set_ylabel('Response Duration (microseconds)', color='b')
        for tl in ax.get_yticklabels():
            tl.set_color('b')
        ax2 = ax.twinx()
        pos = positions([row for row in data if row['status'] == "-1"])
        rcount = [row['count'] for row in data if row['status'] == "-1"]
        ax2.plot(rcount, 'r-', label="Responses per Second")
        ax2.set_ylabel('Responses per Second', color='r')
        for tl in ax2.get_yticklabels():
            tl.set_color('r')
        ax.set_yscale('log')
    elif plot_type == "memory":
        ax.plot([row['rss'] for row in data if row['status'] == "-1"], label="RSS")
        ax.plot([row['vsize'] for row in data if row['status'] == "-1"], label="VSIZE")
        ax.set_xlabel("Time (seconds)");
        ax.set_ylabel("Memory Usage (bytes)");
    elif plot_type == "cpu":
        ax.plot([row['ucpu'] for row in data if row['status'] == "-1"], label="User CPU")
        ax.plot([row['scpu'] for row in data if row['status'] == "-1"], label="System CPU")
        ax.set_xlabel("Time (seconds)")
        ax.set_ylabel("CPU % Usage")
    elif plot_type in ["mtmemory", "stmemory"]:
        is_single = str(["mtmemory", "stmemory"].index(plot_type))
        ax.plot([row['requests'] for row in data if row['single_transaction'] == is_single],
                [row['rss'] for row in data if row['single_transaction'] == is_single],
                label="RSS")
        ax.plot([row['requests'] for row in data if row['single_transaction'] == is_single],
                [row['vsize'] for row in data if row['single_transaction'] == is_single],
                label="VSIZE")
        ax.set_xlabel("Records Inserted")
        ax.set_ylabel("Memory Usage (bytes)")
    elif plot_type in ["mtcpu", "stcpu"]:
        is_single = str(["mtcpu", "stcpu"].index(plot_type))
        ax.plot([row['requests'] for row in data if row['single_transaction'] == is_single],
                [row['ucpu'] for row in data if row['single_transaction'] == is_single],
                label="User CPU")
        ax.plot([row['requests'] for row in data if row['single_transaction'] == is_single],
                [row['scpu'] for row in data if row['single_transaction'] == is_single],
                label="System CPU")
        ax.set_xlabel("Records Inserted")
        ax.set_ylabel("CPU % Usage")
    elif plot_type in ["multiple-transaction", "single-transaction"]:
        is_single = str(["multiple-transaction", "single-transaction"].index(plot_type))
        ax.plot([row['requests'] for row in data if row['single_transaction'] == is_single],
                [row['duration'] for row in data if row['single_transaction'] == is_single],
                label="Insertion Time")
        ax.set_xlabel("Records Inserted")
        ax.set_ylabel("Duration (microseconds)")
    else:
        return plot_type
    ax.legend(loc='best')
    fig.tight_layout()
    canvas=FigureCanvas(fig)
    png_output = StringIO.StringIO()
    canvas.print_png(png_output)
    response=make_response(png_output.getvalue())
    response.headers['Content-Type'] = 'image/png'
    return response

if __name__ == "__main__":
    app.run(debug=True)
