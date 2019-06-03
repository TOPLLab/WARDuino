#!/usr/bin/sh
# Name: Upload all programs in bench.list to arduino and time
# By Robbert Gurdeep Singh
################################################################################

file=${1:-/tmp/res}
cd "$(dirname "$0")"
date > $file

./flash-espruino-esp32.sh

echo "Sleep 5 sec till espruino boots"
sleep 5


cat bench.list | while read l;
do
echo $l | tee -a $file
python flash_and_check.py tasks/$l/espruino/impl.js | tee -a $file
done
