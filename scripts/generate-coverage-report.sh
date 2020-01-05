#!/bin/bash
set -e
gcov="${1:-gcov}"
branch="$(git rev-parse --abbrev-ref HEAD)"
commit="$(git rev-parse --short HEAD)"
lcov --gcov-tool "${gcov}" -d . --capture --no-external --output-file coverage.info | grep -E -v 'ignoring|skipping'
genhtml --show-details --legend --sort --frames --title="dosbox-staging ${branch} (${commit})" --output-directory coverage-report coverage.info
