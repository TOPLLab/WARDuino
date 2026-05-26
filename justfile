set unstable


default:
    just --list


## Run
[group('run')]
[doc('Run program on any platform (rebuilds)')]
run platform program:
    stage {{program}}
    build {{platform}}
    flash {{platform}}


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
build platform *flags: (emulator flags)

[group('build')]
[working-directory: 'build-emu']
[doc('Platform: emulator')]
emulator *flags: _mkdir
    cmake .. -D BUILD_EMULATOR=ON {{cmake(flags)}}
    make

[group('build')]
[working-directory: 'platforms/zephyr']
[doc('Platform: zephir')]
zephir *flags: _zephir
    ls


## Flash
[group('exec')]
[doc('Flash/execute platform')]
flash platform: cli


[group('exec')]
[doc('Run command-line interface')]
cli:
    ./build-emu/wdcli 


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


## Private recipes

# make build folder
_mkdir:
    mkdir -p build-emu

# activate zephyr environment
_zephir:
    source ~/zephyrproject/.venv/bin/activate
    source ~/zephyrproject/zephyr/zephyr-env.sh

