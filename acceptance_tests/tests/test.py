import time
import shutil
import inspect
import os.path
import tempfile
import unittest
import threading
import subprocess

from fixtures import PACKAGESDIR
from fixtures import PACKAGES

OUTPUTDIR = None
OUTPUTDIR_LOCK = threading.Lock()

def get_outputdir():
    global OUTPUTDIR
    if OUTPUTDIR:
        return OUTPUTDIR
    OUTPUTDIR_LOCK.acquire()
    if not OUTPUTDIR:
        prefix = time.strftime("./testresults_%Y%m%d_%H%M%S_")
        OUTPUTDIR = tempfile.mkdtemp(prefix=prefix, dir="./")
    OUTPUTDIR_LOCK.release()
    return OUTPUTDIR

def run(cmd, logfile=None, workdir=None, stdin_data=None):
    """Stolen from the kobo library.
     Author of the original function is dmach@redhat.com"""
    if type(cmd) in (list, tuple):
        import pipes
        cmd = " ".join(pipes.quote(i) for i in cmd)

    if logfile is not None:
        logfile = open(logfile, "a")

    stdin = None
    if stdin_data is not None:
        stdin = subprocess.PIPE

    proc = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, cwd=workdir)

    if stdin_data is not None:
        class StdinThread(threading.Thread):
            def run(self):
                proc.stdin.write(stdin_data)
                proc.stdin.close()
        stdin_thread = StdinThread()
        stdin_thread.daemon = True
        stdin_thread.start()

    output = ""
    while True:
        lines = proc.stdout.readline()
        if lines == "":
            break
        if logfile:
            logfile.write(lines)
        output += lines
    proc.wait()
    logfile.close()

    if stdin_data is not None:
        stdin_thread.join()

    return proc.returncode, output

class CrResult(object):
    def __init__(self):
        self.rc = None       # Return code
        self.out = None      # stdout + stderr
        self.dir = None      # Directory that was processed
        self.prog = None     # Program name
        self.cmd = None      # Complete command
        self.outdir = None   # Output directory
        self.logfile = None  # Log file where the out was logged

class RepoDiffResult(object):
    def __init__(self):
        self.rc = None
        self.out = None
        self.repo1 = None
        self.repo2 = None
        self.cmd = None      # Complete command
        self.logfile = None  # Log file where the out was logged

class BaseTestCase(unittest.TestCase):

    def __init__(self, *args, **kwargs):
        self.outdir = get_outputdir()
        self.tcdir = os.path.join(self.outdir, self.__class__.__name__)
        os.mkdir(self.tcdir)
        unittest.TestCase.__init__(self, *args, **kwargs)

    def postinittest(self):
        pass

    def inittest(self):
        caller = inspect.stack()[1][3]
        self.tdir = os.path.join(self.tcdir, caller)
        os.mkdir(self.tdir)
        self.indir = os.path.join(self.tdir, "input")
        os.mkdir(self.indir)
        if self.__class__.__doc__:
            description_fn = os.path.join(self.tcdir, "description")
            open(description_fn, "w").write(self.__class__.__doc__+'\n')
        #self.log = # TODO
        self.postinittest()

    def tearDown(self):
        #shutil.rmtree(self.tmpdir)
        pass

    def set_description(self, doc):
        fn = os.path.join(self.tdir, "description")
        open(fn, "w").write(doc+'\n')

    def set_success(self):
        fn = os.path.join(self.tdir, "SUCCESS")
        open(fn, "w")

    def copy_pkg(self, name, dst):
        src = os.path.join(PACKAGESDIR, name)
        shutil.copy(src, dst)

    def run_cr(self, dir, args=None, c=False):
        res = CrResult
        res.dir = dir
        res.prog = "createrepo_c" if c else "createrepo"
        res.outdir = os.path.join(self.tdir, res.prog)
        os.mkdir(res.outdir)
        res.logfile =  os.path.join(self.tdir, "out_%s" % res.prog)
        res.cmd = "%(prog)s --verbose -o %(outdir)s %(dir)s" % {
            "prog": res.prog,
            "dir": res.dir,
            "outdir": res.outdir,
        }
        res.rc, res.out = run(res.cmd, logfile=res.logfile)
        return res

    def compare_repos(self, repo1, repo2):
        res = RepoDiffResult()
        res.repo1 = os.path.join(repo1, "repodata")
        res.repo2 = os.path.join(repo2, "repodata")
        res.logfile = os.path.join(self.tdir, "out_cmp")
        res.cmd = "repo_diff.py --verbose --compare %s %s" % (res.repo1, res.repo2)
        res.rc, res.out = run(res.cmd, logfile=res.logfile)
        return res

class TestCaseCreaterepo01(BaseTestCase):
    """Empty input repository"""

    def postinittest(self):
        self.copy_pkg(PACKAGES[0], self.indir)
        self.copy_pkg(PACKAGES[1], self.indir)
        self.copy_pkg(PACKAGES[2], self.indir)

    def test_01_createrepo_3packages(self):
        self.inittest()
        self.set_description("Repo that contains 3 regular packages")
        crres = self.run_cr(self.indir)
        crcres = self.run_cr(self.indir, c=True)
        self.assertFalse(crres.rc)
        self.assertFalse(crcres.rc)
        cmpres = self.compare_repos(crres.outdir, crcres.outdir)
        self.assertFalse(cmpres.rc)
        self.set_success()