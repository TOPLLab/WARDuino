#!/usr/bin/env python3

# run_spec_tests.py
#
# Runs all WebAssembly Core Spec tests for WARDuino.
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

SUPPORTED_ASSERTS = ["assert_return", "assert_exhaustion", "assert_trap"]


class TestsuiteStatistics:
    def __init__(self):
        self.total = 0
        # TODO arrays instead of numbers
        self.skipped = 0
        self.failed = 0
        self.crashed = 0
        self.success = 0
        self.results = []

    def testsuite_passed(self):
        return self.success == self.total

    def add_result(self, result):
        self.results.append(result)
        if result.skipped():
            self.skipped += 1
        elif result.passed():
            self.success += 1
        elif result.compiler_crashed():
            self.crashed += 1
        else:
            self.failed += 1

    def __str__(self):
        individual_passed_tests = sum([result.passed_tests for result in self.results])
        individual_tests = sum([result.total_tests for result in self.results])
        individual_percentage = (individual_passed_tests / individual_tests) * 100
        individual_mark = "\u2714" if individual_percentage == 100 else "\u274C"

        overview = "\nTESTSUITE RESULTS:\n" \
                   + f"total: {self.total}, skipped: {self.skipped}, failed: {self.failed}, " \
                   + f"crashed: {self.crashed}, success: {self.success}\n\n"
        if args.verbosity > 0:
            overview += f"{individual_mark} {individual_passed_tests}/{individual_tests}" \
                        + f" (~{individual_percentage:.2f}%) asserts succeeded.\n"
        return overview


class TestResults:

    def __init__(self, name):
        self.name = name
        self.stdout = b''
        self.stderr = b''
        self.return_code = 0
        self.passed_tests = 0
        self.failed_tests = 0
        self.total_tests = 0
        self.crashed_tests = 0
        self.assert_frequencies = [0, 0, 0]
        self.assert_passed = [0, 0, 0]

    def add_completion(self, completion):
        self.stdout += completion.stdout
        self.stderr += completion.stderr
        if self.return_code < completion.returncode:
            self.return_code = completion.returncode
        self._count_results(completion)

    def _count_results(self, completion):
        self.passed_tests += completion.stdout.count(b'OK')
        self.failed_tests += completion.stdout.count(b'FAIL')
        self.total_tests += completion.stdout.count(b'assert')
        self.crashed_tests += self.total_tests - (self.passed_tests + self.failed_tests)

        for part in completion.stdout.split(b'. '):
            if b'assert' in part and b'OK' in part:
                self.assert_passed[
                    [l for l, a in enumerate(SUPPORTED_ASSERTS) if a.split("_")[1] in str(part)][0]] += 1

    def passed(self):
        return not self.compiler_crashed() and not self.failed() and (
            self.return_code == 0 and self.passed_tests == self.total_tests)

    def skipped(self):
        return not self.compiler_crashed() and self.total_tests == 0

    def failed(self):
        return self.return_code == 2

    def compiler_crashed(self):
        return self.return_code != 0 and b'compile' in self.stderr

    def __str__(self):
        string_representation = ""
        if args.verbosity < 0:
            # negative verbosity gives latex table
            if self.skipped():
                return ""
            start = "            \cat{" + self.name + "}"
            space = " " * (36 - len(start))
            string_representation += f"            \\hline\n{start}{space} "
            for i, freq in enumerate(self.assert_frequencies):
                percent = "& \smath{"
                percent += f"{(self.assert_passed[i]/freq) * 100:.0f}" if freq > 0 else "-"
                percent += " \%}"
                space = " " * (26 - len(percent))
                string_representation += f"{percent}{space} "
            total_percent = 0 if self.total_tests == 0 else (self.passed_tests / self.total_tests) * 100
            total = "& \smath{" + f"{total_percent:.0f} \\%" + "} "
            string_representation += total + (" " * (28 - len(total)))
            space = " " * 37
            string_representation += f"\\\\\n{space}"
            for i, freq in enumerate(self.assert_frequencies):
                div = "& \smallsmath{"
                div += f"{self.assert_passed[i]}/{freq}" if freq > 0 else "-"
                div += "}"
                space = " " * (26 - len(div))
                string_representation += f"{div}{space} "
            total = "& \smallsmath{" + f"{self.passed_tests}/{self.total_tests}" + "} "
            string_representation += total + (" " * (28 - len(total))) + "\\\\"
            return string_representation

        mark = "\u2714" if self.passed() else "\u274C"
        string_representation = f"{mark}\t"
        if args.verbosity > 0:
            if self.skipped():
                string_representation += f"skipped" + (" (0 tests)" if args.verbosity > 2 else "")
            elif self.failed():
                string_representation += f"{self.passed_tests}/{self.total_tests} passed" + (
                    f" (exit {self.return_code})" if args.verbosity > 1 else "")
            elif self.compiler_crashed():
                string_representation += "compiler crashed" + (
                    f" (exit {self.return_code})" if args.verbosity > 1 else "")
            else:
                string_representation += f"{self.passed_tests}/{self.total_tests} passed" + (
                    f" (exit {self.return_code})" if args.verbosity > 1 else "")

        string_representation = string_representation.rstrip()
        if args.verbosity > 2:
            string_representation += "\n> stdout:\n\t".expandtabs(2)
            string_representation += self.stdout.decode("utf-8").rstrip().replace("\n", "\n\t").expandtabs(2)
            string_representation += "\n> stderr:\n\t".expandtabs(2)
            string_representation += self.stderr.decode("utf-8")
            string_representation = string_representation.rstrip() + "\n"
        return string_representation


