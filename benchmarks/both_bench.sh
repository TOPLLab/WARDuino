#!/usr/bin/sh
# Name: Test both
# By Robbert Gurdeep Singh
################################################################################
set -e
file=${1?Give an output file as argument}
tmpfile_w="$(mktemp --tmpdir)"
tmpfile_e="$(mktemp --tmpdir)"
trap "rm '$tmpfile_w' '$tmpfile_e'" EXIT

to_csv () {
  sed -i -n '0~2{N;s/\n/,/p}' $1
}

sleep 5
./espruino_bench.sh $tmpfile_e
to_csv  $tmpfile_e

sleep 5
./warduino_bench.sh $tmpfile_w
to_csv $tmpfile_w


echo "name,espruino,warduino" > $file.csv
join -j 1 -t',' $tmpfile_e $tmpfile_w >> $file.csv
sed 's/,/ /g' $file.csv >  $file

cat $file.csv | \
  sed 's/[0-9]\+\./     &/g;s/ *\([0-9 ]\{3\}\.\)/\1/g;s/\.\([0-9]\{4\}\)[0-9]*/.\1/g' |\
  column -t -s','