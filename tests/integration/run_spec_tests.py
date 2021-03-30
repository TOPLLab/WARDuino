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
                    subprocess.run([args.exec, modules_file.name, asserts_file.name, "wat2wasm"])
                except subprocess.CalledProcessError:
                    pass

    # Remove temporary files
    try:
        shutil.rmtree(tmp_directory)
    except OSError as e:
        print("Error: %s - %s." % (e.filename, e.strerror))


if __name__ == '__main__':
    # Args handling
    parser = argparse.ArgumentParser()
    parser.add_argument("--exec", default="../../cmake-build-debug/TestWARDuino")

    args = parser.parse_args()

    main("core/")
