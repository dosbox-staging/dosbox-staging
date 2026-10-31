# Portable 80-bit x87 backend

The accurate non-native backend stores the explicit 64-bit significand and the full
15-bit exponent of every x87 register. Basic arithmetic, square root, comparisons,
integer conversion, and single/double conversion use Berkeley SoftFloat 3e with
its 8086 specialization. Guest memory is serialized explicitly; host padding,
host floating-point format, and host rounding mode do not affect it.

The Intel SDM references used are Volume 1, chapter 8 (particularly precision
control, unsupported formats, and exceptions), and Volume 2's individual x87
instruction descriptions. Precision control applies only to arithmetic and square
root, retaining the extended exponent range at 24- or 53-bit significand precision.
Constants use their extended encodings and guest rounding control. This backend
does not need the fast backend's CHOP snapping or integer-copy side channel.

Transcendentals use software binary128 intermediate arithmetic. Trigonometric
argument reduction uses a 256-bit fixed-point 2/pi constant before binary128
polynomial evaluation. Small-argument log1p and expm1 avoid cancellation. FPREM
and FPREM1 perform integer significand division, produce quotient bits, and use
a 32-bit reduction step when C2 indicates an incomplete reduction.

On non-x86 hosts, select the backend in the configuration file:

```ini
[cpu]
fpu = accurate
```

The default, `fast`, preserves the existing reduced-precision implementation.
`accurate` selects software 80-bit emulation at a higher CPU cost. Both are built
into the same executable and work independently of the CPU `core` setting.
Changes require a restart. The dynamic recompiler captures the selected operation
addresses when generating code. On x86-64 hosts, native x87 emulation is always
used; this setting has no effect.

## Accuracy boundaries

This is an arithmetic/backend replacement, not complete x87 exception and CPU
integration. In particular:

- DOSBox's existing CPU-level FWAIT/#MF/IRQ13 delivery is not implemented here.
  Pending unmasked exceptions set ES/B; invalid, denormal, and zero-divide prevent
  destination writes. Precision exceptions retain the rounded result. Unmasked
  overflow/underflow at the extended rounding boundary produce exponent-wrapped
  register results. Extreme transcendental intermediates can still overflow the
  binary128 evaluator before that boundary and need further coverage.
- The existing environment format still does not track instruction/data pointers
  or the last opcode. Register payloads, tags, control, status, and TOP are saved.
- Transcendental results approximate the mathematical functions; they do not
  reproduce a particular Intel processor's microcode or its 66-bit pi reduction.
  No exhaustive correct-rounding or Intel error-bound proof has been performed.
  C1 rounding indications are implemented for tiny FSIN/FCOS inputs; other
  transcendental C1 indications and extreme subnormal exceptions still need work.
- Page-fault restart of an instruction after a prepared push remains a CPU/memory
  integration concern. The backend is single-emulated-CPU state, as before.

## Validation

The regression suite includes precision/rounding combinations, 64-bit integer
boundaries, raw 80-bit payload preservation, unsupported encodings, exception
priority, masked stack faults, constants, partial remainder, large trig reduction,
small log1p/atan, BCD, store rounding, and save/restore.

After building the debug configuration:

```sh
build/debug/tests/dosbox_tests --gtest_filter='Fpu80*:FpuBackendConfig.*:PortableFpu/*'
```

`tests/fpu_oracle.cpp` is a standalone differential probe. Build its reference
version on x86-64 (on Apple Silicon, `-arch x86_64` runs the x87 instruction
stream through Rosetta, which is a compatibility reference rather than physical
Intel hardware):

```sh
c++ -arch x86_64 -std=c++23 -O2 tests/fpu_oracle.cpp -o /tmp/x87-native
c++ -std=c++23 -O2 -DSOFTWARE -DSOFTFLOAT_FAST_INT64 \
  -I src -I src/libs -I build/debug/include -I src/libs/softfloat \
  -I build/debug/vcpkg_installed/arm64-osx/include \
  tests/fpu_oracle.cpp build/debug/src/libs/softfloat/libsoftfloat.a \
  src/fpu/float80.cpp -o /tmp/x87-software
/tmp/x87-native 1000000 > /tmp/x87-native.txt
/tmp/x87-software 1000000 > /tmp/x87-software.txt
cmp /tmp/x87-native.txt /tmp/x87-software.txt
```

