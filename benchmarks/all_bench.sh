#!/bin/bash
# Name: Test WARDuino, Espruino and native code
# By Robbert Gurdeep Singh
################################################################################
set -e
file=${1?Give an output file as argument}
tmpdir="$(mktemp --tmpdir -d)"
trap "rm -rf '$tmpdir'" EXIT

echo -e "tip:\ntail -f $tmpdir/*"

echo "Not started yet" >$tmpdir/wasm3
echo "Not started yet" >$tmpdir/espruino
echo "Not started yet" >$tmpdir/warduino
echo "Not started yet" >$tmpdir/native

to_csv() {
  sed -i -n '0~2{N;s/\n/,/p}' $1
}

sleep 5
./espruino_bench.sh $tmpdir/espruino
to_csv $tmpdir/espruino

sleep 5
./warduino_bench.sh $tmpdir/warduino
to_csv $tmpdir/warduino

sleep 5
./wasm3_bench.sh $tmpdir/wasm3
to_csv $tmpdir/wasm3

sleep 5
./native_bench.sh $tmpdir/native
to_csv $tmpdir/native

echo "# Espruino"
cat $tmpdir/espruino
echo "# Warduino"
cat $tmpdir/warduino
echo "# Wasm3"
cat $tmpdir/wasm3
echo "# Native"
cat $tmpdir/native

sizes() {
  find tasks -iname "*.$1" -exec du -b '{}' \+ | sed 's:\s*tasks/:,:;s:/.*::;s:\(.*\),\(.*\):\2,\1:' | sort
}

echo "name,espruino,warduino,wasm3,native,espruinoSize,warduinoSize" >$file.csv
sort $tmpdir/espruino |
  join -j 1 -t',' - <(sort $tmpdir/warduino) |
  join -j 1 -t',' - <(sort $tmpdir/wasm3) |
  join -j 1 -t',' - <(sort $tmpdir/native) |
  join -j 1 -t',' - <(sizes js) |
  join -j 1 -t',' - <(sizes wasm) >>$file.csv
sed 's/,/ /g' $file.csv >$file

cat $file.csv |
  sed 's/[0-9]\+\./     &/g;s/ *\([0-9 ]\{3\}\.\)/\1/g;s/\.\([0-9]\{4\}\)[0-9]*/.\1/g' |
  column -t -s','
