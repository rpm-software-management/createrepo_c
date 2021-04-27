from skbuild import setup


with open('VERSION.cmake', 'r+') as version_file:
    lines = version_file.read().splitlines()
    # parse out digit characters from the line, convert to int
    numbers = [int("".join(filter(str.isdigit, line))) for line in lines]
    # build version string
    version = '{major}.{minor}.{patch}'.format(
        major=numbers[0],
        minor=numbers[1],
        patch=numbers[2]
    )

setup(
    name='createrepo_c',
    description='C implementation of createrepo',
    version=version,
    license='GPLv2+',
    author='RPM Software Management',
    author_email='rpm-ecosystem@lists.rpm.org',
    url='https://github.com/rpm-software-management',
    classifiers=[
        'License :: OSI Approved :: GNU General Public License v2 or later (GPLv2+)',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: C',
        'Topic :: System :: Software Distribution',
        'Topic :: System :: Systems Administration',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
    ],
    packages=['createrepo_c'],
    package_dir={
        'createrepo_c': 'src/python/createrepo_c'
    },
    cmake_args=[
        '-DBIN_INSTALL_DIR:PATH=src/python/createrepo_c/data/bin',
        '-DBUILD_LIBCREATEREPO_C_SHARED:BOOL=OFF',
        '-DCREATEREPO_C_INSTALL_DEVELOPMENT:BOOL=OFF',
        '-DCREATEREPO_C_INSTALL_MANPAGES:BOOL=OFF',
        '-DENABLE_BASHCOMP:BOOL=OFF',
        '-DENABLE_DRPM:BOOL=OFF',
        '-DWITH_LEGACY_HASHES:BOOL=ON',
    ],
    cmake_languages=['C'],
    entry_points={
        'console_scripts': [
            'createrepo_c=createrepo_c:createrepo_c',
            'mergerepo_c=createrepo_c:mergerepo_c',
            'modifyrepo_c=createrepo_c:modifyrepo_c',
            'sqliterepo_c=createrepo_c:sqliterepo_c'
        ]
    },
)
