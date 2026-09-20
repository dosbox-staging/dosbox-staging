# GitHub CI workflows

This directory holds the GitHub Actions configuration for DOSBox Staging:
build and analysis workflows, the website deploy pipeline, composite actions,
and issue/PR templates.

## Analysis workflows

Analysis workflows run when creating a new PR branch, pushing commits to any
branch, and merging to a branch.

| Workflow                    | Purpose
| ---                         | ---
| `linting.yml`               | Shellcheck, pylint, markdownlint, appstream-util
| `pvs-studio.yml`            | PVS-Studio static analysis

## Build workflows

Build workflows run when creating a new PR branch, pushing commits to any
branch, and merging to a branch.

The prerequisite for starting these workflows is successfully passed analysis
steps.

| Workflow                    | Purpose
| ---                         | ---
| `windows.yml`               | Windows x64 release builds and installer packaging
| `macos.yml`                 | macOS universal (x86_64 + arm64) builds and DMG packaging
| `linux.yml`                 | Linux builds (multiple compiler combos) and release tarball

## Website/documentation workflows

See [docs/DOCUMENTATION.md](docs/DOCUMENTATION.md#deploying-the-website--documentation)
for further details on how these workflows are used. The [Automatic preview deploys for PRs and
`main`](docs/DOCUMENTATION.md#automatic-preview-deploys-for-prs-and-main)
details the automatic website/documentation deploys.

| Workflow                    | Purpose
| ---                         | ---
| `deploy-website*.yml`       | Website + preview deploys -- see [`workflows/deploy-website.md`](workflows/deploy-website.md)
| `release-notes-preview.yml` | Generates release notes for upcoming versions

## Composite actions

We have a couple of reusable of composite actions in `.github/actions/`:

- `setup-vcpkg/` -- pins vcpkg to the manifest baseline and configures
  the two vcpkg caches (see below)

- `set-common-vars/` -- exports shared env vars (versions of external
  artifact bundles, etc.)

- `generate-release-notes/` -- generates the release notes used by the
  release notes workflow

- `retry/` -- runs a shell command with retries and linear backoff;
  used where the command has no native retry flag (e.g. `apt-get`)

## External dependency surfaces

Third-party services the CI touches that can flake or go down:

- **vcpkg port upstreams** -- source tarballs for packages declared in
  `vcpkg.json` and the overlay ports in `vcpkg-ports/**`. Fetched whenever the
  binary cache and downloads cache both miss.

- **`dosbox-staging/dosbox-staging-ext` release assets** -- pre-built
  dynamic libraries that take a long time to build, fetched in each
  platform's build workflow.

- **`johnnovak/Nuked-SC55-CLAP` release assets** -- CLAP plugin
  binaries, fetched in each platform's build workflow.

- **PyPI** -- pip packages for building the website and documentation (see
  `deploy-website.yml`) and linting (see `linting.yml`)

- **Ubuntu apt mirrors** -- distro packages on `ubuntu-22.04` and
  `ubuntu-slim` runners

- **`files.pvs-studio.com`** -- non-standard apt repo for the PVS-Studio
  static analyser, configured at the start of `pvs-studio.yml`

## Caching strategy

Four caches are configured across the workflows. The two vcpkg caches
are layered and complementary; the rest are independent.

| Cache                  | Configured in                              | Path                               | Key                                                                       |
| ---                    | ---                                        | ---                                | ---                                                                       |
| vcpkg source downloads | `actions/setup-vcpkg/action.yml`           | `vcpkg_downloads` (workspace)      | vcpkg short hash + manifest + overlay + `runner.os`                       |
| vcpkg binary outputs   | `actions/setup-vcpkg/action.yml`           | `vcpkg_bin_cache` (workspace)      | vcpkg short hash + manifest + overlay + compiler + ImageOS + ImageVersion |
| pip downloads          | `workflows/deploy-website.yml`             | `~/.cache/pip`                     | hash of `mkdocs-package-requirements.txt`                                 |
| apt packages           | `workflows/{linting,linux,pvs-studio}.yml` | (managed by cache-apt-pkgs-action) | package list + `version:` field                                           |

How the two vcpkg caches interact:

- **Binary cache hit** -- skip downloading and building artifacts entirely;
  this is the fast path

- **Binary cache miss + downloads cache hit** -- compile from already-downloaded
  sources; this is the resilience path against flaky vcpkg port download sites

- **Both miss** -- fetch package sources, compile them, then populate both caches;
  this is the slowest path and should only happen when updating the vcpkg
  baseline hash, the vcpkg overlays, or the runner images

The downloads cache is only written to when vcpkg actually fetches package
sources, i.e., on a binary-cache miss. As long as the binary cache hits (the
common case), `downloads/` stays empty on Linux/macOS. The cache download
cache will be only actually used after the next natural binary-cache miss
(e.g., runner image bump, compiler change, manifest edit).

Windows is the exception: its `downloads/` also stores vcpkg's bundled
toolchain (cmake, ninja, meson, etc.) and so populates on every cold build,
binary-cache miss or not.

Approximate sizes (zipped static libs + headers compress hard):

- **vcpkg binary cache** --- 10--50 MB per entry
- **vcpkg downloads cache** --- ~200 MB
- **pip cache** --- 10--50 MB
- **apt cache** --- 10--20 MB


### Retry wrappers (complement to caching)

We're also using a reasonable number of retries to recover from transient
flakes:

- GitHub release-asset downloads in `windows.yml`, `macos.yml`,
  `linux.yml`: `curl -fL --retry 5 --retry-all-errors --retry-delay 10`
  or the equivalent `wget --tries=5 --waitretry=10 --retry-connrefused`.

- `pip3 install --retries 5 --timeout 60` in `deploy-website.yml`.

- `apt-get update` / `apt-get install pvs-studio` in `pvs-studio.yml`,
  wrapped by the `./.github/actions/retry` composite action.


### Observability

- Each cache step echoes `<cache> hit: true|false` after restore so
  you can tell at a glance whether the slow path was taken.

- The release build jobs in `windows.yml`, `macos.yml`, and `linux.yml`
  end with a `gh cache list` step that prints repo-wide cache usage
  sorted largest first -- useful for spotting cache pressure ahead of
  the 10 GB per-repository actions cache quota.


## Cache invalidation

All keys are pure functions of their inputs:

- **vcpkg caches** -- bumping the baseline in `vcpkg.json`, editing
  `vcpkg-configuration.json`, or touching any file under `vcpkg-ports/**`
  invalidates both caches. The binary cache additionally invalidates on
  compiler, ImageOS, and ImageVersion changes.

- **pip cache** -- editing
  `extras/documentation/mkdocs-package-requirements.txt` invalidates it.

- **apt cache** -- editing the package list or
  `packages/ubuntu-22.04-apt.txt` invalidates it. Bumping the
  `version:` field on the `cache-apt-pkgs-action` step force-invalidates
  it.

