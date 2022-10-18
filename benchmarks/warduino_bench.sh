#!/usr/bin/sh
# This scripts takes the name of a benchmark from the tasks folder and uploads
# it to arduino and times the execution
################################################################################

tmpfile="$(mktemp --tmpdir)"
trap "rm '$tmpfile'" EXIT
cd "$(dirname "$0")"
outloc=${2:--}

echo $1 | tee -a $2
./upload ${BOARD:-ESP32WROVER} ./tasks/$1/wast/warduino/warduino.ino 2>&1 >"$tmpfile"
if [ "$?" -eq "0" ]; then
  echo "flashed"
  python3 flash_and_check.py $1 | tee -a $2
else
  cat $tmpfile
  echo "FAILED!"
  exit 1
fi
