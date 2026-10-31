# SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check fpu_math_probe results against an independent Decimal oracle."""

from decimal import Decimal, localcontext
import random
import subprocess
import sys

PI = Decimal(
    '3.14159265358979323846264338327950288419716939937510582097494459230781640628620899'
    '862803482534211706798214808651328230664709384460955058223172535940812848111745028'
    '410270194'
)


def sincos(value):
    """Reduce the argument and evaluate sine and cosine with Taylor series."""
    quadrant = (value / (PI / 2)).to_integral_value()
    reduced = value - quadrant * PI / 2
    sine = sine_term = reduced
    cosine = cosine_term = Decimal(1)
    for index in range(2, 400, 2):
        sine_term = sine_term * -reduced * reduced / (index * (index + 1))
        cosine_term = cosine_term * -reduced * reduced / ((index - 1) * index)
        sine += sine_term
        cosine += cosine_term
    return [(sine, cosine), (cosine, -sine), (-sine, -cosine), (-cosine, sine)][
        int(quadrant) % 4
    ]


def atan(value):
    """Evaluate arctangent after reciprocal and pi/4 argument reductions."""
    negative = value < 0
    value = abs(value)
    reciprocal = value > 1
    if reciprocal:
        value = 1 / value
    shifted = value > Decimal('.5')
    if shifted:
        value = (value - 1) / (value + 1)
    term = result = value
    for index in range(3, 1400, 2):
        term = term * -value * value
        result += term / index
    if shifted:
        result += PI / 4
    if reciprocal:
        result = PI / 2 - result
    return -result if negative else result


def raw(value, rounding):
    """Round a Decimal to the raw extended80 exponent and significand."""
    if not value:
        return (0, 0)
    negative = value < 0
    value = abs(value)
    numerator, denominator = value.as_integer_ratio()
    exponent = numerator.bit_length() - denominator.bit_length()
    if Decimal(2) ** exponent > value:
        exponent -= 1
    scaled = value * Decimal(2) ** (63 - exponent)
    significand = int(scaled)
    remainder = scaled - significand
    if rounding == 0:
        increment = remainder > Decimal('.5') or (
            remainder == Decimal('.5') and significand % 2
        )
    else:
        increment = remainder > 0 and (
            (rounding == 1 and negative) or (rounding == 2 and not negative)
        )
    significand += bool(increment)
    if significand == 2**64:
        significand //= 2
        exponent += 1
    return ((exponent + 16383) | (0x8000 if negative else 0), significand)


def evaluate(operation, value):
    """Compute the mathematical reference for one probe operation."""
    if operation < 3:
        sine, cosine = sincos(value)
        return [sine, cosine, sine / cosine][operation]
    ln2 = Decimal(2).ln()
    if operation == 3:
        return (value * ln2).exp() - 1
    if operation == 4:
        return value.ln() / ln2
    if operation == 5:
        return (1 + value).ln() / ln2
    return atan(value)


def generate_inputs():
    """Yield deterministic random extended80 inputs in each operation's domain."""
    generator = random.Random(51349)
    for operation in range(7):
        for _ in range(50):
            maximum = 62 if operation < 3 else 0 if operation == 3 else 100
            exponent = 16383 + generator.randint(-110, maximum)
            if operation == 5:
                exponent = min(exponent, 16381)
            significand = generator.randrange(2**63, 2**64)
            sign = generator.choice([0, 0x8000]) if operation != 4 else 0
            yield operation, exponent, significand, sign


def generate_cases():
    """Build probe inputs, expected results, and tiny-cosine compatibility flags."""
    cases = []
    expected = []
    compatibility = []
    for operation, exponent, significand, sign in generate_inputs():
        value = Decimal(significand) * Decimal(2) ** (exponent - 16383 - 63)
        value *= -1 if sign else 1
        result = evaluate(operation, value)
        tiny_cosine = operation == 1 and abs(value) < Decimal(2) ** -33
        if tiny_cosine:
            # Native-backend compatibility selects +1; independently check
            # its error against the high-precision mathematical cosine.
            assert abs(Decimal(1) - result) < Decimal(2) ** -67
        for rounding in range(4):
            cases.append((operation, rounding, exponent | sign, significand))
            expected.append((0x3fff, 2**63) if tiny_cosine else raw(result, rounding))
            compatibility.append(tiny_cosine)
    return cases, expected, compatibility


def main():
    """Run the supplied probe executable and report numerical mismatches."""
    with localcontext() as context:
        context.prec = 170
        cases, expected_results, compatibility = generate_cases()
    data = ''.join(f'{op} {rc} {ex:x} {sig:x}\n' for op, rc, ex, sig in cases)
    output = subprocess.check_output([sys.argv[1]], input=data, text=True)
    lines = output.splitlines()
    if len(lines) != len(cases):
        sys.exit(f'Expected {len(cases)} probe results, received {len(lines)}')
    errors = []
    for case, expected, line in zip(cases, expected_results, lines):
        actual = tuple(int(token, 16) for token in line.split())
        if actual != expected:
            errors.append((case, expected, actual))
    print(len(cases), 'cases,', len(errors), 'mismatches;', sum(compatibility),
          'native-compatible tiny-cosine cases with Decimal error bounds')
    for error in errors[:20]:
        print(error)
    return bool(errors)


if __name__ == '__main__':
    sys.exit(main())
