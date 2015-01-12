#!/bin/bash

# Creates a 1G file at given path
dd if=/dev/urandom of=$1 bs=1G count=1
