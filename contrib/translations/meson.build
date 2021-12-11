# Setup language translation files
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
translations_files = {
  'de/de_DE.lng'       : 'de.lng',
  'en/en_US.lng'       : 'en.lng',
  'es/es_ES.lng'       : 'es.lng',
  'fr/fr_FR.lng'       : 'fr.lng',
  'it/it_IT.lng'       : 'it.lng',
  'pl/pl_PL.lng'       : 'pl.lng',
  'pl/pl_PL.CP437.lng' : 'pl.cp437.lng',
  'ru/ru_RU.lng'       : 'ru.lng',
}

# Staging's downloable package includes bundled translations alone-side
# the binary (or relative to the binary on macOS in ../Resources/), which
# Staging tries to lookup when a language is provided.

# This section sets up the same relative translation bundle inside the
# build-dir so users who run and test from source builds also have
# langauge support.
#
# NOTE: Meson currently doesn't support creating directories in the
#       build-dir nor does it support trivially installing or copying
#       source files into build-dir tree. Therefore we leverage the
#       configure_file() function with 'mkdir' and 'install' calls to
#       do this as best we can, platform-aware, until a more elegant
#       approach exists. See:
#         - https://github.com/mesonbuild/meson/issues/860
#         - https://github.com/mesonbuild/meson/issues/7067
#
builddir_translations = meson.current_build_dir() / '../..' / \
                        (host_machine.system() == 'darwin'
                           ? '../Resources/translations'
                           : 'translations')

configure_file(
  output:  'translations', # not used, information-only
  command: ['mkdir', '-p', builddir_translations])

install_cmd = ['install', '-m', '644']

foreach source_file, target_name : translations_files
  target_file = builddir_translations / target_name
  configure_file(
    output:  target_name, # not used, information-only
    command: install_cmd + [ files(source_file), target_file])
endforeach

# Setup the translation files in the system
if get_option('prefix') in ['/usr', '/usr/local']
  system_translations = get_option('prefix') / 'share/dosbox-staging/translations'
  foreach source_file, target_name : translations_files
    install_data(builddir_translations / target_name,
                 install_dir : system_translations)
  endforeach
endif
