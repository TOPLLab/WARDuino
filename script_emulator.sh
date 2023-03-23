#!/bin/bash

OUTPUT=build-emu
rm -rf ${OUTPUT}
mkdir ${OUTPUT}
cd ./${OUTPUT}
cmake .. --log-level=debug -D BUILD_EMULATOR=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=1 . && make

PATHFORCOMP=../
rm ${PATHFORCOMP}/compile_commands.json
cp compile_commands.json ${PATHFORCOMP}/
