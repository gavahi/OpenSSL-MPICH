#!/bin/bash
make clean
make 2>&1 | tee m.txt
make install 2>&1 | tee mi.txt
