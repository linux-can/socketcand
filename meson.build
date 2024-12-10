# SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
# SPDX-FileCopyrightText: 2024 Matthias Weißer <m.weisser.m@gmail.com>

project('socketcand', 'c', version : '0.6.1')
conf = configuration_data()

top_inc = include_directories('.', 'include')
deps = [dependency('threads')]

enable_rc_script = get_option('rc_script')
enable_init_script = get_option('init_script')

# Check if libconfig is available
enable_libconfig = get_option('libconfig')
if enable_libconfig
    conf.set_quoted('HAVE_LIBCONFIG', '1')
    deps += dependency('libconfig')
endif

# Check if libsocketcan is available
enable_libsocketcan = get_option('libsocketcan')
if enable_libsocketcan
    conf.set_quoted('HAVE_LIBSOCKETCAN', '1')
    deps += dependency('libsocketcan')
endif

conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_BUGREPORT', 'https://github.com/linux-can/socketcand/issues')
configure_file(output : 'config.h', configuration : conf)

subdir('src')
subdir('files')
