# createrepo_c

C implementation of createrepo

Run `createrepo -h` for usage syntax.

# Devel tips

## Building

Package build requires - Pkg name in Fedora/Ubuntu:

* bzip2 (http://bzip.org/) - bzip2-devel/libbz2-dev
* cmake (http://www.cmake.org/) - cmake/cmake
* expat (http://expat.sourceforge.net/) - expat-devel/libexpat1-dev
* file (http://www.darwinsys.com/file/) - file-devel/libmagic-dev
* glib2 (http://developer.gnome.org/glib/) - glib2-devel/libglib2.0-dev
* libcurl (http://curl.haxx.se/libcurl/) - libcurl-devel/libcurl4-openssl-dev
* libxml2 (http://xmlsoft.org/) - libxml2-devel/libxml2-dev
* python (http://python.org/) - python2-devel/libpython2.7-dev
* rpm (http://www.rpm.org/) - rpm-devel/librpm-dev
* openssl (http://www.openssl.org/) - openssl-devel/libssl-dev
* sqlite3 (https://sqlite.org/) - sqlite-devel/libsqlite3-dev
* xz (http://tukaani.org/xz/) - xz-devel/liblzma-dev
* zlib (http://www.zlib.net/) - zlib-devel/zlib1g-dev
* *Optional:* doxygen (http://doxygen.org/) - doxygen/doxygen
* **Test requires:** check (http://check.sourceforge.net/) - check-devel/check
* **Test requires:** python-nose (https://nose.readthedocs.org/) - python-nose/python-nose


From your checkout dir:

    mkdir build
    cd build/
    cmake ..
    make

To build the documentation, from the build/ directory:
    make doc

**Note:** For build with debugging symbols you could use (from the build/ directory):

    cmake -DCMAKE_BUILD_TYPE:STRING=DEBUG .. && make

## Build tarball

    utils/make_tarball.sh [git revision]

Without git revision specified HEAD is used.

## Build RPM package

Modify createrepo_c.spec and run:

    utils/make_rpm.sh

Note: [Current .spec for Fedora rawhide](http://pkgs.fedoraproject.org/cgit/createrepo_c.git/plain/createrepo_c.spec)

## Testing

All unit tests run from librepo checkout dir

### Build C tests && run c and python tests

    make tests && make test

Note: For a verbose output of testing use: ``make ARGS="-V" test``

### Run only C unittests (from your checkout dir):

    build/tests/run_gtester.sh

### Run only Python unittests (from your checkout dir):

    PYTHONPATH=`readlink -f ./build/src/python/` nosetests -s tests/python/tests/

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

