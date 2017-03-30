{
    "targets": [
        {
            "variables": {
                "other_cflags%": "",
                "other_lflags%": "",
            },
            'cflags_cc': ['-fexceptions'],
            'target_name': 'emulex',
            'include_dirs': [
                '/usr/local/include',
                '/usr/include',
            ],
            'library_dirs': [
                '/usr/local/lib',
                '/usr/lib',
            ],
            'sources': [
                'src/common.h',
                'src/plugins.hpp',
                'src/plugins.cpp',
                'src/hash.hpp',
                'src/hash.cpp'
            ],
            'conditions': [
                ['OS=="mac"', {
                    'xcode_settings': {
                        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
                        'GCC_ENABLE_CPP_RTTI': 'YES',
                        'MACOSX_DEPLOYMENT_TARGET': '10.11',
                        'OTHER_CFLAGS': [
                            "<(other_cflags)",
                        ],
                        'OTHER_LDFLAGS': [
                            "-stdlib=libc++",
                            "-L/usr/local/lib",
                            '-lboost_system',
                            "-lboost_iostreams",
                            "-lboost_filesystem",
                            "-lboost_thread",
                            "-lemulex",
                            "-Wl,-rpath,.",
                            "<(other_cflags)",
                        ],
                    },
                }],
                ['OS=="linux"', {
                    'cflags_cc': ['-frtti'],
                    'ldflags': [
                        "-L/usr/local/lib",
                        '-lboost_system',
                        "-lboost_iostreams",
                        "-lboost_filesystem",
                        "-lboost_thread",
                        "-lemulex",
                        "<(other_cflags)",
                    ],
                }],
            ],
        }
    ]
}
