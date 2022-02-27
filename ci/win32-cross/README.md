
# Cross-Compiling Win32 DOSBox from Ubuntu 20.04

## Ubuntu Machine One-Time Setup

```
sudo apt-get install mingw-w64 wget build-essential autoconf automake-1.15 autotools-dev m4 zip
./install_dosbox_sdl_deps.sh
```

## Compiling DOSBox

```
./cross_compile_dosbox.sh
```

## Creating Release Package

```
./make_dosbox_zip_package.sh
```



