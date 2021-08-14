*Warning: this information is partially outdated.*

## AutoFDO Procedures

Feedback Directed Optimization (FDO) involves recording performance data from
the Linux kernel and using it to direct the compiler's optimizer.

### Prerequisites for AutoFDO

- An **Intel processor** that supports the last branch record (LBR) instruction.
- A Linux **kernel** built with Branch Profiling tracers enabled:

  ``` shell
  CONFIG_PM_TRACE=y
  CONFIG_TRACE_BRANCH_PROFILING=y
  CONFIG_BRANCH_TRACER=y
  ```

  These can be enable directly in your kernel's `.config` file or using `make
  menuconfig` via the following menu options:
  1. `Kernel hacking  --->`
  1. `[*] Tracers  --->`
  1. `Branch Profiling (Trace likely/unlikely profiler)`
  1. `(X) Trace likely/unlikely profiler`

- The **AutoFDO** software package. It may be available via your package
  manager or built from sources <https://github.com/google/autofdo>.

  - **Note about compiler versions** the autofdo binaries need to be compiled
    with the exact version of the compiler that will later be used to compile
    our final optimized version of dosbox-staging.

    So for example, if you install autofdo via package-manager, then it will be
    valid for the default version of gcc and clang also installed by your
    package manager.  Where as if you plan to build with  `gcc-<latest>`, then
    you will need to compile autofdo from sources using `gcc-<latest>` by
    pointing the `CC` and `CXX` environment variables to the newer gcc
    binaries.

  - **Note about clang** If you plan to compile with a version of clang newer
    than your package manager's default version, then you will need to compile
    autofdo from source and configure it with the corresponding version of
    `llvm-config`.  For example, if I want to build with clang-10, then I would
    configure autofdo with: `./configure --with-llvm=/usr/bin/llvm-config-10`.

  - The included `.github/scripts/build-autofdo.sh` script can be used to build
    and install autofdo, for example:

    - default GCC:

      `sudo .github/scripts/build-autofdo.sh`
    - newer GCC:

      ``` shell
      export CC=/usr/bin/gcc-9
      export CXX=/usr/bin/g++-9
      sudo .github/scripts/build-autofdo.sh
      ```

    - Clang version 10:

      `sudo .github/scripts/build-autofdo.sh`

- The **pmu-tools** software package, which can be downloaded from
  <https://github.com/andikleen/pmu-tools.> This is a collection of python
  scripts used to assist in capturing sampling data.

### Record Data for AutoFDO Builds

1. Ensure the custom Linux Kernel supporting LBR tracing is running.

2. Build `dosbox-staging` from source using the `fdotrain` target:
   `./scripts/build.h -c gcc -t fdotrain`

3. Record kernel sample profiles while running dosbox-staging:

    `/path/to/pmu-tools/ocperf.py record -F max -o "samples-1.prof" -b -e
    br_inst_retired.near_taken:pp -- /path/to/fdo-trained/dosbox ARGS`

   Where `samples-1.prof` is the file that will be filled with samples.

   Repeat this for multiple training runs, each time saving the output to a new
   `-o samples-N.prof` file.  Ideally you want to exercise all code paths in
   dosbox-staging (core types, video cards, video modes, sound cards, and audio
   codecs).

4. Convert your sample profiles into compiler-specific records using tools
   provided in the `autofdo` package:

   For GCC, run:
   - `create_gcov --binary=/path/to/fdo-trained/dosbox
     --profile=samples-1.prof -gcov=samples-1.afdo -gcov_version=1`

     ... for each `.prof` file, creating a corresponding `.afdo` file.

   - At this point, you now have an `.afdo` file for each `.prof` file. Merge
     the `.afdo`s into a single `curren.afdo` file with:

     `profile_merger -gcov_version=1 -output_file=current.afdo *.afdo`

   For Clang, run:

   - `create_llvm_prof --binary=/path/to/fdo-trained/dosbox
     --profile=samples-1.prof --out=samples-1.profraw`

     ... for each `*.prof` file, creating a corresponding `.profraw` file.

   - At this point, you now have a `.profraw` file for each `.prof` file. Merge
     them into a single `current.profraw` file with:

     `llvm-profdata-<version> merge -sample -output=current.profraw *.profraw`

### Build Using AutoFDO Data

You can now use your merged `.afdo` or `.profraw` file to build with the `-m
fdo` modifier by  placing your `current.afdo/.profraw` file in the repo's root
directory, or point to it using the FDO_FILE environment variable, and launch
the build with `./scripts/build.sh -c <gcc or clang> -t release -m lto -m fdo`.