The probe covers add/subtract/multiply/divide/square-root/integer-round/remainder/
nearest-remainder/scale and compares raw result bits and defined status bits.
Add `-DUNMASKED` to both commands to test unmasked overflow, underflow, and
precision responses; the reference uses FNSAVE to capture state without waiting
for exception delivery. This requires physical x86 hardware: Rosetta was observed
returning masked overflow/underflow results despite retaining unmasked CW bits.
Add `-DSPECIAL` to both compile commands and run 10800 cases for every combination
of ten exceptional encodings, nine operations, three precision modes, and four
rounding modes. Partial remainder steps are covered by unit tests; the finite
oracle constrains remainder inputs to a complete reduction.

An independent Python Decimal oracle checks transcendental results at 170 decimal
digits, including arguments near 2^63 and tiny directed-rounding corrections:

```sh
c++ -std=c++23 -O2 -DSOFTFLOAT_FAST_INT64 -I src -I src/libs/softfloat \
  tests/fpu_math_probe.cpp src/fpu/float80.cpp \
  build/debug/src/libs/softfloat/libsoftfloat.a -o /tmp/x87-math
python3 tests/check_fpu_math.py /tmp/x87-math
```

These commands assume the macOS ARM64 debug preset; adapt architecture/include
paths on other hosts. Tests and samples are evidence, not exhaustive conformance.

Observed validation on macOS ARM64 (2026-10-31): debug DOSBox and tests build
successfully; the full suite reports 973 passed and one skipped, including 42 FPU
regressions and 13 backend-selection tests. The latter execute both backends across
all five portable CPU cores and verify startup-only configuration. Config-file
startup was also checked with both values. The masked Rosetta differential probes match 1,000,000 finite cases
and 10,800 exceptional-encoding cases. The Decimal probe matches all 1,400 sampled
transcendental cases. Unmasked exponent-wrap behavior has Intel-derived exact
regression cases, but has not been compared with physical Intel hardware.

## MCPDIAG sine/cosine compatibility

MCPDIAG exposed twelve missing FSIN underflow indications. A nonzero tiny sine
is inexact even when its rounded result equals the input, and a subnormal result
must also set UE. This is handled, including C1 direction and unmasked UE's
exponent-scaled result before subnormal significand loss.

The native DOSBox backend's `FPUD_TRIG` wrapper in
`src/fpu/fpu_instructions_x86.h` executes FCOS without loading the guest control
word. It therefore uses the host rounding mode, normally nearest. A standalone
probe that explicitly loads the guest control word measures a different path.
That distinction explains why the previous direct Rosetta probe agreed with the
software backend while MCPDIAG passed in the user's native-backend DOSBox build.
The earlier conclusion that the eight failures could be dismissed as incorrect
MCPDIAG expectations was not supported by the end-to-end evidence.

For |x| < 2^-33, the portable backend now approximates cosine as +1 for either
sign and every guest rounding mode, matching the native wrapper under its default
host rounding mode. The error relative to the mathematical cosine is less than
2^-67, or 1/8 ULP immediately below 1. This is within Intel SDM Vol. 1 section
8.3.10's transcendental error allowance. This is a deliberate native-backend
compatibility approximation; it can differ from a directly executed FCOS with a
guest-selected downward or truncate rounding mode. General cosine calculations
and sine rounding continue to use guest rounding control.

The updated MCPDIAG run reports 64 sine passes and 64 cosine passes, with zero
failures in every displayed operation. All 64 combinations of eight tiny cosine
boundary inputs, two signs, and four rounding modes match a reproduction of the
native wrapper under Rosetta, including result bits and exception/C1 flags.
Physical Intel hardware has not been tested for this follow-up.

