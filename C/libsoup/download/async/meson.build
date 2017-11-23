project (
    'Asynchronous File Download with libsoup', 'c',
    version: '0.1.0',
    meson_version: '>=0.40.0',
    default_options: []
)

cc = meson.get_compiler ('c')

exe_c_args = ['-O2', '-g']
exe_link_args = []

if cc.has_argument('-fPIE')
    exe_c_args += '-fPIE'
    exe_link_args += '-fpie'
endif

# Settings
exe_c_args += []
exe_link_args += []

# Dependencies
glib_dep = dependency (
    'glib-2.0',
    version: '>=2.50'
)

gio_dep = dependency (
    'gio-2.0',
    version: '>=2.50'
)

libsoup_dep = dependency (
    'libsoup-2.4'
)

dependencies = [
    glib_dep,
    gio_dep,
    libsoup_dep
]

# Sources
sources = [
    'download-async.h',
    'download-async.c',
    'main.c'
]

# Build
executable (
    'download-async',
    sources,
    dependencies: dependencies,
    c_args: exe_c_args,
    link_args: exe_link_args,
    gui_app: false,
    install: true
)
