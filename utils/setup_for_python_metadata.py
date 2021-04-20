from setuptools import setup
import sys

# This is a simple and fragile way of passing the current version
# from cmake to setup as I assume no one else will use this.
#
# This script has to have the version always specified as last argument.
version = sys.argv.pop()

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
)
