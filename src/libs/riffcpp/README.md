# riffcpp

[![Appveyor status](https://ci.appveyor.com/api/projects/status/github/frabert/riffcpp)](https://ci.appveyor.com/project/frabert/riffcpp)

Simple library for reading RIFF files

The data is read lazily, so in cases where seeking is cheaper than reading and
large chunks of data can be skipped, this results in fast parsing, and also saves
memory usage.

## Usage

The following example program prints the structure of the RIFF file, and provides
an hex dump of unknown chunks:

```c++
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <riffcpp.hpp>

void print_hex_dump(std::vector<char> &data, int indent) {
  int i = 0;
  for (char c : data) {
    if (i % 16 == 0) {
      for (int j = 0; j < indent; j++) {
        std::cout << "  ";
      }
    }
    std::cout << std::setfill('0') << std::setw(2) << std::hex
              << (int)((unsigned char)c) << ' ';
    if (i % 16 == 15) {
      std::cout << "  ";
      for (int j = 0; j < 16; j++) {
        char x = data[i - 15 + j];
        if (x >= 0x20 && x <= 0x7E) {
          std::cout << x;
        } else {
          std::cout << '.';
        }
      }
      std::cout << std::endl;
    }
    i++;
  }
  if (i % 16 != 0) {
    for (int k = i % 16; k < 16; k++) {
      std::cout << "-- ";
    }
    std::cout << "  ";
    for (int j = 0; j < i % 16; j++) {
      char x = data[i - (i % 16) + j];
      if (x >= 0x20 && x <= 0x7E) {
        std::cout << x;
      } else {
        std::cout << '.';
      }
    }
  }
  std::cout << std::dec << std::endl;
}

void print_chunks(riffcpp::Chunk &ch, int offs = 0) {
  auto id = ch.id();
  auto size = ch.size();
  std::vector<char> buffer;
  buffer.resize(size);
  ch.read_data(buffer.data(), buffer.size());
  if (id == riffcpp::riff_id || id == riffcpp::list_id) {
    for (int i = 0; i < offs; i++) {
      std::cout << "  ";
    }
    auto type = ch.type();
    std::cout << std::string(id.data(), 4) << " " << std::string(type.data(), 4)
              << " size: " << size << std::endl;
    for (auto ck : ch) {
      print_chunks(ck, offs + 1);
    }
  } else {
    for (int i = 0; i < offs; i++) {
      std::cout << "  ";
    }
    std::cout << std::string(id.data(), 4) << " size: " << size << std::endl;
    print_hex_dump(buffer, offs + 1);
  }
}

int main(int argc, char *argv[]) {
  riffcpp::Chunk ch(argv[1]);
  try {
    print_chunks(ch);
  } catch (riffcpp::Error &e) {
    std::cerr << "\n Error: " << e.what() << std::endl;
  }
  return 0;
}
```

## Installing

### On Debian and Ubuntu

Add the libdmusic PPA to your repository:
```sh
sudo add-apt-repository ppa:libdmusic/unstable
```

Then, run

```sh
sudo apt update
sudo apt install libriffcpp # you'll also need libriffcpp-dev if you need to compile programs that use riffcpp
```

### On Windows and other platforms using `vcpkg`

[vcpkg](https://github.com/Microsoft/vcpkg) already has a portfile for `riffcpp`,
so you can just do

```sh
vcpkg install riffcpp
```

## Building from sources

After downloading the sources (either by `git clone` or archive), run the following commands:

```sh
mkdir build_riffcpp
cd build_riffcpp
cmake path/to/riffcpp
cmake --build .
cmake --build . --target INSTALL # Only if you want to install the library
```
