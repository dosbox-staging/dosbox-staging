# Unit tests / Selftests

To build unit tests you need to install additional dependency (Google Test
framework):

``` shell
# Fedora
sudo dnf install gtest-devel
```

``` shell
# Debian, Ubuntu
sudo apt install libgtest-dev
```

``` shell
# Arch, Manjaro
sudo pacman -S gtest
```

Use `--enable-tests` config flag:

``` shell
./autogen.sh
./configure --enable-tests
make -j$(nproc)
```

Run tests:

```
./tests/tests
```

Additional options (selectve test run, shuffling tests order, etc) are
documented in the test binary itself: `./tests/tests --help`.

# Writing tests

See `tests/example.cpp` for brief tutorial (with examples) on writing tests
using Google Test framework.
