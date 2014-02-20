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
* *Documentation:* doxygen (http://doxygen.org/) - doxygen/doxygen
* *Documentation:* sphinx (http://sphinx-doc.org/) - python-sphinx/
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

Note: The C tests have to be builded by ``make tests``)!

### Run only Python unittests (from your checkout dir):

    PYTHONPATH=`readlink -f ./build/src/python/` nosetests -s tests/python/tests/

### Links

[Bugzilla](https://bugzilla.redhat.com/buglist.cgi?bug_status=NEW&bug_status=ASSIGNED&bug_status=MODIFIED&bug_status=VERIFIED&component=createrepo_c&query_format=advanced)

### Important notes

In original createrepo ``sha`` is a nickname for the ``sha1`` checksum.
Createrepo_c mimics this behaviour.

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

