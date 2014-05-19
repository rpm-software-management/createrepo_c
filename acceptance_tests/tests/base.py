import os
import time
import shutil
import os.path
import tempfile
import unittest
import threading
import subprocess

from fixtures import PACKAGESDIR

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

class RepoSanityCheckResult(object):
    def __init__(self):
        self.rc = None
        self.out = None
        self.repo = None
        self.cmd = None
        self.logfile = None

class BaseTestCase(unittest.TestCase):

    def __init__(self, *args, **kwargs):
        self.outdir = get_outputdir()
        self.tcdir = os.path.join(self.outdir, self.__class__.__name__)
        if not os.path.exists(self.tcdir):
            os.mkdir(self.tcdir)
        if self.__class__.__doc__:
            description_fn = os.path.join(self.tcdir, "description")
            open(description_fn, "w").write(self.__class__.__doc__+'\n')
        unittest.TestCase.__init__(self, *args, **kwargs)

        self.tdir = None            # Test dir for the current test
        self.indir = None           # Input dir for the current test
        self._currentResult = None  # Result of the current test

        # Prevent use of a first line from test docstring as its name in output
        self.shortDescription_orig = self.shortDescription
        self.shortDescription = self._shortDescription

    def _shortDescription(self):
        return ".".join(self.id().split('.')[-2:])

    def run(self, result=None):
        # Hacky
        self._currentResult = result # remember result for use in tearDown
        unittest.TestCase.run(self, result)

    def setUp(self):
        caller = self.id().split(".", 3)[-1]
        self.tdir = os.path.abspath(os.path.join(self.tcdir, caller))
        os.mkdir(self.tdir)
        self.indir = os.path.join(self.tdir, "input")
        os.mkdir(self.indir)
        description = self.shortDescription_orig()
        if description:
            fn = os.path.join(self.tdir, "description")
            open(fn, "w").write(description+'\n')
        #self.log = # TODO
        self.main_cwd = os.getcwd()
        os.chdir(self.tdir)
        self.setup()

    def setup(self):
        pass

    def tearDown(self):
        if self.tdir and self._currentResult:
            if not len(self._currentResult.errors) + len(self._currentResult.failures):
                self.set_success()
        self.teardown()
        os.chdir(self.main_cwd)

    def teardown(self):
        pass

    def runcmd(self, cmd, logfile=None, workdir=None, stdin_data=None):
        """Stolen from the kobo library.
         Author of the original function is dmach@redhat.com"""
        if type(cmd) in (list, tuple):
            import pipes
            cmd = " ".join(pipes.quote(i) for i in cmd)

        if logfile is not None:
            logfile = open(logfile, "a")
            logfile.write("cd %s\n" % os.getcwd())
            for var in ("PATH", "PYTHONPATH", "LD_LIBRARY_PATH"):
                logfile.write('export %s="%s"\n' % (var, os.environ.get(var,"")))
            logfile.write("\n")
            logfile.write(cmd+"\n")
            logfile.write("\n+%s+\n\n" % ('-'*77))

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

        if logfile:
            logfile.write("\n+%s+\n\n" % ('-'*77))
            logfile.write("RC: %s (%s)\n" % (proc.returncode, "Success" if not proc.returncode else "Failure"))
            logfile.close()

        if stdin_data is not None:
            stdin_thread.join()

        return proc.returncode, output

    def set_success(self):
        fn = os.path.join(self.tdir, "SUCCESS")
        open(fn, "w")

    def copy_pkg(self, name, dst):
        src = os.path.join(PACKAGESDIR, name)
        shutil.copy(src, dst)

    def run_cr(self, dir, args=None, c=False, outdir=None):
        res = CrResult()
        res.dir = dir
        res.prog = "createrepo_c" if c else "createrepo"
        if not outdir:
            res.outdir = os.path.join(self.tdir, res.prog)
        else:
            res.outdir = os.path.join(self.tdir, outdir)
        os.mkdir(res.outdir)
        res.logfile =  os.path.join(self.tdir, "out_%s" % res.prog)
        res.cmd = "%(prog)s --verbose -o %(outdir)s %(args)s %(dir)s" % {
            "prog": res.prog,
            "dir": res.dir,
            "args": args or "",
            "outdir": res.outdir,
        }
        res.rc, res.out = self.runcmd(res.cmd, logfile=res.logfile)
        return res

    def compare_repos(self, repo1, repo2):
        res = RepoDiffResult()
        res.repo1 = os.path.join(repo1, "repodata")
        res.repo2 = os.path.join(repo2, "repodata")
        res.logfile = os.path.join(self.tdir, "out_cmp")
        res.cmd = "repo_diff.py --verbose --compare %s %s" % (res.repo1, res.repo2)
        res.rc, res.out = self.runcmd(res.cmd, logfile=res.logfile)
        return res

    def check_repo_sanity(self, repo):
        res = RepoSanityCheckResult()
        res.repo = os.path.join(repo, "repodata")
        res.logfile = os.path.join(self.tdir, "out_sanity")
        res.cmd = "repo_diff.py --verbose --check %s" % res.repo
        res.rc, res.out = self.runcmd(res.cmd, logfile=res.logfile)
        return res

    def assert_run_cr(self, *args, **kwargs):
        res = self.run_cr(*args, **kwargs)
        self.assertFalse(res.rc)
        return res

    def assert_same_results(self, indir, args=None):
        crres = self.run_cr(indir, args)
        crcres = self.run_cr(indir, args, c=True)
        self.assertFalse(crres.rc)
        self.assertFalse(crcres.rc)
        cmpres = self.compare_repos(crres.outdir, crcres.outdir)
        self.assertFalse(cmpres.rc)

    def assert_repo_sanity(self, repo):
        res = self.check_repo_sanity(repo)
        self.assertFalse(res.rc)
        return res