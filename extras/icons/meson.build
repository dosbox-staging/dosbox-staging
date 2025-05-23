icons_hicolor = data_dir / 'icons' / 'hicolor'

png_files = {
    'icon_16.png': '16x16',
    'icon_22.png': '22x22',
    'icon_24.png': '24x24',
    'icon_32.png': '32x32',
    'icon_48.png': '48x48',
    'icon_96.png': '96x96',
    'icon_128.png': '128x128',
    'icon_256.png': '256x256',
    'icon_512.png': '512x512',
    'icon_1024.png': '1024x1024',
}

# small bitmap icons
#

foreach name, size : png_files
    install_data(
        'png' / name,
        rename: 'org.dosbox_staging.dosbox_staging.png',
        install_dir: icons_hicolor / size / 'apps',
    )
endforeach

# scalable vector icon
#
install_data(
    'svg' / 'dosbox-staging.svg',
    rename: 'org.dosbox_staging.dosbox_staging.svg',
    install_dir: icons_hicolor / 'scalable' / 'apps',
)
