#
# Build offline documentation using MkDocs
# ========================================
#
# This module builds self-contained offline HTML documentation from the MkDocs
# sources in website/ and places the output into the build tree's resource
# directory at '<RESOURCE_COPY_PATH>/docs/'. The packaging scripts
# (create-package.sh) already copy the entire resources directory, so the
# built docs are automatically included in release packages.
#
# Target dependency chain
# -----------------------
#
#   dosbox  -->  copy_assets  -->  build_documentation
#
# The 'build_documentation' target is an 'ALL' target, so it runs as part of
# every normal build. The 'copy_assets' target (which 'dosbox' depends on) is
# made to depend on build_documentation, ensuring documentation is built
# before assets are finalized.
#
# Three-stage stamp file pipeline
# --------------------------------
#
# To avoid redundant work on successive builds, this module uses a chain of
# three 'add_custom_command' steps, each producing a stamp file that serves as
# a dependency for the next stage:
#
#   Stage 1: Create Python virtual environment
#            Output: _mkdocs_venv/pyvenv.cfg  (natural venv marker)
#            Runs:   Only once per build directory
#
#   Stage 2: Install MkDocs packages via pip
#            Output: _mkdocs_venv/.pip_install_stamp
#            Deps:   Stage 1 + mkdocs-package-requirements.txt
#            Runs:   Only when requirements.txt changes
#
#   Stage 3: Build offline documentation with mkdocs
#            Output: _mkdocs_build_stamp
#            Deps:   Stage 2 + website/mkdocs.yml
#            Runs:   Only when mkdocs.yml changes or stamp is missing
#
# Caching
# -------
#
# There are two independent cache layers:
#
#   1. The Python venv and installed pip packages live in the build directory
#      at '_mkdocs_venv/'. They persist across builds and are only recreated
#      when the build directory is deleted. pip re-runs only when
#      'extras/documentation/mkdocs-package-requirements.txt' is modified.
#
#   2. The mkdocs-material 'privacy' and 'offline' plugins cache downloaded
#      external assets (web fonts, images, JavaScript) in 'website/.cache/' in
#      the SOURCE tree. This directory is git-ignored. Because it lives
#      outside the build tree, it persists across clean builds and across
#      different build configurations (debug, release, etc.). No special
#      configuration is needed;  mkdocs-material manages this cache
#      automatically.
#
# Rebuilding after documentation changes
# --------------------------------------
#
# Changes to markdown files under 'website/docs/' do NOT automatically trigger
# a rebuild. Globbing hundreds of markdown files into CMake's dependency
# tracking would be fragile and impractical. Instead:
#
#   - Use the 'rebuild_documentation' target to force a rebuild:

#       cmake --build --preset <preset> --target rebuild_documentation
#
#   - Or touch 'website/mkdocs.yml' to invalidate the build stamp:

#       touch website/mkdocs.yml
#
# MkDocs handles its own incrementality internally, so forced re-runs are fast
# when only a few files have changed.
#
# Opt-in and graceful degradation
# --------------------------------
#
# Documentation building is OFF by default. Pass '-DOPT_DOCUMENTATION=ON'
# to enable it. If enabled but Python 3 is not found or the venv module is
# not available, this module emits a warning and returns early. The build
# proceeds normally without documentation; it is never aborted.
#

