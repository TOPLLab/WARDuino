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


## Stage

[group('stage')]
[doc('Stage program as .wasm')]
stage program: (wat program)


[group('stage')]
[doc('Compile target .wat file to .wasm')]
wat program:
  wat2wasm --no-canonicalize-leb128s --disable-bulk-memory --debug-names -v -o upload.wasm {{program}}


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
    cmake .. -D BUILD_EMULATOR=ON {{cmake(flags)}}
    make

[group('build')]
[doc('Platform: zephyr')]
zephyr *flags='-b esp32_devkitc_wroom/esp32/procpu': 
    just _zephyr "west build {{flags}}"


## Flash

[group('exec')]
[doc('Flash/execute platform')]
flash platform program *flags:
    just _flash_{{platform}} {{program}} {{flags}}

_flash_zephyr program *flags:
    just _zephyr "west flash {{flags}}"

_flash_emulator program *flags: (cli program flags)

[group('exec')]
[doc('Run command-line interface')]
cli program *flags:
    ./build-emu/wdcli {{program}} {{flags}}


## Setup

[group('setup')]
[doc('Setup toolchains & platforms')]
setup platform: _setup-emulator

[group('setup')]
[doc('Setup: emulator')]
_setup-emulator:
    git submodule update --init --recursive

[group('lint')]
[doc('Lint src folder')]
lint:
    clang-format -i src/**/*

## Tests

[group('test')]
[doc('Run unit tests')]
test: _mkdir-doctest _build-doctest
    ctest --test-dir ./build-doctest --output-on-failure


## Private recipes

# make build folder
_mkdir-emu:
    mkdir -p build-emu

# make build folder
_mkdir-doctest:
    mkdir -p build-doctest

_build-doctest:
    cmake -B ./build-doctest -D BUILD_UNITTEST=ON
    cmake --build ./build-doctest 2>/dev/null

# activate zephyr environment and run a command in it
[working-directory: 'platforms/Zephyr']
[script("bash")]
_zephyr +cmd:
    set -euo pipefail
    source ~/zephyrproject/.venv/bin/activate
    source ~/zephyrproject/zephyr/zephyr-env.sh
    {{cmd}}
