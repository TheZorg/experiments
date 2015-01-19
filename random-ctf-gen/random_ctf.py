#!/usr/bin/env python3
# Author: Julien Desfossez

import os
import sys
import argparse
import math
import re
try:
    from babeltrace import CTFWriter, CTFStringEncoding
except ImportError:
    # quick fix for debian-based distros
    sys.path.append("/usr/local/lib/python%d.%d/site-packages" %
                    (sys.version_info.major, sys.version_info.minor))
    from babeltrace import CTFWriter, CTFStringEncoding

# Common disk size units, used for formatting and parsing.
disk_size_units = (dict(prefix='b', divider=1, singular='byte', plural='bytes'),
                   dict(prefix='k', divider=1024**1, singular='KB', plural='KB'),
                   dict(prefix='m', divider=1024**2, singular='MB', plural='MB'),
                   dict(prefix='g', divider=1024**3, singular='GB', plural='GB'),
                   dict(prefix='t', divider=1024**4, singular='TB', plural='TB'),
                   dict(prefix='p', divider=1024**5, singular='PB', plural='PB'))

def parse_size(size):
    """
    Parse a human readable data size and return the number of bytes. Raises
    :py:class:`InvalidSize` when the size cannot be parsed.
    :param size: The human readable file size to parse (a string).
    :returns: The corresponding size in bytes (an integer).
    Some examples:
    >>> from humanfriendly import parse_size
    >>> parse_size('42')
    42
    >>> parse_size('1 KB')
    1024
    >>> parse_size('5 kilobyte')
    5120
    >>> parse_size('1.5 GB')
    1610612736
    """
    tokens = re.split(r'([0-9.]+)', size.lower())
    components = [s.strip() for s in tokens if s and not s.isspace()]
    if len(components) == 1 and components[0].isdigit():
        # If the string contains only an integer number, it is assumed to be
        # the number of bytes.
        return int(components[0])
    # Otherwise we expect to find two tokens: A number and a unit.
    if len(components) != 2:
        msg = "Expected to get two tokens, got %s!"
        raise InvalidSize(msg % components)
    # Try to match the first letter of the unit.
    for unit in reversed(disk_size_units):
        if components[1].startswith(unit['prefix']):
            return int(float(components[0]) * unit['divider'])
    # Failed to match a unit: Explain what went wrong.
    msg = "Invalid disk size unit: %r"
    raise InvalidSize(msg % components[1])

array_size = 20
event_header_size = 12
packet_header_size = 64
event_size = array_size + event_header_size

parser = argparse.ArgumentParser(description="Create a random CTF trace of a certain size.")
parser.add_argument("-s", "--size", default="32k", help="Size of output CTF trace in bytes, may use unit (e.g. 32M) (default: 32k.)")
parser.add_argument('path', help="Path of the output directory (must exist).")
args = parser.parse_args()

path = args.path
size = parse_size(args.size)

if size % 32768 != 0:
    size = size + (32768 - (size % 32768))
    print("Rounding up to nearest packet size", size)

num_events = math.floor((size - packet_header_size)/event_size)

trace_path = os.path.basename(path)

print("Writing trace at {}".format(trace_path))
writer = CTFWriter.Writer(trace_path)

clock = CTFWriter.Clock("A_clock")
clock.description = "Simple clock"

writer.add_clock(clock)
writer.add_environment_field("Python_version", str(sys.version_info))

stream_class = CTFWriter.StreamClass("test_stream")
stream_class.clock = clock

char8_type = CTFWriter.IntegerFieldDeclaration(8)
char8_type.signed = True
char8_type.encoding = CTFStringEncoding.UTF8
char8_type.alignment = 8

int32_type = CTFWriter.IntegerFieldDeclaration(32)
int32_type.signed = True
int32_type.alignment = 8

uint32_type = CTFWriter.IntegerFieldDeclaration(32)
uint32_type.signed = False
uint32_type.alignment = 8

int64_type = CTFWriter.IntegerFieldDeclaration(64)
int64_type.signed = True
int64_type.alignment = 8

array_type = CTFWriter.ArrayFieldDeclaration(char8_type, array_size)

dummy_event = CTFWriter.EventClass("dummy")

dummy_event.add_field(array_type, "dummy_field")

stream_class.add_event_class(dummy_event)
stream_class.id = 1
stream = writer.create_stream(stream_class)


def set_char_array(event, string):
    if len(string) > array_size:
        string = string[0:array_size]
    else:
        string = "%s" % (string + "\0" * (array_size - len(string)))

    for i in range(len(string)):
        a = event.field(i)
        a.value = ord(string[i])


def set_int(event, value):
    event.value = value


def write_dummy_event(time_ms, dummy_field):
    event = CTFWriter.Event(dummy_event)
    clock.time = time_ms * 1000000
    set_char_array(event.payload("dummy_field"), dummy_field)
    stream.append_event(event)
    #stream.flush()


def sched_switch_50pc(start_time_ms, end_time_ms, cpu_id, period,
                      comm1, tid1, comm2, tid2):
    current = start_time_ms
    while current < end_time_ms:
        write_sched_switch(current, cpu_id, comm1, tid1, comm2, tid2)
        current += period
        write_sched_switch(current, cpu_id, comm2, tid2, comm1, tid1)
        current += period


def sched_switch_rr(start_time_ms, end_time_ms, cpu_id, period, task_list):
    current = start_time_ms
    while current < end_time_ms:
        current_task = task_list[len(task_list) - 1]
        for i in task_list:
            write_sched_switch(current, cpu_id, current_task[0],
                               current_task[1], i[0], i[1])
            current_task = i
            current += period

#write_dummy_event(1393345613900, "anticonstitution")
#write_dummy_event(1393345613900, "anticonstitutionalist")
#write_dummy_event(1393345613900, "overenthusiastically")
for _ in range(num_events):
    #write_dummy_event(1393345613900, "anticonstitution")
    write_dummy_event(1393345613900, "overenthusiastically")
    #write_sched_switch(1393345613900, 5, "swapper/5", 0, "prog100pc-cpu5", 42)
    #sched_switch_50pc(1393345614000, 1393345615000, 0, 100,
            #"swapper/0", 0, "prog50pc-cpu0", 30664)
#sched_switch_50pc(1393345615000, 1393345616000, 0, 100,
                  #"swapper/1", 0, "prog50pc-cpu1", 30665)
#sched_switch_50pc(1393345616000, 1393345617000, 0, 100,
                  #"swapper/2", 0, "prog50pc-cpu2", 30666)
#sched_switch_50pc(1393345617000, 1393345618000, 0, 100,
                  #"swapper/3", 0, "prog50pc-cpu3", 30667)
#sched_switch_50pc(1393345618000, 1393345619000, 0, 100,
                  #"swapper/0", 0, "prog50pc-cpu0", 30664)
stream.flush()

#proc_list = [("prog1", 10), ("prog2", 11), ("prog3", 12), ("prog4", 13)]
#sched_switch_rr(1393345619000, 1393345622000, 4, 100, proc_list)
#write_sched_switch(1393345622300, 5, "prog100pc-cpu5", 42, "swapper/5", 0)
