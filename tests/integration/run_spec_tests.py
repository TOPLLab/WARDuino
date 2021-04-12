import argparse
import os
import shutil
import subprocess


class TestStatistics:
    def __init__(self):
        self.total = 0
        self.skipped = 0
        self.failed = 0
        self.crashed = 0
        self.timeout = 0
        self.success = 0
        self.passed_files = []

    def __str__(self):
        return "\n\n=============\nTestsuite results:\n" \
               + f"""total: {self.total}, skipped: {self.skipped}, failed: {self.failed}, """ \
               + f"""crashed: {self.crashed}, timeout: {self.timeout}, success: {self.success}\n\n""" \
               + f"""{self.success}/{self.total} testfiles passed completely."""  # \
        # + f"""{}/{} individual tests passed.\n"""


def main(test_directory):
    if test_directory[-1] != "/":
        test_directory += "/"
    # Create temporary directory
    tmp_directory = test_directory + ".tmp/"
    if not os.path.exists(tmp_directory):
        os.mkdir(tmp_directory)

    tests = [filename for filename in os.listdir(test_directory) if filename.endswith(".wast")]

    stats = TestStatistics()
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
                    print("\u274C")
                    if args.verbose:
                        print(f"""Error {filename} requires support for multiple modules""", end="")
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
                                            stdout=subprocess.PIPE)
                if completion.returncode == 0:
                    print("\u2713")
                    stats.success += 1
                    stats.passed_files.append((filename, completion.returncode))
                elif completion.returncode == 2:
                    print("\u274C")
                    stats.failed += 1
                else:
                    print("\u274C")
                    stats.crashed += 1
            except subprocess.CalledProcessError:
                pass
        else:
            stats.skipped += 1

    print(stats)
    if args.verbose:
        print("Passed files:")
        for passed_test in stats.passed_files:
            print(f"""\t{passed_test[0]}""")

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
