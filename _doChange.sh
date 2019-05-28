#!/bin/sh


if test -n "$WARDUINO_DEV"
then
    target="$WARDUINO_DEV"
    write () {
        (tr -d '\n\t ' ; echo"") | tr '[:lower:]' '[:upper:]' | tee $WARDUINO_DEV
    }
else
    write () {
        # xxd -r -p > /tmp/change
        (tr -d '\n\t ' ; echo"") | tr '[:lower:]' '[:upper:]' | tee /tmp/change
        kill -USR1 $(pgrep warduino)
    }
fi

#echo "AAcAQeQAEAIL" | base64 -d > /tmp/change
case "$1" in
"REPLACE")
cat <<HERE | sed 's/#.*//'| write
10                # Replace function (hard)
${2:-00}                # Function id (excluding imported functions)
${3:-070041e40010020b}  # New body
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
bp=${2:-55a5994fa3d6}
bpl=$(printf "%02x" $(expr ${#bp} / 2 ))
cat <<HERE | sed 's/#.*//' | write
06 # BP 
$bpl # LEN ptr
$bp # ptr
HERE
;;


"BPR") # Remove a breapoint
bp=${2:-55a5994fa3d6}
bpl=$(printf "%02x" $(expr ${#bp} / 2 ))
cat <<HERE | sed 's/#.*//' | write
07 # BP remove
$bpl # LEN ptr
$bp # ptr
HERE
;;

*)
cat <<HELP
Usage: $0 TASK [DEVICE]

TASK:
$(cat "$0" | grep '^".*")' | column -t -s')' | sed 's/^/  /')

DEVICE:
  The location of the serial device on the system
$(find /dev/serial \( -type l -o -type c \) -exec realpath '{}' \; 2> /dev/null | sort | uniq | sed 's/^/  /')


HELP

;;
esac
