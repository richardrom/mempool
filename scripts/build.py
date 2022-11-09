# Ricardo Romero
# Compile and run tests scripts

import sys, getopt, os, subprocess

vcpkg_toolchain = ''
cl_c = ''
cl_cpp = ''


def main(argv):
    global vcpkg_toolchain, cl_cpp, cl_c
    try:
        opts, args = getopt.getopt(argv, "hv:c:", ["vcpkg=", "cl="])
    except getopt.GetoptError:
        print('build.py -v <vcpkg install path> -c <clang compiler binaries path>')
        sys.exit(1)
    for opt, arg in opts:
        if opt == '-h':
            print('build.py -v <vcpkg toolchain cmake file> -c <clang compiler binaries path>')
            sys.exit()
        elif opt in ("-v", "--vcpkg"):
            vcpkg_toolchain = arg + '/vcpkg/scripts/buildsystems/vcpkg.cmake'
        elif opt in ("-c", "--cl"):
            cl_c = arg + '/clang'
            cl_cpp = arg + '/clang++'

    is_vcpkg = os.path.isfile(vcpkg_toolchain)

    if not is_vcpkg:
        print(f'{vcpkg_toolchain} could not be found')
        exit(1)

    is_c = os.path.isfile(cl_c)
    is_cpp = os.path.isfile(cl_cpp)

    if not is_c:
        print(f'{cl_c} could not be found')
        exit(1)
    if not is_cpp:
        print(f'{cl_cpp} could not be found')
        exit(1)


def cmake_configure():
    targets = {
        "Release": "../build/release",
        "Debug": "../build/debug"
    }

    for t in targets:
        params = ["cmake", f'-DCMAKE_BUILD_TYPE={t}', F"-DCMAKE_MAKE_PROGRAM=ninja",
                  f"-DCMAKE_C_COMPILER={cl_c}", f"-DCMAKE_CXX_COMPILER={cl_cpp}",
                  "-G", "Ninja", f"-DCMAKE_TOOLCHAIN_FILE={vcpkg_toolchain}",
                  "-S", "..", "-B", f"{targets[t]}"]
        cmake_bin = subprocess.run(params)
        if cmake_bin.returncode != 0:
            print(f'CMake returned {cmake_bin.returncode} and can not proceed')
            exit(1)

        compile_params = ["cmake", "--build", f"{targets[t]}", "--target", "mempool", "memtests", "--", "-j", "8"]
        cmake_bin = subprocess.run(compile_params)
        if cmake_bin.returncode != 0:
            print(f'CMake returned {cmake_bin.returncode} and can not proceed')
            exit(1)

        print(f"Running {t} tests...")
        path = targets[t] + "/test/memtests"
        run_tests = [path]
        tests_bin = subprocess.run(run_tests)
        if tests_bin.returncode != 0:
            print(f'{targets[t]} tests failed')


if __name__ == "__main__":
    main(sys.argv[1:])
    cmake_configure()
