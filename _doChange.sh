#!/bin/sh
#echo "AAcAQeQAEAIL" | base64 -d > /tmp/change
case "$1" in
"REPLACE")
cat <<HERE | sed 's/#.*//'| xxd -r -p > /tmp/change
10                # Replace function (hard)
00                # Function id (excluding imported functions)
070041e40010020b  # New body
HERE
;;

"RUN") # continue execution
echo "01" | xxd -r -p > /tmp/change
;;
"STOP") # stop execution
echo "02" | xxd -r -p > /tmp/change
;;
"PAUSE") # pause execution
echo "03" | xxd -r -p > /tmp/change
;;
"STEP") # execute one step
echo "04" | xxd -r -p > /tmp/change
;;

*)
echo "give an action:"
cat "$0" | grep '^".*")'
;;
esac
