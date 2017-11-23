# Modules
gnome = import ('gnome')

# Config
ref_conf = configuration_data ()

ref_conf.set ('PACKAGE_VERSION', meson.project_version())

# Dependencies
glib_dep = dependency (
    'glib-2.0',
    version: '>=2.50'
)

gtk_dep = dependency (
    'gtk+-3.0',
    version: '>=3.18'
)

# Variables
private_headers = []

## Doc paths
glib_prefix = glib_dep.get_pkgconfig_variable ('prefix')
glib_doc_path = join_paths (glib_prefix, 'share', 'gtk-doc', 'html')

gtk_prefix = gtk_dep.get_pkgconfig_variable ('prefix')
gtk_doc_path = join_paths (gtk_prefix, 'share', 'gtk-doc', 'html')

# version.xml
configure_file (
    input: 'version.xml.in',
    output: 'version.xml',
    configuration: ref_conf
)

# Build docs
gnome.gtkdoc (
    'snippets',
    main_xml: 'snippets-docs.xml',
    src_dir: [
        join_paths (meson.source_root (), 'C'),
        join_paths (meson.build_root (), 'C')
    ],
    scan_args: [
        '--ignore-headers=@0@'.join (private_headers),
    ],
    fixxref_args: [
        '--extra-dir=@0@'.format(join_paths(glib_doc_path, 'glib')),
        '--extra-dir=@0@'.format(join_paths(glib_doc_path, 'gmodule')),
        '--extra-dir=@0@'.format(join_paths(glib_doc_path, 'gobject')),
        '--extra-dir=@0@'.format(join_paths(glib_doc_path, 'gio')),
        '--extra-dir=@0@'.format(join_paths(glib_doc_path, 'libsoup')),
        '--extra-dir=@0@'.format(join_paths(gtk_doc_path, 'gdk')),
        '--extra-dir=@0@'.format(join_paths(gtk_doc_path, 'gtk')),
    ],
    install: true
)
