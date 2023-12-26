# Analog output filter emulation

## Overview


## Custom filter settings

One or two custom filters in the following format:

```
TYPE ORDER FREQ
```

Where **TYPE** can be `hpf` (high-pass) or `lpf` (low-pass), **ORDER** is the
order of the filter from 1 to 16 (1st order = 6dB/octave slope, 2nd order =
12dB/octave, etc.), and **FREQ** is the cutoff frequency in Hz.

Examples:

- 2nd order (12dB/octave) low-pass filter at 12kHz:

    ```
    lpf 2 12000
    ```

- 3rd order (18dB/octave) high-pass filter at 120Hz, and 1st order
  (6dB/octave) low-pass filter at 6.5kHz:

    ```
    hpf 3 120 lfp 1 6500
    ```


