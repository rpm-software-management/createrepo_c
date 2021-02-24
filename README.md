# createrepo_c

C implementation of createrepo

Run `createrepo -h` for usage syntax.

# Devel tips

## Building

Package build requires - Pkg name in Fedora/Ubuntu:

* bzip2 (http://bzip.org/) - bzip2-devel/libbz2-dev
* cmake (http://www.cmake.org/) - cmake/cmake
* drpm (https://github.com/rpm-software-management/drpm) - drpm-devel/
* file (http://www.darwinsys.com/file/) - file-devel/libmagic-dev
* glib2 (http://developer.gnome.org/glib/) - glib2-devel/libglib2.0-dev
* libcurl (http://curl.haxx.se/libcurl/) - libcurl-devel/libcurl4-openssl-dev
* libmodulemd (https://github.com/fedora-modularity/libmodulemd/) - libmodulemd-devel/
* libxml2 (http://xmlsoft.org/) - libxml2-devel/libxml2-dev
* python (http://python.org/) - python3-devel/libpython3-dev
* rpm (http://www.rpm.org/) - rpm-devel/librpm-dev
* openssl (http://www.openssl.org/) - openssl-devel/libssl-dev
* sqlite3 (https://sqlite.org/) - sqlite-devel/libsqlite3-dev
* xz (http://tukaani.org/xz/) - xz-devel/liblzma-dev
* zchunk (https://github.com/zchunk/zchunk) - zchunk-devel/
* zlib (http://www.zlib.net/) - zlib-devel/zlib1g-dev
* *Documentation:* doxygen (http://doxygen.org/) - doxygen/doxygen
* *Documentation:* sphinx (http://sphinx-doc.org/) - python3-sphinx/python3-sphinx
* **Test requires:** check (http://check.sourceforge.net/) - check-devel/check
* **Test requires:** xz (http://tukaani.org/xz/) - xz/
* **Test requires:** zchunk (https://github.com/zchunk/zchunk) - zchunk/

From your checkout dir:

    mkdir build
    cd build/
    cmake ..
    make

To build the documentation, from the build/ directory:

    make doc

**Note:** For build with debugging symbols you could use (from the build/ directory):

    cmake -DCMAKE_BUILD_TYPE:STRING=DEBUG .. && make

## Building from an rpm checkout

E.g. when you want to try weak and rich dependencies.

    cmake .. && make

**Note:** The RPM must be built in that directory

Commands I am using for building the RPM:

    cd /home/tmlcoch/git/rpm
    CPPFLAGS='-I/usr/include/nss3/ -I/usr/include/nspr4/' ./autogen.sh --rpmconfigure --with-vendor=redhat --with-external-db --with-lua --with-selinux --with-cap --with-acl --enable-python
    make clean && make
## Other build options

### ``-DENABLE_LEGACY_WEAKDEPS=ON``

Enable legacy SUSE/Mageia/Mandriva weakdeps support (Default: ON)

### ``-DENABLE_THREADED_XZ_ENCODER=ON``

Threaded XZ encoding (Default: OFF)

Note: This option is disabled by default, because Createrepo_c
parallelizes a lot of tasks (including compression) by default; this
only adds extra threads on XZ library level which causes thread bloat
and for most usecases doesn't bring any performance boost.
On regular hardware (e.g. less-or-equal 4 cores) this option may even
cause degradation of performance.

### ``-DENABLE_DRPM=ON``

Enable DeltaRPM support using drpm library (Default: ON)

Adds support for creating DeltaRPMs and incorporating them
into the repository.

### ``-DWITH_ZCHUNK=ON``

Build with zchunk support (Default: ON)

### ``-DWITH_LIBMODULEMD=ON``

Build with libmodulemd support (Default: ON)

Adds support for working with repos containing
[Fedora Modularity](https://docs.fedoraproject.org/en-US/modularity/) metadata.


## Build tarball

    utils/make_tarball.sh [git revision]

Without git revision specified HEAD is used.

## Build Python package

To create a binary "wheel" distribution, use:

    python setup.py bdist_wheel

To create a source distribution, use:

    python setup.py sdist

Installing source distributions require the installer of the package to have all of the build dependencies installed on their system, since they compile the code during installation. Binary distributions are pre-compiled, but they are likely not portable between substantially different systems, e.g. Fedora and Ubuntu.

Note: if you are building a bdist or installing the sdist on a system with an older version of Pip, you may need to install the ```scikit-build``` Python package first.

To install either of these packages, use:

    pip install dist/{{ package name }}

To create an "editable" install of createrepo_c, use:

    python setup.py develop

Note: To recompile the libraries and binaries, you muse re-run this command.


## Build RPM package

Modify createrepo_c.spec and run:

    utils/make_rpm.sh

Note: [Current .spec for Fedora rawhide](https://src.fedoraproject.org/rpms/createrepo_c/blob/master/f/createrepo_c.spec)

## Testing

All unit tests run from librepo checkout dir

### Build C tests && run c and python tests

    make tests && make test

Note: For a verbose output of testing use: ``make ARGS="-V" test``

### Run only C unittests (from your checkout dir):

    build/tests/run_tests.sh

Note: The C tests have to be built by ``make tests``)!

### Run only Python unittests (from your checkout dir):

    PYTHONPATH=`readlink -f ./build/src/python/` python3 -m unittest discover -bs tests/python/

Note: When compiling createrepo_c without libmodulemd support add ``WITH_LIBMODULEMD=OFF``

### Links

[Bugzilla](https://bugzilla.redhat.com/buglist.cgi?bug_status=NEW&bug_status=ASSIGNED&bug_status=MODIFIED&bug_status=VERIFIED&component=createrepo_c&query_format=advanced)

### Important notes

In original createrepo ``sha`` is a nickname for the ``sha1`` checksum.
Createrepo_c mimics this behaviour.

## Contribution

Here's the most direct way to get your work merged into the project.

1. Fork the project
1. Clone down your fork
1. Implement your feature or bug fix and commit changes
1. If the change fixes a bug at [Red Hat bugzilla](https://bugzilla.redhat.com/), or if it is important to the end user, add the following block to the commit message:

       = changelog =
       msg:           message to be included in the changelog
       type:          one of: bugfix/enhancement/security (this field is required when message is present)
       resolves:      URLs to bugs or issues resolved by this commit (can be specified multiple times)
       related:       URLs to any related bugs or issues (can be specified multiple times)

   * For example::

         = changelog =
         msg: Enhance error handling when locating repositories
         type: bugfix
         resolves: https://bugzilla.redhat.com/show_bug.cgi?id=1762697

   * For your convenience, you can also use git commit template by running the following command in the top-level directory of this project:

         git config commit.template ./.git-commit-template

1. In a separate commit, add your name and email into the [authors file](https://github.com/rpm-software-management/createrepo_c/blob/master/AUTHORS) as a reward for your generosity
1. Push the branch to your fork
1. Send a pull request for your branch

---------------------------------------------------

# Differences in behavior between createrepo_c and createrepo

## Checksums after update

### Use case:
- Repodata in repo/ are has checksum xxx
- Params: --update --checksum=yyy repo/

### createrepo_c result:
- All package checksums are recalculated into yyy

### original createrepo result:
- Only new and changed packages has yyy checksums
  other packages has still xxx checksums


## Skip symlinks param

### Use case:
- Some packages in repo/ are symlinks
- Params: --skip-symlinks repo/

### createrepo_c result:
- Symlinked packages are ignored

### original createrepo result:
- Symlinked packages are processed
  (https://bugzilla.redhat.com/show_bug.cgi?id=828848)


## Base path from update-md-path repo

### Use case:
- A somebody else's repo is somewhere
- The repo items have set a base path to http://foo.com/
- We want to create metadata for our repo
- Some packages in our repo are same as packages in somebody else's repo
- We want to speed up creation of our repodata with
  combo --update and --update-md-path=somebody_else's_repo
- Params: --update --update-md-path=ftp://somebody.else/repo our_repo/

### createrepo_c results:
- All our packages have no base path set (if we don't set --baseurl explicitly)

### original createrepo result:
- Some packages in metadata (which was same in our repo and in somebody
  else's repo) have base path set to http://foo.com/
- (https://bugzilla.redhat.com/show_bug.cgi?id=875029)


## Crippled paths in filelists.xml after update

### Use case:
- A repo with old metadata exists
- We want to update metadata
- Params: --update repo/

### createrepo_c results:
- All is fine

### original createrepo result:
- Some paths in filelists.xml are crippled
  (https://bugzilla.redhat.com/show_bug.cgi?id=835565)


## --update leaves behind some old repodata files

### Use case:
- A repo with repodata created with --simple-md-filenames exists
- We want to update repodata to have checksums in filenames
- Params: --update repo/

### createrepo_c results:
- All repodata contains checksum in the name

### original createrepo result:
- All repodata contains checksum in the name
- There are old metadata without checksum in the name too
- (https://bugzilla.redhat.com/show_bug.cgi?id=836917)

## Mergerepo_c
### Default merge method
- Original mergerepo included even packages with the same NVR by default
- Mergerepo_c can be configured by --method option to specify how repositories should be merged.
- Additionally its possible to use --all option to replicate original mergerepo behavior.

## Modifyrepo_c

Modifyrepo_c is compatible with classical Modifyrepo except some misbehaviour:

* TODO: Report bugs and add reference here

### Batch file

When there is need to do several modification to repository (``repomd.xml``)
a batch file could be used.

> Batch file is Modifyrepo_c specific. It is not supported by the classical
Modifyrepo - at least not yet.

#### Example

    # Add:
    #   [<path/to/file>]
    #   <options>

    # Metadata that use a bunch of config options
    [some/path/comps.xml]
    type=group
    compress=true
    compress-type=gz
    unique-md-filenames=true
    checksum=sha256
    new-name=group.xml

    # Metadata that use default settings
    [some/path/bar.xml]

    # Remove:
    #   [<metadata name>]
    #   remove=true

    [updateinfo]
    remove=true


#### Supported options

| Option name   | Description | Supported value(s) | Default |
|---------------|-------------|--------------------|---------|
| path          | Path to the file. When specified it override the path specified in group name (name between [] parenthesis) | Any string | group name (string between '[' ']') |
| type          | Type of the metadata | Any string | Based on filename |
| remove        | Remove specified file/type from repodata | ``true`` or ``false`` | ``false`` |
| compress      | Compress the new metadata before adding it to repo | ``true`` or ``false`` | ``true`` |
| compress-type | Compression format to use | ``gz``, ``bz2``, ``xz`` | ``gz`` |
| checksum      | Checksum type to use | ``md5``, ``sha``, ``sha1``, ``sha224``, ``sha256``, ``sha384``, ``sha512`` | ``sha256`` |
| unique-md-filenames | Include the file's checksum in the filename | ``true`` or ``false`` | ``true`` |
| new-name      | New name for the file. If ``compress`` is ``true``, then compression suffix will be appended. If ``unique-md-filenames`` is ``true``, then checksum will be prepended. | Any string | Original source filename |

#### Notes

* Lines beginning with a '#' and blank lines are considered comments.
* If ``remove=true`` is used, no other config options should be used

