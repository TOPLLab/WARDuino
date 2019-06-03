#!/usr/bin/sh
# Name: Upload all programs in bench.list to arduino and time
# By Robbert Gurdeep Singh
################################################################################
cd "$(dirname "$0")"
date > $1
make -C tasks all

cat bench.list | while read l;
do
echo $l | tee -a $1
../scripts/upload ESP32 ./tasks/$l/wast/arduino/arduino.ino 2>/dev/null
if test "$?" == 0
then
    python flash_and_check.py 2>/dev/null | tee -a $1
else
    echo "FAILED!"
fi
done
