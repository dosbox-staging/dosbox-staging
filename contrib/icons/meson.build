icons_hicolor = data_dir / 'icons' / 'hicolor'

# small pre-rendered raster icons
#
foreach size : ['16x16', '22x22', '24x24', '32x32']
  install_data('small-png' / size / 'dosbox-staging.png',
               install_dir : icons_hicolor / size / 'apps')
endforeach

# scalable vector icon
#
install_data('dosbox-staging.svg',
             install_dir : icons_hicolor / 'scalable' / 'apps')
