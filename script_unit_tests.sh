#!/bin/bash

PATHFORCOMP=../tests/vm_unit_tests

rm -rf build-test
mkdir build-test
cd build-test
cmake .. --log-level=debug -D BUILD_UNITTEST=ON . && make
rm ${PATHFORCOMP}/compile_commands.json
cp compile_commands.json ${PATHFORCOMP}/

