#!/usr/bin/sh
# Name: Upload all programs in bench.list to arduino and time
# By Robbert Gurdeep Singh
################################################################################
tmpfile="$(mktemp --tmpdir)"
trap "rm '$tmpfile'" EXIT
cd "$(dirname "$0")"
outloc=${1:--}
date >$1
make clean all
make -C tasks all

cat bench.list | while read l; do
  echo $l | tee -a $1
  ../scripts/upload ${BOARD:-ESP32WROVER} ./tasks/$l/c/arduino.ino 2>&1 >"$tmpfile"
  if [ "$?" -eq "0" ]; then
    echo "flashed"
    python flash_and_check.py | tee -a $1
  else
    cat $tmpfile
    echo "FAILED!"
    exit 1
  fi
done
