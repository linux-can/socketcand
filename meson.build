project('socketcand', 'c', version : '0.6.1')
conf = configuration_data()

deps = [dependency('threads')]

enable_rc_script = get_option('rc_script')
enable_init_script = get_option('init_script')

# Check if libconfig is available
enable_libconfig = get_option('libconfig')
if enable_libconfig
    dep_libconfig = dependency('libconfig')
    if dep_libconfig.found()
        conf.set_quoted('HAVE_LIBCONFIG', '1')
        deps += dep_libconfig
    endif
endif

executable('socketcand',
    [
        'socketcand.c',
        'statistics.c',
        'beacon.c',
        'state_bcm.c',
        'state_raw.c',
        'state_isotp.c',
        'state_control.c',
    ],
    include_directories : include_directories('include'),
    dependencies: deps,
    install : true,
    install_dir : get_option('sbindir')
)

executable('socketcandcl',
    [
        'socketcandcl.c',
    ],
    include_directories : include_directories('include'),
    dependencies: deps,
    install : true
)

conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_BUGREPORT', 'https://github.com/linux-can/socketcand/issues')
configure_file(output : 'config.h', configuration : conf)

subdir('files')
