import argparse
import os
import shutil
import subprocess


class TestsuiteStatistics:
    def __init__(self):
        self.total = 0
        self.skipped = 0
        self.failed = 0
        self.crashed = 0
        self.success = 0
        self.results = []

    def __str__(self):
        return "\n\n=============\nTestsuite results:\n" \
               + f"total: {self.total}, skipped: {self.skipped}, failed: {self.failed}, " \
               + f"crashed: {self.crashed}, success: {self.success}\n\n" \
               + f"{self.success}/{self.total} testfiles passed completely.\n" \
               + f"{sum([result.passed_tests for result in self.results])}" \
               + f"/{sum([result.total_tests for result in self.results])} individual tests passed.\n"


class TestResults:

    def __init__(self, name, completion):
        self.name = name
        self.stderr = completion.stderr
        self.return_code = completion.returncode
        self.passed_tests = completion.stdout.count(b'OK')
        self.failed_tests = completion.stdout.count(b'FAIL')
        self.total_tests = completion.stdout.count(b'assert')
        self.crashed_tests = self.total_tests - (self.passed_tests + self.failed_tests)

    def __str__(self):
        mark = "\u274C" if self.return_code != 0 else "\u2713"
        string_representation = f"(exit {self.return_code}) {mark}"
        if self.crashed_tests != 0:
            string_representation += f" ({self.crashed_tests} tests crashed)"
        elif self.total_tests > 0:
            string_representation += f" ({self.passed_tests}/{self.total_tests})"
        if args.verbose and len(self.stderr) > 0:
            string_representation += "\n"
            string_representation += self.stderr.decode("utf-8")
        return string_representation.rstrip()


def main(test_directory):
    if test_directory[-1] != "/":
        test_directory += "/"
    # Create temporary directory
    tmp_directory = test_directory + ".tmp/"
    if not os.path.exists(tmp_directory):
        os.mkdir(tmp_directory)

    tests = [filename for filename in os.listdir(test_directory) if filename.endswith(".wast")]

    stats = TestsuiteStatistics()
    # For each test file in the test directory
    print(f"""RUNNING TESTSUITE:\n==================""")
    for filename in tests:
        print(f"""\t{filename}\t""", end="")
        stats.total += 1

        base_name = tmp_directory + os.path.splitext(filename)[0]
        modules_file = open(base_name + "_modules.wast", "w")
        asserts_file = open(base_name + "_asserts.wast", "w")

        file = modules_file
        module = False
        failed = False
        for line in open(test_directory + filename, "r"):
            if line.startswith("(module"):
                if module:
                    print(f"(has multiple modules) \u274C""", end="")  # WARDuino doesn't support this yet. Is skipped.
                    failed = True
                    break

                file = modules_file
                module = True
            elif line.startswith("(assert"):
                file = asserts_file

            if not line.startswith(";;"):
                file.write(line)

        modules_file.close()
        asserts_file.close()

        if not failed:
            try:
                completion = subprocess.run([args.interpreter, modules_file.name, asserts_file.name, args.compiler],
                                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                test_results = TestResults(filename, completion)
                stats.results.append(test_results)
                if completion.returncode == 0:
                    stats.success += 1
                elif completion.returncode == 2:
                    stats.failed += 1
                else:
                    stats.crashed += 1
                print(test_results, end="")
            except subprocess.CalledProcessError:
                pass
        else:
            stats.skipped += 1

        print("")

    print(stats)
    if args.verbose:
        print("Passed files:")
        for passed_test in stats.passed_files:
            print(f"""\t{passed_test.name}""")

    # Remove temporary files
    try:
        shutil.rmtree(tmp_directory)
    except OSError as e:
        print("Error: %s - %s." % (e.filename, e.strerror))


if __name__ == '__main__':
    # Args handling
    parser = argparse.ArgumentParser()
    parser.add_argument("--interpreter", default="../../cmake-build-debug/TestWARDuino", help="Interpreter")
    parser.add_argument("--compiler", default="wat2wasm", help="WebAssembly text format compiler (default: wat2wasm)")
    parser.add_argument("--verbose", default=False, help="Verbose output (default: False)")

    args = parser.parse_args()

    main("core/")
