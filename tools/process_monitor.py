#!/usr/bin/env python

import sys, os, signal, logging
from time import *
from lib.daemonize import Daemonize

cpu_time = {'initial': 0, 'final': 0}
final = {'utime': 0, 'cutime': 0}
initial = {'utime':0, 'cutime': 0}
elapsed_current_time = {'initial': 0, 'final': 0}

current_date = str(localtime().tm_year)+str(localtime().tm_mon)+str(localtime().tm_mday)

pid_process_monitor = "/tmp/pm.pid"

def get_pid_of(process):
    """ get_pid_of method
    Will return the process id by given process name
    """
    try:
        pids = [pid for pid in os.listdir('/proc') if pid.isdigit()]
        for pid in pids:
            if process in open(os.path.join('/proc', pid, 'cmdline'), 'rb').read():
                return pid
    except IOError as e:
        print "Unable to identify the Process ID for %s\n" % process
        sys.exit(2)

def get_stat_info(daemon_pid):
    """ get_stat_info method
    Will gather and write the stat info in a csv file.
    """
    global cpu_time, current_date, initial, final, elapsed_current_time

    if sys.argv[2] == '-pname':
        pid=get_pid_of(sys.argv[3])
        try:
            if pid == str(open(daemon_pid,'r').readline()):
                print "This process cannot be monitorized.\n"
                sys.exit(2)
        except IOError as e:
            print "An error occurred when trying to open the process with pid %s\n" % pid
            sys.exit(2)
    else:
        pid=sys.argv[3]

    try:
        f = open('/tmp/process_monitor_stats_'+current_date+'.csv', 'a')

    except IOError as e:
        print "An error occurred when trying to create the csv file.\n"
        sys.exit(2)

    try:
        hertz_list = open('/proc/stat','r').readline().split(' ')

    except IOError as e:
        print "An error ocurred when trying to open the /proc/stat file.\m"
        sys.exit(2)

    hertz = 0

    for i in range(2,len(hertz_list)-1):
        hertz += int(hertz_list[i])

    try:
        with open(os.path.join('/proc/', pid, 'stat'), 'r') as pidfile:
            proctime = pidfile.readline().split(' ')
            utime = int(proctime[13])
            stime = int(proctime[14])
            cutime = int(proctime[15])
            cstime = int(proctime[16])
            starttime = int(proctime[21])
            vsize = int(proctime[22])
            rss = int(proctime[23])

            if cpu_time['initial'] == 0:
                cpu_time['initial']=hertz
                cpu_time['final']=hertz
            else:
                cpu_time['initial']=cpu_time['final']
                cpu_time['final']=hertz

            if initial['utime'] == 0:
                initial['utime']=utime
                initial['cutime']=cutime
                final['utime']=utime
                final['cutime']=cutime
            else:
                initial['utime']=final['utime']
                initial['cutime']=final['cutime']
                final['utime']=utime
                final['cutime']=cutime

            if elapsed_current_time['initial'] == 0:
                elapsed_current_time['initial'] = int(round(time() * 1000))
                elapsed_current_time['final'] = int(round(time() * 1000))
            else:
                elapsed_current_time['final'] = int(round(time() * 1000))

            total_time = cpu_time['final'] - cpu_time['initial']

            if total_time != 0:
                cpu_usage = 100.00 * ( (final['cutime']+final['utime']) - (initial['utime']+initial['cutime']) ) / total_time
            else:
                cpu_usage=0

            total_elapsed_current_time = elapsed_current_time['final'] - elapsed_current_time['initial']

            log = str(total_elapsed_current_time)+','+strftime('%X')+','+str(total_time)+','+str(cpu_usage)+','+str(vsize)+','+str(rss)+'\n'

            f.write(log)
    except IOError as e:
        print "Oh oh Something went wrong...\n"
        sys.exit(2)

    f.close()


def prepare_report():
    """ prepare_report method
    It writes the csv header.
    """
    try:
        f = open('/tmp/process_monitor_stats_'+current_date+'.csv', 'a')
        file_header = 'Elapsed Current Time'+','+'Current Time'+','+'CPU Ticks'+','+'CPU Usage per Core'+','+'Virtual memory size in bytes'+','+'Resident Set Size\n'
        f.write(file_header)
        f.close()
    except IOError as e:
        print "An error occurred when trying to create the csv file.\n"
        sys.exit(2)

def print_error_message():
    """ print_error_message method
    Prints a default error message with the command usage instructions.
    """
    print "usage: %s start -pid [process id] | start -pname [process name] | stop\n\n" % sys.argv[0]

def represents_int(s):
    """ represents_int method
    Returns True or False if the string passed represents an integer
    """
    try:
        int(s)
        return True
    except ValueError:
        return False

def main():
    """ main method
    This main method will loop as long as the daemon is alive.
    """
    while True:
        get_stat_info(pid_process_monitor)
        sleep(1)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)
logger.propagate = False
fh = logging.FileHandler("/tmp/process_monitor_debug.log", "w")
fh.setLevel(logging.DEBUG)
logger.addHandler(fh)
keep_fds = [1]

daemon = Daemonize(app="process_monitor", pid=pid_process_monitor, action=main, keep_fds=keep_fds, logger=logger, verbose=True)

if len(sys.argv) >= 2:
    if ('start' == sys.argv[1] and len(sys.argv) >= 4):
        if ('-pid' == sys.argv[2] and represents_int(sys.argv[3])) or '-pname' == sys.argv[2]:
            prepare_report()
            daemon.start()
        else:
            print_error_message()
            sys.exit(2)
    elif 'stop' == sys.argv[1]:
        try:
            pid = open(daemon.pid,'r').readline()
            os.kill(int(pid), signal.SIGKILL)
            sys.exit(0)
        except IOError as e:
            print("The Process Monitor haven\'t started yet.\n")
            sys.exit(2)
    else:
        print_error_message()
        sys.exit(2)
    sys.exit(0)
else:
    print_error_message()
    sys.exit(2)
