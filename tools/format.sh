#!/bin/bash

git ls-files -c -o --exclude-standard | egrep '\.(cpp|hpp|json)' | xargs clang-format -i
