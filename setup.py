from skbuild import setup

setup(
    name='createrepo_c',
    description='C implementation of createrepo',
    version='0.10.0',
    license='GPLv2',
    author='RPM Software Management',
    author_email='',
    url='https://github.com/rpm-software-management',
    classifiers=(
        'License :: OSI Approved :: GNU General Public License v2 (GPLv2)',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: C',
        'Topic :: System :: Software Distribution',
        'Topic :: System :: Systems Administration',
        'Programming Language :: Python :: 3',
    ),
    python_requires=">=3.5",
    packages=['createrepo_c'],
    package_dir={
        'createrepo_c': 'src/python'
    },
    cmake_args=[
        '-DBUILD_LIBCREATEREPO_C_SHARED:BOOL=OFF',
        '-DCREATEREPO_C_INSTALL_DEVELOPMENT:BOOL=OFF',
        '-DCREATEREPO_C_INSTALL_MANPAGES:BOOL=OFF',
        '-DENABLE_BASHCOMP:BOOL=OFF',
        '-DPYTHON_DESIRED:STRING=3',
    ],
    cmake_languages=['C'],
)
