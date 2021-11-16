#!/bin/sh
# Update core specification testsuite
set -e -u

testsuite="core"
testsuite_url="https://github.com/WebAssembly/testsuite"

update_repo() {
    local url=$1
    local dir=$2
    if [ -d ${dir} ]; then
        git -C ${dir} restore .
        git -C ${dir} pull origin
    else
        git clone ${url} ${dir}
    fi
}

update_repo ${testsuite_url} ${testsuite}

echo "done"