function(add_build_documentation)

  # ---------------------------------------------------------------------------
  # Find Python
  # ---------------------------------------------------------------------------
  # On Linux and macOS, the interpreter is typically called "python3".
  # On Windows, it is usually just "python" (the Python installer and the
  # Microsoft Store both register it under that name).

  find_program(PYTHON_EXECUTABLE NAMES python3 python)

  if (NOT PYTHON_EXECUTABLE)
    message(WARNING "\n"
      "  ==========================================================\n"
      "  Python not found; offline documentation will not be built.\n"
      "  Install Python 3 to enable documentation building.\n"
      "  ==========================================================")
    return()
  endif()

  # ---------------------------------------------------------------------------
  # Verify the venv module is available
  # ---------------------------------------------------------------------------
  # On Debian and Ubuntu, the 'venv' module is shipped in a separate
  # 'python3-venv' package that is not always installed by default. Without
  # it, 'python -m venv' fails. We check at configure time, so the user gets a
  # clear message rather than a cryptic build-time error.

  execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -c "import venv"
    RESULT_VARIABLE VENV_CHECK_RESULT
    OUTPUT_QUIET ERROR_QUIET
  )

  if (NOT VENV_CHECK_RESULT EQUAL 0)
    message(WARNING "\n"
      "  ==========================================================\n"
      "  Python 'venv' module not available; offline documentation\n"
      "  will not be built. Install 'python3-venv' (Debian/Ubuntu)\n"
      "  to enable documentation building.\n"
      "  ==========================================================")
    return()
  endif()

  # ---------------------------------------------------------------------------
  # Verify the ensurepip module is functional
  # ---------------------------------------------------------------------------
  # The 'venv' module uses 'ensurepip' to bootstrap pip into new virtual
  # environments. On Windows, Python installed without pip leaves the
  # 'ensurepip' module importable but non-functional (the bundled pip wheel
  # is missing). Simply 'import ensurepip' does not catch this; calling
  # ensurepip.version() exercises the full wheel discovery path on Python
  # 3.14+ and fails when the bundled pip wheel is absent.

  execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -c "import ensurepip; ensurepip.version()"
    RESULT_VARIABLE ENSUREPIP_CHECK_RESULT
    OUTPUT_QUIET ERROR_QUIET
  )

  if (NOT ENSUREPIP_CHECK_RESULT EQUAL 0)
    message(WARNING "\n"
      "  ==========================================================\n"
      "  Python 'ensurepip' module not available; offline\n"
      "  documentation will not be built. Reinstall Python 3 with\n"
      "  pip enabled to enable documentation building.\n"
      "  ==========================================================")
    return()
  endif()

  message(STATUS "Found Python for documentation: ${PYTHON_EXECUTABLE}")

  # ---------------------------------------------------------------------------
  # Paths
  # ---------------------------------------------------------------------------

  set(MKDOCS_VENV_DIR      "${CMAKE_BINARY_DIR}/_mkdocs_venv")
  set(MKDOCS_REQUIREMENTS  "${CMAKE_SOURCE_DIR}/extras/documentation/mkdocs-package-requirements.txt")
  set(MKDOCS_CONFIG        "${CMAKE_SOURCE_DIR}/website/mkdocs.yml")
  set(MKDOCS_OUTPUT_DIR    "${CMAKE_CURRENT_BINARY_DIR}/${RESOURCE_COPY_PATH}/docs")
  set(MKDOCS_PIP_STAMP     "${MKDOCS_VENV_DIR}/.pip_install_stamp")
  set(MKDOCS_BUILD_STAMP   "${CMAKE_BINARY_DIR}/_mkdocs_build_stamp")

  # The venv places executables in bin/ on Unix and Scripts/ on Windows.
  if (WIN32)
    set(MKDOCS_VENV_BIN "${MKDOCS_VENV_DIR}/Scripts")
  else()
    set(MKDOCS_VENV_BIN "${MKDOCS_VENV_DIR}/bin")
  endif()

  set(MKDOCS_PIP "${MKDOCS_VENV_BIN}/pip")
  set(MKDOCS_EXE "${MKDOCS_VENV_BIN}/mkdocs")

  # ---------------------------------------------------------------------------
  # Stage 1: Create the Python virtual environment
  # ---------------------------------------------------------------------------
  # The 'pyvenv.cfg' file is always created by 'python -m venv', so it serves
  # as a natural output marker. This command runs only once per build
  # directory; subsequent builds see 'pyvenv.cfg' already exists and skip this
  # step entirely.

  add_custom_command(
    OUTPUT  "${MKDOCS_VENV_DIR}/pyvenv.cfg"

    COMMAND "${PYTHON_EXECUTABLE}" -m venv "${MKDOCS_VENV_DIR}"
    COMMENT "Creating Python virtual environment for MkDocs"
  )

  # ---------------------------------------------------------------------------
  # Stage 2: Install MkDocs and its plugins into the virtual environment
  # ---------------------------------------------------------------------------
  # This stage depends on both the 'venv' (stage 1) and the requirements file.
  # If the requirements file is edited (e.g., a plugin version is bumped),
  # this step re-runs on the next build. The '--quiet' flag suppresses pip's
  # verbose output during normal builds.
  #
  # A stamp file is used because pip's output is not a single predictable
  # file; the 'touch' command creates the stamp after a successful install.

  add_custom_command(
    OUTPUT  "${MKDOCS_PIP_STAMP}"

    DEPENDS "${MKDOCS_VENV_DIR}/pyvenv.cfg"
            "${MKDOCS_REQUIREMENTS}"

    COMMAND "${MKDOCS_PIP}" install --quiet -r "${MKDOCS_REQUIREMENTS}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${MKDOCS_PIP_STAMP}"

    COMMENT "Installing MkDocs packages into virtual environment"
  )

  # ---------------------------------------------------------------------------
  # Stage 3: Build the offline documentation
  # ---------------------------------------------------------------------------
  # The 'OFFLINE=true' environment variable activates the offline and privacy
  # plugins in 'website/mkdocs.yml'. These plugins download external assets
  # (fonts, scripts, images) and embed them into the output, producing fully
  # self-contained HTML documentation.
  #
  # 'cmake -E env' is used to set the environment variable because it works
  # identically on Linux, macOS, and Windows.
  #
  # 'WORKING_DIRECTORY' is set to the 'website/' directory so that mkdocs
  # plugins (privacy, offline) place their '.cache/' directory inside
  # 'website/.cache/' rather than at the repository root.
  #
  # The build stamp depends on 'mkdocs.yml' so that configuration changes
  # trigger a rebuild. It intentionally does NOT depend on individual markdown
  # files under 'website/docs/'; globbing hundreds of files into CMake's
  # dependency tracking would be fragile and impractical. Use the
  # rebuild_documentation target (below) or 'touch website/mkdocs.yml' to
  # trigger a rebuild after editing documentation content.

  add_custom_command(
    OUTPUT  "${MKDOCS_BUILD_STAMP}"

    DEPENDS "${MKDOCS_PIP_STAMP}"
            "${MKDOCS_CONFIG}"

    COMMAND "${CMAKE_COMMAND}" -E env "OFFLINE=true"
            "${MKDOCS_EXE}" build
            --config-file "${MKDOCS_CONFIG}"
            --site-dir "${MKDOCS_OUTPUT_DIR}"

    COMMAND "${CMAKE_COMMAND}" -E remove
            "${MKDOCS_OUTPUT_DIR}/404.html"
            "${MKDOCS_OUTPUT_DIR}/sitemap.xml"
            "${MKDOCS_OUTPUT_DIR}/sitemap.xml.gz"

    COMMAND "${CMAKE_COMMAND}" -E touch "${MKDOCS_BUILD_STAMP}"

    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/website"
    COMMENT "Building offline documentation with MkDocs"
  )

  # ---------------------------------------------------------------------------
  # Main target: build_documentation (ALL)
  # ---------------------------------------------------------------------------
  # This target is part of the default build (ALL) and is wired into the
  # existing target chain: dosbox -> copy_assets -> build_documentation.

  add_custom_target(build_documentation ALL
    DEPENDS "${MKDOCS_BUILD_STAMP}"
  )
  add_dependencies(copy_assets build_documentation)

  # ---------------------------------------------------------------------------
  # Convenience target: rebuild_documentation (not ALL)
  # ---------------------------------------------------------------------------
  # Unlike build_documentation, this target always re-runs mkdocs regardless
  # of stamp file state. Use it after editing markdown files under
  # 'website/docs/':
  #
  #   cmake --build --preset <preset> --target rebuild_documentation
  #
  # This target depends on build_documentation (via add_dependencies) to
  # ensure the venv and pip packages are set up before running mkdocs. It does
  # NOT use 'DEPENDS' on the stamp files directly because the Xcode "new build
  # system" does not allow custom command outputs to be consumed by multiple
  # independent targets.

  add_custom_target(rebuild_documentation
    COMMAND "${CMAKE_COMMAND}" -E env "OFFLINE=true"
            "${MKDOCS_EXE}" build
            --config-file "${MKDOCS_CONFIG}"
            --site-dir "${MKDOCS_OUTPUT_DIR}"

    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/website"

    COMMENT "Rebuilding offline documentation with MkDocs"
  )
  add_dependencies(rebuild_documentation build_documentation)

endfunction()
