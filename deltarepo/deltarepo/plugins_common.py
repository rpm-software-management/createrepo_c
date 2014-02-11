"""
Classes related for communication between plugins
and applicator/generator.
"""

class GlobalBundle(object):

    __slots__ = ("contenthash_type_str",
                 "unique_md_filenames",
                 "calculated_old_contenthash",
                 "calculated_new_contenthash",
                 "force_database",
                 "ignore_missing")

    def __init__(self):
        self.contenthash_type_str = "sha256"
        self.unique_md_filenames = True
        self.force_database = False
        self.ignore_missing = False

        # Filled by plugins
        self.calculated_old_contenthash = None
        self.calculated_new_contenthash = None

class Metadata(object):
    """Metadata file"""

    def __init__(self, metadata_type):

        self.metadata_type = metadata_type

        # Output directory
        self.out_dir = None

        # Repomd records (if available in corresponding repomd.xml)
        self.old_rec = None
        self.delta_rec = None
        self.new_rec = None

        # Paths
        # If record is available in corresponding repomd.xml
        self.old_fn = None      # in old (source) repository
        self.delta_fn = None    # in delta repository
        self.new_fn = None      # in new (target) repository

        # Exists the file on the filesystem?
        self.old_fn_exists = False
        self.delta_fn_exists = False
        self.new_fn_exists = False

        # Settings
        self.checksum_type = None
        self.compression_type = None