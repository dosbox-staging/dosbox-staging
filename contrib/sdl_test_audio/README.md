# SDL Test Audio

A simple program that tests audio drivers and their devices.

## Build

```shell
meson setup build
meson compile -C build
```

## Run

### Test all the drivers

```shell
  ./build/test_audio
```

### Test a single driver (by name)

```shell
  ./build/test_audio pulseaudio
  ./build/test_audio alsa
  ./build/test_audio pipewire
```
