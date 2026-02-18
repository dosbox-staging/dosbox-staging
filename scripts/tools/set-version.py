#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
# SPDX-License-Identifier: GPL-2.0-or-later

# pylint: disable=invalid-name
# pylint: disable=missing-docstring
# pylint: disable=too-few-public-methods


"""
Set the DOSBox Staging version across all relevant files.

This script ensures version consistency across the codebase during the
release process. It supports the following workflows:

  Preparing an RC:         ./scripts/set-version.py 0.81.0 --rc 1
  Preparing final release: ./scripts/set-version.py 0.81.0
  Post-release bump:       ./scripts/set-version.py 0.82.0 --alpha
  Check version sync:      ./scripts/set-version.py --check

<h>FILES UPDATED BY THIS SCRIPT
----------------------------</>
  - <f>CMakeLists.txt</>
      <c>VERSION</> in project() and suffix in <c>DOSBOX_VERSION</>

  - <f>vcpkg.json</>
      "version" field

  - <f>.github/workflows/release-notes-preview.yml</>
      <c>PRE_RELEASE_TAG</> environment variable

  - <f>.github/ISSUE_TEMPLATE/bug_report.yml</>
  - <f>.github/ISSUE_TEMPLATE/feature_request.yml</>
  - <f>.github/ISSUE_TEMPLATE/question.yml</>
      Version placeholder in example text

<h>FILES NOT UPDATED (require manual intervention)
-----------------------------------------------</>
  - <f>extras/linux/org.dosbox_staging.dosbox_staging.metainfo.xml</>
      Requires release date; only updated for actual releases, not alpha.
      Use --release-date flag to add a release entry.

  - <f>.github/actions/set-common-vars/action.yml</> (<c>VCPKG_EXT_DEPS_VERSION</>)
      External dependency version with its own iteration counter (e.g.,
      v0.83.0-4). Incremented independently when vcpkg deps change.

  - <f>.github/workflows/release-notes-preview.yml</> (<c>RELEASE_START_TIME</>)
      Must be set manually to the previous release's publish timestamp.

  - <f>.github/workflows/deploy-website.yml</> (release branch)
      Updated manually when creating a new release branch.

  - Copyright years in various files
      Updated annually, not per-release.
"""

import argparse
import os
import re
import sys
from datetime import date
from pathlib import Path


