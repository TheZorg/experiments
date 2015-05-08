#!/usr/bin/python3
import sys
import subprocess
import argparse
import os
import csv
import re

class termcolors:
    blue="\033[0;34m"
    red="\033[0;31m"
    cyan="\033[0;36m"
    NC="\033[0m"

if __name__ == "__main__":
    if os.geteuid() != 0:
        print("Must be root to flush cache")
        sys.exit(-1)

    parser = argparse.ArgumentParser(description="Benchmarking")
    parser.add_argument('input')
    parser.add_argument('output', nargs='?', default="out.csv")
    parser.add_argument('--threads', default=8, type=int)
    parser.add_argument('--runs', default=1, type=int)
    args = parser.parse_args()

    if not os.path.exists(args.input):
        print("Input file does not exist")
        sys.exit(-1)

    csv_file = open(args.output, "w")
    writer = csv.writer(csv_file)
    writer.writerow(("threads","iterations","cache_cold","bandwidth"))

    iterations = [1, 10, 100, 1000, 10000, 100000]
    program_name="./pipelined-io-test"
    cache_cold="../scripts/cache_cold.sh"

    for i in iterations:
        threads = 1
        while threads <= args.threads:
            print(termcolors.cyan + 
                    "Testing with %s iterations, %s threads" % (i, threads) +
                    termcolors.NC)
            call_args = [program_name, "-t", str(threads), "-i", str(i), "-n", str(64), args.input]

            print(termcolors.blue + "Cache cold" + termcolors.NC)
            print("Run: ", end="", flush=True)
            for r in range(args.runs):
                print(str(r+1), end=" ", flush=True)
                call_output = subprocess.check_output([cache_cold] + call_args).decode("utf-8")
                bandwidth = re.findall("Bandwidth.*$", call_output)[0].split()[-1]
                writer.writerow((threads,i,1,bandwidth))

            print("")
            print(termcolors.red + "Cache hot" + termcolors.NC)
            print("Run: ", end="", flush=True)
            for r in range(args.runs):
                print(str(r+1), end=" ", flush=True)
                call_output = subprocess.check_output(call_args).decode("utf-8")
                bandwidth = re.findall("Bandwidth.*$", call_output)[0].split()[-1]
                writer.writerow((threads,i,0,bandwidth))

            print("")
            threads = threads * 2

    csv_file.close()
