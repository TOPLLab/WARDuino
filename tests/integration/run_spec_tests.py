import argparse
import os
import shutil
import subprocess


def main(test_directory):
    if test_directory[-1] != "/":
        test_directory += "/"

    # Create temporary directory
    tmp_directory = test_directory + ".tmp/"
    if not os.path.exists(tmp_directory):
        os.mkdir(tmp_directory)

    passed_tests = []

    # For each test file in the test directory
    for filename in os.listdir(test_directory):
        if filename.endswith(".wast"):
            print(f"""\n====== TESTING {filename} ======""")

            base_name = tmp_directory + os.path.splitext(filename)[0]
            modules_file = open(base_name + "_modules.wast", "w")
            asserts_file = open(base_name + "_asserts.wast", "w")

            file = modules_file
            module = False
            failed = False
            for line in open(test_directory + filename, "r"):
                if line.startswith("(module"):
                    if module:
                        print(f"""Error {filename} requires support for multiple modules""")
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
                    completion = subprocess.run(
                        [args.interpreter, modules_file.name, asserts_file.name, args.compiler])
                    if completion.returncode == 0:
                        passed_tests.append((filename, completion.returncode))
                except subprocess.CalledProcessError:
                    pass

    if args.verbose:
        print("\n\n=============\nPassed tests:")
        for passed_test in passed_tests:
            print(f"""\t{passed_test}""")

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
