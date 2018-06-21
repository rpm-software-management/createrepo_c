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
    requires_python=">=3.5",
    packages=['createrepo_c'],
    package_dir={
        'createrepo_c': 'src'
    },
    cmake_args=['-DPYTHON_DESIRED:STRING=3']
)