def main():
    test_directory = args.testsuite
    if test_directory[-1] != "/":
        test_directory += "/"
    # Create temporary directory
    tmp_directory = test_directory + ".tmp/"
    if not os.path.exists(tmp_directory):
        os.mkdir(tmp_directory)

    tests = [filename for filename in os.listdir(test_directory) if filename.endswith(".wast")]

    stats = TestsuiteStatistics()
    # For each test file in the test directory
    print(f"RUNNING TESTSUITE: {args.testsuite.strip('/')}\n")
    for filename in tests:
        base_path = tmp_directory + os.path.splitext(filename)[0]
        asserts_path = base_path + "_asserts.wast"
        modules_file = open(base_path + "_modules.wast", "w")
        asserts_file = open(asserts_path, "w")

        base_name = base_path.split("/")[-1]

        if args.verbosity >= 0:
            tabs = '\t' * (7 - (len(base_name) // 4))
            print(f"""{base_name}{tabs}""".expandtabs(4), end="")
        stats.total += 1

        test_results = TestResults(base_name)
        file = modules_file
        has_module = False
        total_asserts = 0
        for line in open(test_directory + filename, "r"):
            if line.startswith("(module"):
                if has_module:
                    modules_file.close()
                    asserts_file.close()

                    if os.path.getsize(asserts_path):
                        try:
                            completion = subprocess.run(
                                [args.interpreter, modules_file.name, asserts_file.name, args.compiler],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                            test_results.add_completion(completion)
                        except subprocess.CalledProcessError:
                            pass
                    modules_file = open(base_path + "_modules.wast", "w")
                    asserts_file = open(base_path + "_asserts.wast", "w")
                has_module = True
                file = modules_file
            elif line.startswith("(assert"):
                file = False
                if any([line.startswith(f"({_assert}") for _assert in SUPPORTED_ASSERTS]):
                    total_asserts += 1
                    file = asserts_file
                    test_results.assert_frequencies[
                        [l for l, a in enumerate(SUPPORTED_ASSERTS) if line.startswith(f"({a}")][0]] += 1

            if file and not line.startswith(";;"):
                file.write(line)

        modules_file.close()
        asserts_file.close()

        if os.path.getsize(asserts_path):
            try:
                completion = subprocess.run([args.interpreter, modules_file.name, asserts_file.name, args.compiler],
                                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                test_results.add_completion(completion)
                test_results.total_tests = total_asserts
            except subprocess.CalledProcessError:
                pass
        stats.add_result(test_results)
        print(test_results)

    print(stats)
    if args.verbosity > 2:
        print("Passed testfiles:")
        for test_result in stats.results:
            if test_result.passed() and test_result.total_tests > 0:
                tabs = '\t' * (14 - (len(test_result.name) // 2))
                print(f"> {test_result.name}{tabs}# tests run: {str(test_result.total_tests)}".expandtabs(2))

    # Remove temporary files
    try:
        shutil.rmtree(tmp_directory)
    except OSError as e:
        print("Error: %s - %s." % (e.filename, e.strerror))

    exit(int(not stats.testsuite_passed()))


if __name__ == '__main__':
    # Handle script arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("--testsuite", default="core/", help="Testsuite directory (default: core/)")
    parser.add_argument("--interpreter", default="../../cmake-build-debug/TestWARDuino",
                        help="Interpreter (TestWARDuino executable")
    parser.add_argument("--compiler", default="wat2wasm", help="WebAssembly text format compiler (default: wat2wasm)")
    parser.add_argument("--verbosity", type=int, default=1, help="Verbosity level: 0-3 (default: 1)")

    args = parser.parse_args()

    main()
