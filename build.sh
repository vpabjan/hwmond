#!/bin/bash
gcc -O2 -Wall -Wextra -o hwctl hwctl.c
gcc -O2 -Wall -Wextra -o hwmond hwmond.c -lm
