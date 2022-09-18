resource_dirs = [
    'drives',
    'freedos-cpi',
    'freedos-keyboard',
    'glshaders',
    'translated-0.79-conf-files',
    'translations',
    # insert new resource subdirectory names here
]

# This will install all the above resource_dirs into the
# platform-specific resources/ directory in the build area,
# which is then bundled into the release package.

# To add a new resources subdirectory, create a new directory
# under ./contrib/resources/<here>.
#  - Add files to it including non-deliverables
#  - Create a meson.build inside it containing a
#    "resource_files = []" and/or "resource_dirs = []" list
#   containing either filenames and/or directories to be bundled.
#    For example:
#    resource_files = [ 'file1', 'file2', ... ]
#    resource_dirs = [ 'dir1', 'dir2', ... ]
#  - See the translations directory for a working example.
#  - Finally, add the name of the directory to the above
#    resource_dirs list at the top of this file.

# ---- no further changes are needed to add new resources ----

# NOTE: Meson currently doesn't support creating directories in the
#       build-dir nor does it support trivially installing or copying
#       source files into build-dir tree. Therefore we leverage the
#       configure_file() function with 'mkdir' and 'install' calls to
#       do this as best we can, platform-aware, until a more elegant
#       approach exists. See:
#         - https://github.com/mesonbuild/meson/issues/860
#         - https://github.com/mesonbuild/meson/issues/7067
#
relative_resources = (
    target_machine.system() == 'darwin' ? '../../../Resources' : '../../resources'
)

target_resources = meson.current_build_dir() / relative_resources

foreach source_dir : resource_dirs

    # Create the resources/source_dir in the build area
    target_dir = target_resources / source_dir
    configure_file(output: source_dir, command: ['mkdir', '-p', target_dir])

    # These two variables get re-populated inside each subdir(..)
    resource_files = []
    resource_dirs = []

    subdir(source_dir)

    # Install each of the source_dir's resource files
    foreach resource : resource_files
        source_file = source_dir / resource
        target_file = target_dir / resource
        configure_file(
            output: resource,
            command: ['install', '-m', '644', files(source_file), target_file],
        )
    endforeach

    # Get this source_dir's full path
    source_dir_cmd = run_command(
        'dirname',
        files(source_dir / 'meson.build'),
        check: true,
    )
    source_dir_path = source_dir_cmd.stdout().strip()

    # Install each of the source_dir's resource dirs
    foreach resource : resource_dirs
        configure_file(
            output: resource,
            command: ['cp', '-f', '-R', '-P', source_dir_path / resource, target_dir],
        )
    endforeach

    # Setup the the source_dir's resource files and dirs into the system (if requested)
    if get_option('prefix') in ['/usr', '/usr/local']
        system_resources = (
            get_option('prefix') / 'share/dosbox-staging' / source_dir
        )
        foreach resource : resource_files
            install_data(
                target_dir / resource,
                install_dir: system_resources,
            )
        endforeach

        foreach resource : resource_dirs
            install_subdir(
                target_dir / resource,
                install_dir: system_resources,
            )
        endforeach
    endif

endforeach
