#!/bin/sh
#echo "AAcAQeQAEAIL" | base64 -d > /tmp/change

cat <<HERE | sed 's/#.*//'| xxd -r -p > /tmp/change
00                # Function id (excluding imported functions)
070041e40010020b  # New body
HERE