# Building the offline documentation

Self-contained offline HTML documentation can optionally be built as part of the
CMake build. The output appears in the build directory at
`<build-dir>/Resources/docs/` (macOS and Windows) or
`<build-dir>/resources/docs/` (Linux) — this is the same documentation bundled
with the release packages.

Documentation building is **off by default**. To enable it, pass
`-DOPT_DOCUMENTATION=ON` when configuring, using your platform's preset:

```shell
cmake --preset=<preset> -DOPT_DOCUMENTATION=ON
cmake --build --preset=<preset>
```

On Windows you can also use the dedicated **manual** presets
(`debug-windows-manual`, `release-windows-manual`, and their `-vs2022`
variants), which enable documentation building without the extra flag.

To rebuild just the documentation after editing content:

```shell
cmake --build --preset=<preset> --target rebuild_documentation
```


## Prerequisites

Python 3 with the `venv` module is required. The build automatically creates a
Python virtual environment in the build directory and installs all MkDocs
dependencies into it — no other manual setup is needed.

- **Linux** — on Debian and Ubuntu, the `venv` module ships in a separate
  package that may not be installed by default:

    ```shell
    sudo apt-get install python3-venv
    ```

- **macOS** — Python 3 with `venv` ships with the Xcode Command Line Tools and
  Homebrew; no extra packages are needed.

- **Windows** — install Python 3 from <https://www.python.org/downloads/> and
  make sure to check **"Add Python to PATH"** during installation. The default
  installer options include pip; do not uncheck it. The Python bundled with
  Visual Studio's "Python development" workload may work, but it is not
  guaranteed to be on the system `PATH` — a standalone installation is
  recommended.


## Best-effort

If Python is not available or is missing required modules (`venv`, `ensurepip`),
the build proceeds normally without documentation — a warning is shown during
CMake configuration, but the build is **never aborted**.


## Caching

There are two independent cache layers that make successive builds fast:

1. **Python venv and pip packages** — stored in the build directory at
   `_mkdocs_venv/`. The virtual environment is created once per build directory.
   pip only re-runs when `extras/documentation/mkdocs-package-requirements.txt`
   is modified.

2. **Downloaded external assets** — the mkdocs-material privacy plugin caches
   downloaded web fonts, images, and scripts in `website/.cache/` in the source
   tree (this directory is git-ignored). Because it lives outside the build
   directory, it persists across clean builds and across different build
   configurations (debug, release, etc.).

> [!IMPORTANT]
> The privacy plugin only downloads assets from a small set of trusted
> sources: **Google Fonts** (fonts.googleapis.com, fonts.gstatic.com),
> **www.dosbox-staging.org** (our GitHub Pages website, completely under our
> control), and a few well-known CDNs used by the MkDocs Material theme
> (cdn.jsdelivr.net, unpkg.com, mirrors.creativecommons.org). No content from
> untrusted origins is ever fetched. The build uses the system CA certificate
> bundle instead of Python's built-in certifi bundle, so VPNs that perform
> SSL inspection work without issues. If the build fails with certificate
> errors, set the `SSL_CERT_FILE` environment variable to your organisation's
> CA bundle path.


## Rebuilding after documentation changes

Changes to markdown files under `website/docs/` do not automatically trigger a
rebuild — globbing hundreds of files into CMake's dependency tracking would be
impractical. To rebuild after editing documentation content:

```shell
# Option 1: Use the dedicated rebuild target
cmake --build --preset=<preset> --target rebuild_documentation

# Option 2: Invalidate the build stamp (triggers rebuild on next normal build)
touch website/mkdocs.yml
```

On Windows, run the `touch` command from the Git Bash shell or WSL.


## Forcing a full rebuild

To rebuild documentation from scratch, delete the build stamp from the build
directory:

```shell
rm build/<preset>/_mkdocs_build_stamp
```

On Windows: `del build\<preset>\_mkdocs_build_stamp`.


## Cleaning all documentation caches

To remove all MkDocs caches from the source tree (`website/.cache`,
`website/__pycache__`, `website/site`):

```shell
cmake --build --preset=<preset> --target clean-manual
```