class Colors:
    """ANSI color codes (only used when output is a TTY)"""
    RESET = "\033[0m"
    BOLD = "\033[1m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    CYAN = "\033[36m"
    BRIGHT_WHITE = "\033[97m"

    @classmethod
    def enabled(cls):
        """Check if colors should be enabled."""
        # Respect NO_COLOR environment variable
        if os.environ.get("NO_COLOR"):
            return False
        # Enable if stdout is a TTY
        return sys.stdout.isatty()


def colorize(text, *codes):
    """Apply color codes to text if colors are enabled."""
    if Colors.enabled():
        return "".join(codes) + text + Colors.RESET
    return text


def filepath(path):
    """Format a filepath with highlighting."""
    return colorize(str(path), Colors.BOLD, Colors.CYAN)


def const(name):
    """Format a constant name with highlighting."""
    return colorize(name, Colors.BOLD, Colors.BRIGHT_WHITE)


def colorize_help_text(text):
    """Process color tags: <f> files, <c> constants, <h> headers."""
    if Colors.enabled():
        text = re.sub(r'<f>([^<]+)</>', lambda m: filepath(m.group(1)), text)
        text = re.sub(r'<c>([^<]+)</>', lambda m: const(m.group(1)), text)
        text = re.sub(r'<h>(.*?)</>',
                      lambda m: colorize(m.group(1), Colors.YELLOW), text, flags=re.DOTALL)
        return text

    return re.sub(r'<[fch]>([^<]+)</>', r'\1', text)


# Repository root (script is in scripts/tools/)
REPO_ROOT = Path(__file__).parent.parent.parent

# Files to update
CMAKE_FILE = REPO_ROOT / "CMakeLists.txt"
VCPKG_FILE = REPO_ROOT / "vcpkg.json"
RELEASE_NOTES_WORKFLOW = REPO_ROOT / ".github/workflows/release-notes-preview.yml"
ISSUE_TEMPLATES = [
    REPO_ROOT / ".github/ISSUE_TEMPLATE/bug_report.yml",
    REPO_ROOT / ".github/ISSUE_TEMPLATE/feature_request.yml",
    REPO_ROOT / ".github/ISSUE_TEMPLATE/question.yml",
]
METAINFO_FILE = REPO_ROOT / "extras/linux/org.dosbox_staging.dosbox_staging.metainfo.xml"

def custom_error(message):
    sys.stderr.write(f'error: {message}\n')
    sys.exit(2)

def parse_args():
    """Parse command line arguments."""

    parser = argparse.ArgumentParser(
        description=colorize_help_text(__doc__),
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "version",
        nargs="?",
        help="Base version number (e.g., 0.83.0)"
    )
    parser.add_argument(
        "--alpha",
        action="store_true",
        help="Add -alpha suffix (for post-release main branch)"
    )
    parser.add_argument(
        "--rc",
        type=int,
        metavar="N",
        help="Add -rcN suffix (for release candidates)"
    )
    parser.add_argument(
        "--release-date",
        metavar="YYYY-MM-DD",
        help="Add release entry to metainfo.xml with this date"
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Check if all version strings are in sync (no changes made)"
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be changed without modifying files"
    )

    parser.error = custom_error

    args = parser.parse_args()

    if not args.check and not args.version:
        parser.error("version is required unless using --check")

    if args.version and not re.match(r"^\d+\.\d+\.\d+$", args.version):
        parser.error(f"Invalid version format: {args.version} (use X.Y.Z, e.g., 0.83.0)")
    if args.alpha and args.rc:
        parser.error("--alpha and --rc are mutually exclusive")

    if args.rc is not None and args.rc < 1:
        parser.error("RC number must be positive")
    if args.release_date:
        try:
            date.fromisoformat(args.release_date)
        except ValueError:
            parser.error(f"Invalid date format: {args.release_date} (use YYYY-MM-DD)")

    return args


def build_version_string(base_version, alpha=False, rc=None):
    """Build the full version string with optional suffix."""
    if rc:
        return f"{base_version}-rc{rc}"
    if alpha:
        return f"{base_version}-alpha"
    return base_version


def get_suffix(alpha=False, rc=None):
    """Get just the suffix part for CMakeLists.txt."""
    if rc:
        return f"-rc{rc}"
    if alpha:
        return "-alpha"
    return ""


def read_file(path):
    """Read file contents."""
    return path.read_text(encoding="utf-8")


def write_file(path, content, dry_run=False):
    """Write file contents (unless dry_run)."""
    if dry_run:
        label = colorize("[dry-run]", Colors.YELLOW)
        print(f"  {label} Would update {filepath(path)}")
    else:
        path.write_text(content, encoding="utf-8")
        label = colorize("Updated", Colors.GREEN)
        print(f"  {label} {filepath(path)}")


def update_cmake(base_version, suffix, dry_run=False):
    """Update CMakeLists.txt VERSION and DOSBOX_VERSION suffix."""
    content = read_file(CMAKE_FILE)

    # Update VERSION in project()
    new_content = re.sub(
        r'(project\(dosbox-staging[^)]*VERSION\s+)\d+\.\d+\.\d+',
        rf'\g<1>{base_version}',
        content
    )

    # Update DOSBOX_VERSION suffix
    if suffix:
        new_content = re.sub(
            r'set\(DOSBOX_VERSION\s+\$\{PROJECT_VERSION\}[^)]*\)',
            f'set(DOSBOX_VERSION ${{PROJECT_VERSION}}{suffix})',
            new_content
        )
    else:
        # No suffix - just use PROJECT_VERSION
        new_content = re.sub(
            r'set\(DOSBOX_VERSION\s+\$\{PROJECT_VERSION\}[^)]*\)',
            'set(DOSBOX_VERSION ${PROJECT_VERSION})',
            new_content
        )

    if new_content != content:
        write_file(CMAKE_FILE, new_content, dry_run)
        return True
    return False


def update_vcpkg_json(full_version, dry_run=False):
    """Update vcpkg.json version field."""
    content = read_file(VCPKG_FILE)

    new_content = re.sub(
        r'^(\s*"version"\s*:\s*")[^"]*(")',
        rf'\g<1>{full_version}\2',
        content,
        count=1,
        flags=re.MULTILINE
    )

    if new_content != content:
        write_file(VCPKG_FILE, new_content, dry_run)
        return True
    return False


def update_release_notes_workflow(full_version, dry_run=False):
    """Update PRE_RELEASE_TAG in release-notes-preview.yml."""
    content = read_file(RELEASE_NOTES_WORKFLOW)

    new_content = re.sub(
        r'(PRE_RELEASE_TAG:\s*)\S+',
        rf'\g<1>{full_version}',
        content
    )

    if new_content != content:
        write_file(RELEASE_NOTES_WORKFLOW, new_content, dry_run)
        return True
    return False


def update_issue_templates(full_version, dry_run=False):
    """Update version placeholder in issue templates."""
    updated = False

    for template_path in ISSUE_TEMPLATES:
        if not template_path.exists():
            label = colorize("Warning:", Colors.YELLOW)
            print(f"  {label} {filepath(template_path)} not found")
            continue

        content = read_file(template_path)

        # Match pattern like: E.g., 0.83.0-alpha, git version
        new_content = re.sub(
            r'(placeholder:\s*["\']E\.g\.,\s*)\d+\.\d+\.\d+(?:-\w+)?',
            rf'\g<1>{full_version}',
            content
        )

        if new_content != content:
            write_file(template_path, new_content, dry_run)
            updated = True

    return updated


def add_metainfo_release(base_version, release_date, dry_run=False):
    """Add a release entry to metainfo.xml."""
    content = read_file(METAINFO_FILE)

    # Check if this version already exists
    if f'version="{base_version}"' in content:
        label = colorize("Warning:", Colors.YELLOW)
        print(f"  {label} Release {base_version} already exists in metainfo.xml")
        return False

    # Find the <releases> section and add new entry at the top
    new_entry = f'\t<release version="{base_version}" date="{release_date}"/>'

    new_content = re.sub(
        r'(<releases>\s*\n)',
        rf'\g<1>{new_entry}\n',
        content
    )

    if new_content != content:
        write_file(METAINFO_FILE, new_content, dry_run)
        return True
    return False


def extract_version_from_cmake():
    """Extract current version from CMakeLists.txt."""
    content = read_file(CMAKE_FILE)

    # Extract base version
    match = re.search(r'project\(dosbox-staging[^)]*VERSION\s+(\d+\.\d+\.\d+)', content)
    base_version = match.group(1) if match else None

    # Extract suffix
    match = re.search(r'set\(DOSBOX_VERSION\s+\$\{PROJECT_VERSION\}([^)]*)\)', content)
    suffix = match.group(1).strip() if match else ""

    return base_version, suffix


def extract_version_from_vcpkg():
    """Extract current version from vcpkg.json."""
    content = read_file(VCPKG_FILE)
    match = re.search(r'^\s*"version"\s*:\s*"([^"]*)"', content, re.MULTILINE)
    return match.group(1) if match else ""


def extract_version_from_workflow():
    """Extract current version from release-notes-preview.yml."""
    content = read_file(RELEASE_NOTES_WORKFLOW)
    match = re.search(r'PRE_RELEASE_TAG:\s*(\S+)', content)
    return match.group(1) if match else ""


def extract_version_from_issue_template():
    """Extract current version from first issue template."""
    if not ISSUE_TEMPLATES[0].exists():
        return ""
    content = read_file(ISSUE_TEMPLATES[0])
    match = re.search(r'placeholder:\s*["\']E\.g\.,\s*(\d+\.\d+\.\d+(?:-\w+)?)', content)
    return match.group(1) if match else ""


def check_versions():
    """Check if all versions are in sync."""
    base_version, suffix = extract_version_from_cmake()
    cmake_full = f"{base_version}{suffix}" if base_version else "NOT FOUND"

    vcpkg_version = extract_version_from_vcpkg()
    workflow_version = extract_version_from_workflow()
    template_version = extract_version_from_issue_template()

    print("Current versions:")
    print(f"  {filepath('CMakeLists.txt')}:              {cmake_full}")
    print(f"  {filepath('vcpkg.json')}:                  {vcpkg_version}")
    print(f"  {filepath('release-notes-preview.yml')}:   {workflow_version}")
    print(f"  {filepath('Issue templates')}:             {template_version}")

    all_versions = [cmake_full, vcpkg_version, workflow_version, template_version]
    all_versions = [v for v in all_versions if v and v != "NOT FOUND"]

    if all_versions and len(set(all_versions)) == 1:
        msg = colorize("All versions are in sync.", Colors.GREEN)
        print(f"\n{msg}")
        return True

    msg = colorize("WARNING: Versions are out of sync!", Colors.YELLOW, Colors.BOLD)
    print(f"\n{msg}")
    return False


def main():
    args = parse_args()

    if args.check:
        sys.exit(0 if check_versions() else 1)

    base_version = args.version
    full_version = build_version_string(base_version, args.alpha, args.rc)
    suffix = get_suffix(args.alpha, args.rc)

    print(f"Setting version to: {const(full_version)}")
    if args.dry_run:
        print("(dry-run mode - no files will be modified)")
    print()

    # Update all files
    update_cmake(base_version, suffix, args.dry_run)
    update_vcpkg_json(full_version, args.dry_run)
    update_release_notes_workflow(full_version, args.dry_run)
    update_issue_templates(full_version, args.dry_run)

    # Optionally add metainfo release entry
    if args.release_date:
        print()
        add_metainfo_release(base_version, args.release_date, args.dry_run)

    print()
    header = colorize("Files not updated", Colors.YELLOW, Colors.BOLD)
    print(f"{header} (require manual intervention):")

    print(f"  - {const('RELEASE_START_TIME')} in "
          f"{filepath(RELEASE_NOTES_WORKFLOW.relative_to(REPO_ROOT))}")

    print(f"  - {const('VCPKG_EXT_DEPS_VERSION')} in "
          f"{filepath('.github/actions/set-common-vars/action.yml')}")

    if not args.release_date:
        print(f"  - {filepath(METAINFO_FILE.relative_to(REPO_ROOT))} "
              f"(use --release-date for actual releases)")


if __name__ == "__main__":
    main()
