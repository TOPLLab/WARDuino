#!/usr/bin/env python3

# run_spec_tests.py
#
# Runs all WebAssembly Core Spec tests for WARDuino.
#
# By default the script stops when it encounters a single error.
# At verbosity level 0: the script will run all spectest files regardless of failing tests.
#
# Requirements:
#   command to translate WebAssembly text format to the WebAssembly binary format (default: wat2wasm)
#   TESTWARDuino executable (build cmake project in root)
#
# Author: Tom Lauwaerts

import argparse
import os
import shutil
import subprocess

from glob import glob
from git import Repo, exc

SUPPORTED_ASSERTS = ["assert_return", "assert_exhaustion", "assert_trap"]


def update_repo():
    try:
        suite = Repo(args.testsuite)
        assert not suite.bare
        suite.git.clean('-f')
        suite.git.restore('.')
        suite.git.pull()
    except exc.NoSuchPathError:
        Repo.clone_from(args.repo, args.testsuite)


def sanitise_repo():
    shutil.rmtree(os.path.join(args.testsuite, "proposals"))
    shutil.rmtree(os.path.join(args.testsuite, "repos"))

    if args.ignore is not None:
        with open(args.ignore, "r") as ignore:
            for line in ignore:
                pattern = args.testsuite + line.strip()
                files = glob(pattern)
                for file in files:
                    os.remove(file)

    tests = [os.path.join(args.testsuite, filename) for filename in os.listdir(args.testsuite) if
             filename.endswith(".wast")]
    for filename in tests:
        sanitise_testfile(filename)


def sanitise_testfile(filename):
    base_path = os.path.splitext(filename)[0]
    with open(filename, "r") as template:
        module_file_name = base_path + ".module.wast"
        asserts_file_name = base_path + ".asserts.wast"
        module_file = open(module_file_name, "w")
        asserts_file = open(asserts_file_name, "w")

        # Move asserts to separate file
        file = module_file
        module_seen = False
        for line in template:
            if line.startswith("(module"):
                if module_seen:
                    break
                module_seen = True
            elif line.startswith("(assert") or line.startswith("(invoke"):
                file = False
                if any([line.startswith(f"({_assert}") for _assert in SUPPORTED_ASSERTS]):
                    file = asserts_file

            if file and not line.startswith(";;"):
                file.write(line)
        module_file.close()
        asserts_file.close()
        shutil.move(module_file_name, filename)

        # Remove unnecessary files (no asserts)
        if os.path.getsize(asserts_file_name) == 0:
            os.remove(filename)
            os.remove(asserts_file_name)


def main():
    tests = [os.path.join(args.testsuite, filename) for filename in sorted(os.listdir(args.testsuite)) if
             filename.endswith(".asserts.wast")]

    if args.file:
        tests = [args.file]

    for filename in tests:
        base = "core/" + "".join(os.path.basename(filename).split(".")[:-2])
        print(base)
        status = subprocess.run(
            [args.interpreter, base + ".wast", "--asserts", base + ".asserts.wast", "--watcompiler", args.compiler],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if status.returncode == 0:
            print(f"{filename}: All tests passed.\n")
        else:
            if args.verbosity == 0:
                print(f"""{filename}: exited with {status.returncode}""")
            else:
                print(f"""{filename}:\n {status.stdout.decode("utf-8")}\n""")
                exit(status.returncode)


if __name__ == '__main__':
    # Handle script arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default="https://github.com/WebAssembly/testsuite",
                        help="Testsuite Git repo (default: WebAssembly/testsuite)")
    parser.add_argument("--testsuite", default="core/", help="Testsuite directory (default: core/)")
    parser.add_argument("--interpreter", default="../../cmake-build-debug/warduino",
                        help="Interpreter (TestWARDuino executable")
    parser.add_argument("--compiler", default="wat2wasm", help="WebAssembly text format compiler (default: wat2wasm)")
    parser.add_argument("--verbosity", type=int, default=1, help="Verbosity level: 0-3 (default: 1)")
    parser.add_argument("--ignore", type=str, default=None, help="Ignore file lists tests to ignore")
    parser.add_argument("--file", type=str, default=None, help="Assert file to run (.asserts.wast extension)")

    args = parser.parse_args()

    # Get latest version of tests
    update_repo()
    sanitise_repo()

    # Run tests
    main()
