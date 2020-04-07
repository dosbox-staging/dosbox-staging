# Development Tools

An inventory of tools used by the dosbox-staging team and community.

- [Development Tools](#development-tools)
	- [Versioning](#versioning)
	- [Editing](#editing)
	- [Formatting](#formatting)
	- [Building](#building)
	- [Analyzing](#analyzing)
	- [Visualizing](#visualizing)
	- [Optimizing](#optimizing)
	- [Debugging](#debugging)
	- [Package Managers](#package-managers)
	- [Operating Sytems](#operating-sytems)
	- [Platforms](#platforms)


## Versioning

| Name | Icon | Role | Refs  |
|---|---|---|---|
| Git  | <img src="https://user-images.githubusercontent.com/1557255/78574678-304d3500-77df-11ea-8961-d6d1197936cc.png" height="27">  | Eases branching, merging, and code management among distributed team |   |
| GitHub  | <img src="https://github.githubassets.com/images/modules/logos_page/GitHub-Mark.png" height="27">  | Allows team and community to collaborate within the repo  |   |
|  Gitlab | <img src="https://about.gitlab.com/images/press/logo/png/gitlab-icon-rgb.png" height="27">  | Holds tests data and regression tests   |   |

## Editing

| Name | Icon | Role | Refs  |
|---|---|---|---|
| vi, vim | <img src="https://upload.wikimedia.org/wikipedia/commons/9/9f/Vimlogo.svg" height="27">  |  Thorough CLI text editor  |  |
| nano  | <img src="https://upload.wikimedia.org/wikipedia/commons/d/d9/Nano_text_editor.png" height="27">  | Basic CLI text editor |   |
| VS Code  | <img src="https://upload.wikimedia.org/wikipedia/commons/2/2d/Visual_Studio_Code_1.18_icon.svg" height="27">  | Power IDE editor |   |

## Formatting

| Name | Icon | Role | Refs  |
|---|---|---|---|
|Clang-format | <img src="https://llvm.org/img/DragonMedium.png" height="27">  | Formats changes per guidelines  |   |

## Building

| Name | Icon | Role | Refs  |
|---|---|---|---|
| GCC  | <img src="https://upload.wikimedia.org/wikipedia/commons/a/af/GNU_Compiler_Collection_logo.svg" height="27"> | Builds the source on all platforms; release builds on Linux |   |
| Clang  | <img src="https://llvm.org/img/DragonMedium.png" height="27">  | Builds the source on all platforms; release builds on macOS  |   |
| MSVC | <img src="https://visualstudio.microsoft.com/wp-content/uploads/2019/02/VSWinIcon_100x.png" height="27">  | Release builds on Windows  |   |

## Analyzing

| Name | Icon | Role | Refs  |
|---|---|---|---|
| pylint  | <img src="https://www.pylint.org/pylint.svg" height="27"> | Ensures python scripts conform to best practices |   |
| markdownlint  |  | Ensures Markdown files conform to best practices  |   |
| shellcheck  |   | Ensures bash scripts conform to best practices  |   |
| Clang Analyzer   | <img src="https://llvm.org/img/DragonMedium.png" height="27"> | Finds bugs in C, C++ through static analysis  |  |
| Coverity  | <img src="https://cdn.freebiesupply.com/logos/large/2x/coverity-logo-svg-vector.svg" height="27">  | Finds bugs in C, C++ through static analysis   |    |
| PVS-Studio  | <img src="https://import.viva64.com/docx/blog/0234_PVS-Studio_and_open_source_software_2/image1.png" height="27">  | Finds bugs in C, C++ through static analysis  |   |
| GCC sanitizers  | <img src="https://upload.wikimedia.org/wikipedia/commons/a/af/GNU_Compiler_Collection_logo.svg" height="27">  | Finds bugs in C, C++ during runtime |   |
| Clang sanitizers  | <img src="https://llvm.org/img/DragonMedium.png" height="27">   | Finds bugs in C, C++ during runtime  |   |

## Visualizing

| Name | Icon | Role | Refs  |
|---|---|---|---|
| CppDepend  | <img src="https://www.cppdepend.com/images/cppdependlogo.png" height="27">  | Explore code flows and inter-relationships  |   |

## Optimizing

| Name | Role | Refs  |
|---|---|---|
| pmu-tools  | Captures performance samples from the Linux kernel  |   |
| autofdo  | Converts Linux kernel performance samples to GCC and Clang-specific feedback inputs |   |

## Debugging

| Name | Icon | Role | Refs  |
|---|---|---|---|
| gdb  | <img src="https://www.gnu.org/software/gdb/images/archer.svg" height="27">  | Investigates runtime bugs by setting break points, showing variables, and printing backtraces  |   |

## Package Managers

| Name  | Role | Refs  |
|---|---|---|
| dnf   | Installs build and runtime dependencies on Redhat-derived OSes  |   |
| apt   | Installs build and runtime dependencies on Ubuntu-derived OSes  |   |
| vcpkg    | Install build and runtime dependencies on Windows  |   |
| msys2-pacman    | Install build and runtime dependencies for GCC and Clang on Windows |   |

## Operating Sytems

| Name | Icon | Role | Refs  |
|---|---|---|---|
| Fedora Linux  | <img src="https://upload.wikimedia.org/wikipedia/commons/3/3f/Fedora_logo.svg" height="27">  | Linux distribution sponsored primarily by Red Hat |   |
| Ubuntu Linux  | <img src="https://upload.wikimedia.org/wikipedia/commons/5/54/Ubuntu-Logo_ohne_Schriftzug.svg" height="27">  | Popular Linux distribution funded by Canonical  |   |
| Windows  | <img src="https://upload.wikimedia.org/wikipedia/commons/5/5f/Windows_logo_-_2012.svg" height="27">  | Microsoft's flagship operating system |   |
| macOS  | <img src="https://upload.wikimedia.org/wikipedia/commons/1/1b/Apple_logo_grey.svg" height="27">  | Apple's desktop OS X operating system  |   |
| VirtualBox  |  <img src="https://www.vectorlogo.zone/logos/virtualbox/virtualbox-icon.svg" height="27"> | Virtual Machine Engine software to test the project within VM-hosted OSes  |   |
| GitHub-CI  | <img src="https://github.githubassets.com/images/modules/site/features/actions-live-logs.svg" height="27">   | Github's backend Virtual Machine environment under which workflows run |   |

## Platforms

| Name | Icon | Role | Refs  |
|---|---|---|---|
| x86-64 | <img src="https://upload.wikimedia.org/wikipedia/en/1/1a/AMD64_Logo.svg" height="27"> | Primary development and test platform  |   |
| Raspberry Pi  | <img src="https://elinux.org/images/c/cb/Raspberry_Pi_Logo.svg" height="27">  | Runtime test environment for ARM and OpenGLES |   |
| i686 | <img src="https://upload.wikimedia.org/wikipedia/commons/c/c9/Intel-logo.svg" height="27">  |  Runtime test environment for older OSes such as Windows XP  |   |
| PPC-32 | <img src="https://upload.wikimedia.org/wikipedia/commons/b/bc/PowerPC_logo.svg" height="27">  | Runtime test environment for big-endian branches  |   |