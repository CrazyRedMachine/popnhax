import os
import subprocess

ignore_folders = [".git", "build", "dist", "minhook"]
cpp_extensions = (".cpp", ".cxx", ".c++", ".h++", ".hpp", ".hxx")

for root, dirs, files in os.walk(".", topdown=True):
    dirs[:] = [d for d in dirs if d not in ignore_folders]

    for filename in files:
        if filename.endswith(cpp_extensions):
            print(filename)
            subprocess.check_call(["clang-format", "-i", "-style=file", root + "/" + filename])
            # print(" ".join(["clang-tidy", root + "/" + filename, "--", "-I."]))
            subprocess.check_call(["./clang-tidy.sh", root + "/" + filename, "c++11"])
            print()


cpp_extensions = (".cc", ".cp", ".c", ".i", ".ii", ".h")

for root, dirs, files in os.walk(".", topdown=True):
    dirs[:] = [d for d in dirs if d not in ignore_folders]

    for filename in files:
        if filename.endswith(cpp_extensions):
            print(filename)
            subprocess.check_call(["clang-format", "-i", "-style=file", root + "/" + filename])
            # print(" ".join(["clang-tidy", root + "/" + filename, "--", "-I."]))
            subprocess.check_call(["./clang-tidy.sh", root + "/" + filename, "c11"])
            print()
