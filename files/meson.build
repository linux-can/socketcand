if enable_libconfig
    install_data(
        sources: 'socketcand.conf',
        install_dir: '/etc',
    )
endif

if enable_rc_script
    install_data(
        sources: 'socketcand-rc.d',
        rename: 'socketcand',
        install_mode: 'rwxr-xr-x',
        install_dir: '/etc/rc.d',
    )
endif

if enable_init_script
    init_conf = configuration_data()
    init_conf.set('fullpath', get_option('prefix') / get_option('sbindir'))
    configure_file(
        input : 'socketcand-init.d.in',
        output : 'socketcand',
        configuration : init_conf,
        install : true,
        install_mode : 'rwxr-xr-x',
        install_dir : '/etc/init.d',
    )
endif
