resource_dirs = [
  'translations',
  # insert new resource subdirectory names here
]

# This will install all the above resource_dirs into the
# platform-specific resources/ directory in the build area,
# which is then bundled into the release package.

# To add a new resources subdirectory, create a new directory
# under ./contrib/resources/<here>.
#  - Add files to it including non-deliverables
#  - Create a meson.build inside it.
#  - Make the file contain a single "resources" list of
#    just the filenames to be bundled in the output.
#    resources = [ 'file1', 'file2', ... ].
#  - See the translations directory for a working example.
#  - Finally, add the name of the directory to the above
#    resource_dirs list.

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
relative_resources = target_machine.system() == 'darwin' \
                     ? '../../../Resources' : '../../resources'

target_resources = meson.current_build_dir() / relative_resources

foreach source_dir : resource_dirs

  # Create the resources/source_dir in the build area
  target_dir = target_resources / source_dir
  configure_file(
    output:  source_dir,
    command: ['mkdir', '-p', target_dir])

  subdir(source_dir)

  # Install each of the source_dir's resource files
  foreach source_name : resources
    source_file = source_dir / source_name
    target_file = target_dir / source_name
    configure_file(
      output:  source_name,
      command: ['install', '-m', '644', files(source_file), target_file])
  endforeach

  # Setup the translation files in the system
  if get_option('prefix') in ['/usr', '/usr/local']
    system_resources = get_option('prefix') / 'share/dosbox-staging' / source_dir
    foreach source_name : resources
      install_data(target_dir / source_name,
                   install_dir : system_resources)
    endforeach
  endif

endforeach