The full regression suite reports 958 passed and one skipped. The independent
Decimal check covers 1,316 correctly rounded mathematical samples and 84 tiny
cosine compatibility samples. For the latter it checks both the selected +1
result and its error bound against the high-precision mathematical cosine.

`tests/fpu_trig_probe.cpp` accepts operation (0 sine, 1 cosine), raw exponent,
raw significand, and control word on stdin. To reproduce the native wrapper's
host-CW behavior with a deterministic nearest host mode:

```sh
c++ -arch x86_64 -std=c++23 -O2 -DNATIVE_DOSBOX \
  tests/fpu_trig_probe.cpp -o /tmp/x87-trig
printf '1 8001 8000000000000000 077f\n' | /tmp/x87-trig
```

Omit `-DNATIVE_DOSBOX` to test direct x87 execution with the supplied guest CW.
Compile with `-DSOFTWARE -DSOFTFLOAT_FAST_INT64` and the include/library paths
from the arithmetic probe above, adding `src/fpu/float80.cpp`, to compare the
portable backend. The regression tests retain all tiny cosine input families,
check even symmetry and status flags, and also test tiny sine underflow.

## MCPDIAG simple Run Diagnostics follow-up

The Environment failure was caused by FLDLG2's rounding-direction metadata,
not by saved register payload corruption. Its nearest significand
`9A209A84FBCFF799` is above log10(2). The old metadata incorrectly treated it as
below, making upward loads one ULP too large and failing the subsequent upward
PI/log10(2) division and exact status comparison. The corrected encodings are
`...F799` for nearest/up and `...F798` for down/chop, matching direct x87 constant
loads under Rosetta. Regression tests cover all four modes and the Environment
test's division/status check. The user verified that Environment now passes.

The simpler Transcendental test also fails in the user's native-backend Rosetta
run and on a Windows 11 Intel machine. In the captured software run, the failing
comparison follows FLDLG2 and three successive F2XM1 instructions. MCPDIAG expects
`3FFC:83A4D99B068361BC` exactly; the software backend and a direct x87-instruction
probe under Rosetta both produce `3FFC:83A4D99B068361BE`. No numerical workaround
was applied to force this shared baseline failure to pass. The raw result on the
Windows Intel machine was not captured.

Temporary instruction/CPU tracing has been removed. The full regression suite
after the FLDLG2 correction reports 960 passed and one skipped.

## Performance checks

The build enables `SOFTFLOAT_FAST_INT64` and `INLINE_LEVEL=5`. GCC/Clang builds
also enable `SOFTFLOAT_BUILTIN_CLZ` and, after a compiler capability check,
`SOFTFLOAT_INTRINSIC_INT128`. ARM64 and x86-64 targets enable
`SOFTFLOAT_FAST_DIV64TO32`. A private forced-include header loads the library's
existing intrinsic implementations after its platform definitions; defining the
intrinsic macros alone would not activate them with the bundled platform header.
All configuration lives outside the SoftFloat source directory. Other compilers
retain the portable implementations.

### Opt-in instruction profiling

On the portable FPU backend, launch with `DOSBOX_FPU_PROFILE=1` to profile
`fpu=fast` or `fpu=accurate`. Unset the variable (or use any value other than `1`)
to retain normal dispatch without per-call counters or clock reads. Native x87
backends are not instrumented.

The profiler counts every backend operation and times approximately one call in
1,024, spreading samples across recurring guest loops. `LOG_INFO()` emits
cumulative calls, timed samples, and total sampled nanoseconds for each operation
every five seconds during FPU activity and once at normal process exit. Divide
sampled nanoseconds by sample count for the mean sampled duration. Counters also
include startup/loading and stack helpers; they are not retired x87 instruction
counts. Internal helper calls are included in the enclosing operation's timing,
not counted separately by the profiler.

Timings include clock overhead, and profiling affects benchmark performance.
Compare both backends under the same instrumentation and check whole-game
performance with profiling disabled.
