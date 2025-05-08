resources = [
    'drives',
    'disknoise',
    'freedos-cpi',
    'freedos-keyboard',
    'glshaders',
    'mapping',
    'mapping-freedos.org',
    'mapping-unicode.org',
    'mapping-wikipedia.org',
    'translations',
    'mapperfiles',
    # insert new resource subdirectory names here
]

# This will install all the above resources into the
# platform-specific resources/ directory in the build area,
# which is then bundled into the release package.

# When "meson install" is used, the same tree is installed
# under the provided data_dir (or prefix / share) area.

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
#    'resources' list at the top of this file.

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

# Top-level resources directory for the build and installation area
target_build_dir = meson.current_build_dir() / relative_resources
target_install_dir = get_option('datadir') / meson.project_name()

foreach resource : resources

    # Create the resources/resource in the build area
    configure_file(
        output: resource,
        command: ['mkdir', '-p', target_build_dir / resource],
    )

    # These two variables get re-populated inside each subdir(..)
    resource_files = []
    resource_dirs = []
    subdir(resource)

    # Install each of the resource's files
    foreach filename : resource_files
        source_file = resource / filename
        configure_file(
            output: filename,
            command: [
                'install',
                '-m', '644',
                files(source_file),
                target_build_dir / resource / filename,
            ],
        )
        install_data(
            source_file,
            install_dir: target_install_dir / resource,
        )
    endforeach

    # Get this resource's full path
    source_dir_cmd = run_command(
        'dirname',
        files(resource / 'meson.build'),
        check: true,
    )
    resource_absolute_dir = source_dir_cmd.stdout().strip()

    # Install each of the resource's dirs
    foreach dirname : resource_dirs
        configure_file(
            output: dirname,
            command: [
                'cp',
                '-f',
                '-R',
                '-P', resource_absolute_dir / dirname,
                target_build_dir / resource,
            ],
        )
        install_subdir(
            resource / dirname,
            install_dir: target_install_dir / resource,
        )
    endforeach
endforeach
