#!/bin/sh


if test -n "$2"
then
    target="$2"
    write () {
        (tr -d '\n\t ' ; echo"") | tr '[:lower:]' '[:upper:]' > $target
    }
else
    write () {
        # xxd -r -p > /tmp/change
        (tr -d '\n\t ' ; echo"") | tr '[:lower:]' '[:upper:]' > /tmp/change
        kill -USR1 $(pgrep warduino)
    }
fi

#echo "AAcAQeQAEAIL" | base64 -d > /tmp/change
case "$1" in
"REPLACE")
cat <<HERE | sed 's/#.*//'| write
10                # Replace function (hard)
00                # Function id (excluding imported functions)
070041e40010020b  # New body
HERE
;;

"RUN") # continue execution
echo "01" | write
;;
"STOP") # stop execution
echo "02" | write
;;
"PAUSE") # pause execution
echo "03" | write
;;
"STEP") # execute one step
echo "04" | write
;;

"DUMP") # execute one step
echo "05" | write
;;

"BP") # Add a breapoint
cat <<HERE | sed 's/#.*//' | write
06 # BP 
06 # LEN ptr
55a5994fa3d6 # ptr
HERE
;;


"BPR") # Remove a breapoint
cat <<HERE | sed 's/#.*//' | write
07 # BP remove
06 # LEN ptr
55a5994fa3d6 # ptr
HERE
;;

*)
cat <<HELP
Usage: $0 TASK [DEVICE]

TASK:
$(cat "$0" | grep '^".*")' | sed 's/^/  /')

DEVICE:
$(find /dev/serial \( -type l -o -type c \) -exec realpath '{}' \; | sort | uniq | sed 's/^/  /')


HELP

;;
esac
