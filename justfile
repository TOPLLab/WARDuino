set unstable

default:
    just --list


## Run

[group('run')]
[doc('Run program on any platform (rebuilds)')]
run platform program:
    just stage {{program}}
    just build {{platform}}
    just flash {{platform}} upload.wasm
    just monitor {{platform}}


## Stage

[group('stage')]
[doc('Stage program as .wasm')]
stage program: (wat program)


[group('stage')]
[doc('Compile target .wat file to .wasm')]
wat program:
  wat2wasm --no-canonicalize-leb128s --disable-bulk-memory --debug-names -v -o upload.wasm {{program}}


## Clean

[group('clean')]
[doc('Clean build folders')]
clean:
    rm -rf build-emu build-doctest build

[group('clean')]
[doc('Reset repo (warn: aggresive)')]
[confirm("Hard reset repository?")]
reset: clean prune


## Build

has(flags, flag) := if flags =~ ('(^| )' + flag + '($| )') { "true" } else { "false" }

extract(flags) := trim(
  (if has(flags, "debug") == "true" { "-DDEBUG" } else { "" }) + " " +
  (if has(flags, "trace") == "true" { "-DTRACE" } else { "" })
)

cmake(flags) := (
    if extract(flags) == "" {
      ""
    } else {
      '-DCMAKE_CXX_FLAGS="' + extract(flags) + '"'
    }
)

[group('build')]
[doc('Build runtime for any platform (flags: debug, trace)')]
build platform *flags:
    just {{platform}} {{flags}}

[group('build')]
[working-directory: 'build-emu']
[doc('Platform: emulator')]
emulator *flags: _mkdir-emu
    cmake .. -D BUILD_EMULATOR=ON {{cmake(flags)}} -G Ninja
    ninja

[group('build')]
[doc('Platform: zephyr')]
zephyr *flags='-b esp32_devkitc_wroom/esp32/procpu': 
    just _zephyr "west build {{flags}}"


## Flash

[group('exec')]
[doc('Flash/execute platform')]
flash platform program="upload.wasm" *flags:
    just _flash_{{platform}} {{program}} {{flags}}

_flash_zephyr program *flags:
    just _zephyr "west flash {{flags}}"

_flash_emulator program *flags='--socket 8119': (cli program flags)

[group('exec')]
[doc('Run command-line interface')]
cli program *flags:
    ./build-emu/wdcli {{program}} {{flags}}


## Monitor

monitor platform port='8119':
    just _monitor_{{platform}} {{port}}

[no-exit-message]
_monitor_emulator port='8119':
    telnet localhost {{port}}


_monitor_zephyr port:
    west espressif monitor


## Setup

[group('setup')]
[doc('Setup toolchains & platforms')]
setup platform: _setup-emulator

[confirm('You are about to install nanopb using Homebrew. Proceed? [y/n]')]
_nanopb:
    brew install nanopb

[group('setup')]
[doc('Setup: emulator')]
_setup-emulator: _nanopb
    git submodule update --init --recursive

[group('lint')]
[doc('Lint src folder')]
[no-exit-message]
lint:
    clang-format -i src/**/*

## Tests

[group('test')]
[doc('Run tests at level (unit, integration, spec, all)')]
test level='all':
    just {{level}}

[group('test')]
unit: _mkdir-doctest _build-doctest
    ctest --test-dir ./build-doctest --output-on-failure

[group('test')]
[working-directory: 'tests/latch/']
spec: 
    WABT="../../lib/wabt/build/" npm run tests:spec

[group('test')]
[working-directory: 'tests/latch/']
integration: 
    WABT="../../lib/wabt/build/" npm run tests:integration

[group('test')]
[working-directory: 'tests/latch/']
all: 
    WABT="../../lib/wabt/build/" npm run tests:all

## QoL / Maintenance

[group('maintenance')]
maintain:
    git maintenance run

[group('maintenance')]
[doc('Aggressive GC for git (use wisely)')]
[confirm("GC git and delete stale files?")]
prune:
    git maintenance run --task=gc
    git remote prune origin


## Private recipes

# make build folder
_mkdir-emu:
    mkdir -p build-emu

# make build folder
_mkdir-doctest:
    mkdir -p build-doctest

_build-doctest:
    cmake -B ./build-doctest -D BUILD_UNITTEST=ON -G Ninja
    cmake --build ./build-doctest 2>/dev/null

# activate zephyr environment and run a command in it
[working-directory: 'platforms/Zephyr']
[script("bash")]
_zephyr +cmd:
    set -euo pipefail
    source ~/zephyrproject/.venv/bin/activate
    source ~/zephyrproject/zephyr/zephyr-env.sh
    {{cmd}}
