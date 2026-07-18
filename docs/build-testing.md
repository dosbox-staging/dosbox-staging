# Running the unit tests

Unit tests are built by default. To disable building them, pass the
`-DOPT_TESTS=OFF` option when configuring the project, for example:

```shell
cmake --preset=<preset> -DOPT_TESTS=OFF
```

Tests are run with `ctest`, using the same CMake preset you built with.

To run the entire test suite:

```shell
ctest -j 8 --preset=<preset>
```

The `-j 8` option runs the tests in parallel on 8 CPU cores. Adjust it to suit
your system.

To run all test cases in a single test suite, pass its name with the `-R`
option:

```shell
ctest -j 8 --preset=<preset> -R DOS_FilesTest
```

You can narrow this down to a single test case:

```shell
ctest -j 8 --preset=<preset> -R DOS_FilesTest.DOS_MakeName_Basic_Failures
```

To run a group of tests, use wildcards and regexes. E.g. to run all test cases
in the `DOS_FilesTest` suite with names starting with `DOS_MakeName_`:

```shell
ctest -j 8 --preset=<preset> -R "DOS_FilesTest.DOS_MakeName_*"
```

Pass the `-V` option to see the DOSBox Staging log output:

```shell
ctest -j 8 --preset=<preset> -R DOS_FilesTest.DOS_MakeName_Basic_Failures -V
```

You might want to run the test executable directly to get coloured output, and
the option to drop into an interactive debugger (e.g. `gdb`) if a test crashes.
For example:

```shell
build/<preset>/tests/dosbox_tests --gtest_filter=DOS_FilesTest.DOS_MakeName_Basic_Failures
```

> [!NOTE]
> The path above is the single-config (Ninja) layout used on Linux. With the
> multi-config Xcode and Visual Studio generators the executable is nested
> under the configuration directory, e.g. `build/<preset>/Debug/tests/`.

See the [ctest documentation](https://cmake.org/cmake/help/v3.31/manual/ctest.1.html)
for the full list of available options.
