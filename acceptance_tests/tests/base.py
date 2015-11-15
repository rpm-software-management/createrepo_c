import os
import re
import time
import shutil
import pprint
import filecmp
import os.path
import tempfile
import unittest
import threading
import subprocess

from .fixtures import PACKAGESDIR

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


class _Result(object):
    def __str__(self):
        return pprint.pformat(self.__dict__)


class CrResult(_Result):
    def __init__(self):
        self.rc = None       # Return code
        self.out = None      # stdout + stderr
        self.dir = None      # Directory that was processed
        self.prog = None     # Program name
        self.cmd = None      # Complete command
        self.outdir = None   # Output directory
        self.logfile = None  # Log file where the out was logged


class RepoDiffResult(_Result):
    def __init__(self):
        self.rc = None
        self.out = None
        self.repo1 = None
        self.repo2 = None
        self.cmd = None      # Complete command
        self.logfile = None  # Log file where the out was logged


class RepoSanityCheckResult(_Result):
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
        self.main_cwd = os.getcwd()

    def _shortDescription(self):
        return ".".join(self.id().split('.')[-2:])

    def run(self, result=None):
        # Hacky
        self._currentResult = result # remember result for use in tearDown
        unittest.TestCase.run(self, result)

    def setUp(self):
        os.chdir(self.main_cwd) # In case of TimedOutException in Nose test... the tearDown is not called :-/
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

        # TODO: Add time how long the command takes

        if type(cmd) in (list, tuple):
            import pipes
            cmd = " ".join(pipes.quote(i) for i in cmd)

        if logfile is not None:
            already_exists = False
            if os.path.exists(logfile):
                already_exists = True
            logfile = open(logfile, "a")
            if already_exists:
                logfile.write("\n{0}\n Another run\n{0}\n".format('='*79))
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
            lines = proc.stdout.readline().decode('utf-8')
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
        """Create a SUCCESS file in directory of the current test"""
        fn = os.path.join(self.tdir, "SUCCESS")
        open(fn, "w")

    def copy_pkg(self, name, dst):
        """Copy package from testdata into specified destination"""
        src = os.path.join(PACKAGESDIR, name)
        shutil.copy(src, dst)
        return os.path.join(dst, name)

    def indir_addpkg(self, name):
        """Add package into input dir for the current test"""
        src = os.path.join(PACKAGESDIR, name)
        return self.copy_pkg(src, self.indir)

    def indir_makedirs(self, path):
        """Make a directory in input dir of the current test"""
        path = path.lstrip('/')
        final_path = os.path.join(self.indir, path)
        os.makedirs(final_path)
        return final_path

    def indir_mkfile(self, name, content=""):
        """Create a file in input dir of the current test"""
        fn = os.path.join(self.indir, name)
        with open(fn, "w") as f:
            f.write(content)
        return fn

    def tdir_makedirs(self, path):
        """Make a directory in test dir of the current test"""
        path = path.lstrip('/')
        final_path = os.path.join(self.tdir, path)
        os.makedirs(final_path)
        return final_path

    def run_prog(self, prog, dir, args=None, outdir=None):
        res = CrResult()
        res.dir = dir
        res.prog = prog
        res.outdir = outdir
        res.logfile =  os.path.join(self.tdir, "out_%s" % res.prog)
        res.cmd = "%(prog)s --verbose %(args)s %(dir)s" % {
            "prog": res.prog,
            "dir": res.dir,
            "args": args or "",
        }
        res.rc, res.out = self.runcmd(res.cmd, logfile=res.logfile)
        return res

    def run_cr(self, dir, args=None, c=False, outdir=None):
        """Run createrepo and return CrResult object with results

        :returns: Result of the createrepo run
        :rtype: CrResult
        """
        prog = "createrepo_c" if c else "createrepo"

        if not outdir:
            outdir = os.path.join(self.tdir, prog)
        else:
            outdir = os.path.join(self.tdir, outdir)
        if not os.path.exists(outdir):
            os.mkdir(outdir)

        args = " -o %s %s" % (outdir, args if args else "")

        res = self.run_prog(prog, dir, args, outdir)
        return res

    def run_sqlr(self, dir, args=None):
        """"""
        res = self.run_prog("sqliterepo_c", dir, args)
        return res

    def compare_repos(self, repo1, repo2):
        """Compare two repos

        :returns: Difference between two repositories
        :rtype: RepoDiffResult
        """
        res = RepoDiffResult()
        res.repo1 = os.path.join(repo1, "repodata")
        res.repo2 = os.path.join(repo2, "repodata")
        res.logfile = os.path.join(self.tdir, "out_cmp")
        res.cmd = "yum-metadata-diff --verbose --compare %s %s" % (res.repo1, res.repo2)
        res.rc, res.out = self.runcmd(res.cmd, logfile=res.logfile)
        return res

    def check_repo_sanity(self, repo):
        """Check if a repository is sane

        :returns: Result of the sanity check
        :rtype: RepoSanityCheckResult
        """
        res = RepoSanityCheckResult()
        res.repo = os.path.join(repo, "repodata")
        res.logfile = os.path.join(self.tdir, "out_sanity")
        res.cmd = "yum-metadata-diff --verbose --check %s" % res.repo
        res.rc, res.out = self.runcmd(res.cmd, logfile=res.logfile)
        return res

    def assert_run_cr(self, *args, **kwargs):
        """Run createrepo and assert that it finished with return code 0

        :returns: Result of the createrepo run
        :rtype: CrResult
        """
        res = self.run_cr(*args, **kwargs)
        self.assertFalse(res.rc)
        return res

    def assert_run_sqlr(self, *args, **kwargs):
        """Run sqliterepo and assert that it finished with return code 0

        :returns: Result of the sqliterepo run
        :rtype: CrResult
        """
        res = self.run_sqlr(*args, **kwargs)
        self.assertFalse(res.rc)
        return res

    def assert_same_results(self, indir, args=None):
        """Run both createrepo and createrepo_c and assert that results are same

        :returns: (result of comparison, createrepo result, createrepo_c result)
        :rtype: (RepoDiffResult, CrResult, CrResult)
        """
        crres = self.run_cr(indir, args)
        crcres = self.run_cr(indir, args, c=True)
        self.assertFalse(crres.rc)   # Error while running createrepo
        self.assertFalse(crcres.rc)  # Error while running createrepo_c
        cmpres = self.compare_repos(crres.outdir, crcres.outdir)
        self.assertFalse(cmpres.rc)  # Repos are not same
        return (cmpres, crres, crcres)

    def assert_repo_sanity(self, repo):
        """Run repo sanity check and assert it's sane

        :returns: Result of the sanity check
        :rtype: RepoSanityCheckResult
        """
        res = self.check_repo_sanity(repo)
        self.assertFalse(res.rc)
        return res

    def assert_repo_files(self, repo, file_patterns, additional_files_allowed=True):
        """Assert that files (defined by re) are in the repo
        """
        compiled_patterns = map(re.compile, file_patterns)
        fns = os.listdir(os.path.join(repo, "repodata/"))
        used_patterns = []
        for pattern in compiled_patterns:
            for fn in fns[:]:
                if pattern.match(fn):
                    fns.remove(fn)
                    used_patterns.append(pattern)

        if not additional_files_allowed:
            self.assertEqual(fns, [])  # Unexpected additional files

        unused_paterns = [x.pattern for x in (set(compiled_patterns) - set(used_patterns))]
        self.assertEqual(unused_paterns, [])  # Some patterns weren't used

    def assert_same_dir_content(self, a, b):
        """Assert identical content of two directories
        (Not recursive yet)
        """
        # TODO: Recursive
        self.assertTrue(os.path.isdir(a))
        self.assertTrue(os.path.isdir(b))

        _, logfn = tempfile.mkstemp(prefix="out_dircmp_%s_" % long(time.time()), dir=self.tdir)
        logfile = open(logfn, "w")
        logfile.write("A: %s\n" % a)
        logfile.write("B: %s\n" % b)
        logfile.write("\n")

        dircmpobj = filecmp.dircmp(a, b)
        if dircmpobj.left_only or dircmpobj.right_only or dircmpobj.diff_files:
            logfile.write("A only:\n%s\n\n" % dircmpobj.left_only)
            logfile.write("B only:\n%s\n\n" % dircmpobj.right_only)
            logfile.write("Diff files:\n%s\n\n" % dircmpobj.diff_files)
            logfile.close()
            self.assertTrue(False)
        logfile.write("OK\n")
        logfile.close()
