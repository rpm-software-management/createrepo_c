import os.path

TESTDATADIR = os.path.normpath(os.path.join(__file__, "../../testdata"))
PACKAGESDIR=os.path.join(TESTDATADIR, "packages")

PACKAGES = [
    "Archer-3.4.5-6.x86_64.rpm",
    "fake_bash-1.1.1-1.x86_64.rpm",
    "super_kernel-6.0.1-2.x86_64.rpm",
]