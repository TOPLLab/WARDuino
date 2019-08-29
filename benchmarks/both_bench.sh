#!/bin/bash
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

echo "# Espruino"
cat $tmpfile_e
echo "# Warduino"
cat $tmpfile_w

sizes () {
  find tasks -iname "*.$1" -exec du -b '{}' \+ | sed 's:\s*tasks/:,:;s:/.*::;s:\(.*\),\(.*\):\2,\1:' | sort
}

echo "name,espruino,warduino,espruinoSize,warduinoSize" > $file.csv
join -j 1 -t',' <(sort $tmpfile_e) <(sort $tmpfile_w) |\
join -j 1 -t',' - <(sizes js) |\
join -j 1 -t',' - <(sizes wasm) >> $file.csv
sed 's/,/ /g' $file.csv >  $file

cat $file.csv | \
  sed 's/[0-9]\+\./     &/g;s/ *\([0-9 ]\{3\}\.\)/\1/g;s/\.\([0-9]\{4\}\)[0-9]*/.\1/g' |\
  column -t -s','