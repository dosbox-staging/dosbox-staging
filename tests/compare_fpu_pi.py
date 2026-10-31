# SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compare trig reduction with Rosetta x87; takes native and software probe paths.

Example builds from the repository root (macOS ARM64 debug configuration):

    c++ -arch x86_64 -std=c++23 -O2 tests/fpu_trig_probe.cpp -o /tmp/pi-native
    c++ -std=c++23 -O2 -DSOFTFLOAT_FAST_INT64 -I src -I src/libs/softfloat \
        tests/fpu_math_probe.cpp src/fpu/float80.cpp \
        build/debug/src/libs/softfloat/libsoftfloat.a -o /tmp/pi-software
    python3 tests/compare_fpu_pi.py /tmp/pi-native /tmp/pi-software > /tmp/pi.csv

The native executable uses guest CW 037f (nearest, extended precision). Results are written to
CSV on stdout, with a summary on stderr. No emulator implementation is changed.
"""

import csv
from decimal import Decimal, localcontext
import subprocess
import sys

from check_fpu_math import PI, raw, sincos


def decode(exponent, significand):
    """Decode a finite normal extended80 value without host floating point."""
    magnitude = Decimal(significand) * Decimal(2) ** ((exponent & 0x7fff) - 16383 - 63)
    return -magnitude if exponent & 0x8000 else magnitude


def inputs():
    """Generate exact binary inputs and neighbors of rounded multiples of pi."""
    for power in (0, 10, 30, 50, 60, 62):
        yield f'2^{power}', *raw(Decimal(2) ** power, 0)
    for power in (0, 10, 30, 50, 60):
        for divisor in (1, 2):
            exponent, significand = raw(PI * Decimal(2) ** power / divisor, 0)
            for offset in (-1, 0, 1):
                label = f'pi*2^{power}/{divisor} neighbor {offset:+}'
                yield label, exponent, significand + offset


def run_probes(cases):
    """Run identical raw inputs through the native and software executables."""
    native_data = ''.join(f'{op} {ex:x} {sig:x} 037f\n'
                          for _, op, ex, sig in cases)
    software_data = ''.join(f'{op} 0 {ex:x} {sig:x}\n'
                            for _, op, ex, sig in cases)
    native = subprocess.check_output([sys.argv[1]], input=native_data, text=True)
    software = subprocess.check_output([sys.argv[2]], input=software_data, text=True)
    native_results = [tuple(int(word, 16) for word in line.split('->')[1].split()[:2])
                      for line in native.splitlines()]
    software_results = [tuple(int(word, 16) for word in line.split())
                        for line in software.splitlines()]
    if len(native_results) != len(cases) or len(software_results) != len(cases):
        sys.exit('Probe returned an unexpected number of results')
    return native_results, software_results


def format_bits(pair):
    """Format an extended80 exponent/significand pair."""
    return f'{pair[0]:04x}:{pair[1]:016x}'


def compare_case(case, native, software, finite_pi):
    """Return one CSV row and exact/one-ULP match indicators."""
    value = decode(case[2], case[3])
    # Intel describes sin/cos(x*pi/Pi). Reduce exactly against finite Pi
    # before evaluating, retaining cancellation in the Decimal reference.
    quadrant = (value / (finite_pi / 2)).to_integral_value()
    remainder = value - quadrant * finite_pi / 2
    sine, cosine = sincos(remainder * PI / finite_pi)
    model_value = [(sine, cosine), (cosine, -sine),
                   (-sine, -cosine), (-cosine, sine)][int(quadrant) % 4][case[1]]
    model = raw(model_value, 0)
    software_error = abs(decode(*software) - decode(*native))
    model_error = abs(decode(*model) - decode(*native))
    ulp = Decimal(2) ** ((native[0] & 0x7fff) - 16383 - 63)
    row = [case[0], ('sin', 'cos')[case[1]], format_bits((case[2], case[3])),
           format_bits(native), format_bits(software), format_bits(model),
           f'{software_error:.4E}', f'{model_error:.4E}', f'{model_error / ulp:.4E}']
    return row, (software == native, model == native, model_error <= ulp)


def compare():
    """Compare true-pi reduction and Intel's documented finite-pi model."""
    # Intel SDM Vol. 1 section 8.3.8: 0.C90FDAA22168C234C * 2^2.
    # The trailing zero bits leave 66 significant bits.
    finite_pi = Decimal(0xc90fdaa22168c234c) / Decimal(2) ** 66
    cases = [(label, op, ex | sign, sig)
             for label, ex, sig in inputs()
             for sign in (0, 0x8000) for op in (0, 1)]
    native_results, software_results = run_probes(cases)
    counts = [0, 0, 0]
    writer = csv.writer(sys.stdout)
    writer.writerow(['input', 'operation', 'raw_input', 'rosetta', 'software',
                     'pi66_model', 'software_abs_error_vs_rosetta',
                     'pi66_abs_error_vs_rosetta', 'pi66_error_in_rosetta_ulps'])
    for case, native, software in zip(cases, native_results, software_results):
        row, metrics = compare_case(case, native, software, finite_pi)
        writer.writerow(row)
        counts = [count + metric for count, metric in zip(counts, metrics)]
    print(f'{len(cases)} cases; exact matches with Rosetta: software={counts[0]}, '
          f'pi66 model={counts[1]}; pi66 within 1 result ULP={counts[2]}', file=sys.stderr)


def main():
    """Use enough Decimal precision to retain cancellation near large multiples."""
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    with localcontext() as context:
        context.prec = 170
        compare()


if __name__ == '__main__':
    main()
