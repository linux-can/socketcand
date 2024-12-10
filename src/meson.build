# SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
# SPDX-FileCopyrightText: 2024 Matthias Weißer <m.weisser.m@gmail.com>

src =  [
    'socketcand.c',
    'statistics.c',
    'beacon.c',
    'state_bcm.c',
    'state_raw.c',
    'state_isotp.c',
    'state_control.c',
    'state_nobus.c',
]


if enable_libsocketcan
    src += 'canctl.c'
endif

executable('socketcand',
    src,
    include_directories : top_inc,
    dependencies: deps,
    install : true,
    install_dir : get_option('sbindir')
)

executable('socketcandcl',
    [
        'socketcandcl.c',
    ],
    include_directories : top_inc,
    dependencies: deps,
    install : true
)
