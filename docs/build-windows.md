# Building on Windows

Windows builds are created with CMake and Visual Studio, using the Clang/LLVM
compiler and vcpkg to provide the library dependencies. This is the
fully-supported toolchain used to create the official release builds.

We recommend using CMake with presets because they're CI-tested and produce a
binary using consistent compiler flags. Run `cmake --list-presets` to list the
presets.

**Visual Studio 18 2026** is the recommended version. Visual Studio 17 2022 is
also supported for the time being via the dedicated `*-vs2022` presets (e.g.
`debug-windows-vs2022`, `release-windows-vs2022`).


## Installing prerequisites

1. Install Visual Studio Community 2026 (or 2022):
   <https://visualstudio.microsoft.com/vs/community/>
    - Select **"C++ Clang tools for Windows"** in the Visual Studio installer.

2. Install Python 3 (only needed if you want to build the
   [offline documentation](#offline-documentation)). Download the installer
   from <https://www.python.org/downloads/> and make sure to check **"Add
   Python to PATH"** during installation.


## Installing vcpkg

The library dependencies are built and provided by
[vcpkg](https://github.com/microsoft/vcpkg). The CMake presets expect the
`VCPKG_ROOT` environment variable to point at your vcpkg checkout.

### Standalone vcpkg (recommended)

This matches the toolchain used by our CI.

1. Clone vcpkg (for example into `C:\vcpkg`):

    ``` shell
    git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
    ```

2. Bootstrap it:

    ``` shell
    cd C:\vcpkg
    .\bootstrap-vcpkg.bat -disableMetrics
    ```

3. Set the `VCPKG_ROOT` environment variable so the CMake presets can find
   vcpkg. This sets it permanently for your user account; restart your shell or
   Visual Studio afterwards:

    ``` shell
    setx VCPKG_ROOT C:\vcpkg
    ```

### Visual Studio's bundled vcpkg (alternative)

Recent versions of Visual Studio already include vcpkg, but it needs to be
initialized. Open a Visual Studio Developer Command Prompt and run:

``` shell
vcpkg integrate install
```

You still need to set `VCPKG_ROOT` to the bundled copy's location so the CMake
presets can find it. Existing
[standalone versions](https://github.com/Microsoft/vcpkg#quick-start-windows)
work as well.


## Building from the command line

From the repository root, select the preset matching your Visual Studio version
(the Visual Studio Developer Command Prompt is a convenient shell that already
has the tooling on its `PATH`).

Visual Studio 2026:

``` shell
cmake --preset release-windows
cmake --build --preset release-windows
```

Visual Studio 2022:

``` shell
cmake --preset release-windows-vs2022
cmake --build --preset release-windows-vs2022
```

Use the `debug-windows` / `debug-windows-vs2022` presets for debug builds.

> [!NOTE]
> The first configure step builds all library dependencies from source via
> vcpkg, which can take a significant length of time. Subsequent builds reuse
> the cached artifacts.


## Building in the Visual Studio IDE

1. Remove any existing DOSBox Staging projects from Visual Studio.

2. Exit Visual Studio, then delete the `.vs` folder from your local copy of
   the DOSBox Staging repository if it exists.

3. Start Visual Studio, then import your local DOSBox Staging repository
   folder by selecting the **Open a local folder** item in the startup screen,
   or by selecting **File / Open / Folder** in the top menu.

4. You'll be greeted by a CMake splash screen, then after the CMake project
   setup has been completed, you'll see the list of available CMake
   configurations in the second dropdown below the menu bar, next to **Local
   Machine**.

5. Select a configuration matching your Visual Studio version —
   **debug-windows** or **release-windows** on Visual Studio 2026, or
   **debug-windows-vs2022** / **release-windows-vs2022** on Visual Studio 2022
   — then select **Build / Build All** or press **F7** to build the project.
   You'll need to do this twice for the first time, and probably after changing
   the `CMakeLists.txt` files.

## Offline documentation

Self-contained offline HTML documentation can optionally be built as part of
the CMake build. The output appears in the build directory at
`build\<preset>\Resources\docs\` — this is the same documentation bundled with
the release packages.

Documentation building is **off by default**. Use the **manual** CMake presets
to enable it:

- `debug-windows-manual`
- `release-windows-manual`

These presets appear in Visual Studio's configuration dropdown after importing
the CMake project. From the command line:

```shell
cmake --preset release-windows-manual
cmake --build --preset release-windows-manual
```

To rebuild just the documentation after editing content:

```shell
cmake --build --preset release-windows-manual --target rebuild_documentation
```

Alternatively, you can enable documentation building with any preset by passing
`-DOPT_DOCUMENTATION=ON` on the command line:

```shell
cmake --preset release-windows -DOPT_DOCUMENTATION=ON
```

### Best-effort

Offline documentation building requires Python 3 with pip to be installed and
available on the system `PATH`. If Python is not available or is missing
required modules (`venv`, `ensurepip`), the build proceeds normally without
documentation — a warning is shown during CMake configuration, but the build is
**never aborted**.

To install Python, download the installer from <https://www.python.org/downloads/>
and make sure to check **"Add Python to PATH"** during installation. The
default installer options include pip; do not uncheck it.

> **Note**
>
> The Python included with Visual Studio's "Python development" workload may
> work, but it is not guaranteed to be on the system `PATH`. A standalone
> Python installation is recommended.

### Caching

There are two independent cache layers that make successive builds fast:

1. **Python venv and pip packages** — stored in the build directory at
   `_mkdocs_venv\`. The virtual environment is created once per build directory.
   pip only re-runs when `extras\documentation\mkdocs-package-requirements.txt`
   is modified.

2. **Downloaded external assets** — the mkdocs-material privacy plugin caches
   downloaded web fonts, images, and scripts in `website\.cache\` in the source
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
> errors, set the `SSL_CERT_FILE` environment variable to your
> organisation's CA bundle path.

### Rebuilding after documentation changes

Changes to markdown files under `website\docs\` do not automatically trigger a
rebuild — globbing hundreds of files into CMake's dependency tracking would be
impractical. To rebuild after editing documentation content:

```shell
# Option 1: Use the dedicated rebuild target
cmake --build --preset release-windows --target rebuild_documentation

# Option 2: Invalidate the build stamp (triggers rebuild on next normal build)
# (run from the Git Bash shell or WSL)
touch website/mkdocs.yml
```

### Forcing a full rebuild

To rebuild documentation from scratch, delete the build stamp from the build
directory:

```shell
del build\release-windows\_mkdocs_build_stamp
```

### Cleaning all documentation caches

To remove all MkDocs caches from the source tree (`website\.cache`,
`website\__pycache__`, `website\site`):

```shell
cmake --build --preset release-windows --target clean-manual
```


## Bisecting and building old versions

Versions prior to 0.83.0 used the Meson build system. See the
[Meson build guide](build-meson.md) for instructions on building older
checkouts.


## Troubleshooting

### Random build failures

Try using **Clean All** from the **Build** menu, then **Rescan Solution**
from the **Project** menu. In most cases, **Build All** should work without
errors after this.

If you're still getting errors, or some of these menu items are not available,
delete the DOSBox Staging project in Visual Studio, delete the `.vs` folder
inside the local repo folder, then follow the instructions in
**Importing the CMake project** to reimport the project.

### CMake configurations have disappeared

If the CMake configurations have disappeared from the toolbar after updating
Visual Studio or the local DOSBox Staging repository, try **Rescan Solution**
from the **Project** menu.

If that doesn't help, delete the DOSBox Staging project in Visual Studio,
delete the `.vs` folder inside the local repo folder, then follow the
instructions in **Importing the CMake project** to reimport the project.

