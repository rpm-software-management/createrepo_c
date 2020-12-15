/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2015  Tomas Mlcoch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "error.h"
#include "misc.h"
#include "cleanup.h"

char *global_lock_dir     = NULL;  // Path to .repodata/ dir that is used as a lock
char *global_tmp_out_repo = NULL;  // Path to temporary repodata directory,
                                   // if NULL that it's same as
                                   // the global_lock_dir

/**
 * Clean up function called on normal program termination.
 * It removes temporary .repodata/ directory that servers as a lock
 * for other createrepo[_c] processes.
 * This functions acts only if exit status != EXIST_SUCCESS.
 *
 */
static void
exit_cleanup()
{
    if (global_lock_dir) {
        g_debug("Removing %s", global_lock_dir);
        cr_remove_dir(global_lock_dir, NULL);
    }

    if (global_tmp_out_repo) {
        g_debug("Removing %s", global_tmp_out_repo);
        cr_remove_dir(global_tmp_out_repo, NULL);
    }
}

/** Signal handler
 * @param sig       Signal number
 */
static void
sigint_catcher(int sig)
{
    g_message("%s caught: Terminating...", strsignal(sig));
    exit(1);
}

gboolean
cr_set_cleanup_handler(const char *lock_dir,
                       const char *tmp_out_repo,
                       G_GNUC_UNUSED GError **err)
{
    assert(!err || *err == NULL);

    // Set global variables
    global_lock_dir     = g_strdup(lock_dir);
    if (g_strcmp0(lock_dir, tmp_out_repo))
        global_tmp_out_repo = g_strdup(tmp_out_repo);
    else
        global_tmp_out_repo = NULL;

    // Register on exit cleanup function
    if (atexit(exit_cleanup))
        g_warning("Cannot set exit cleanup function by atexit()");

    // Prepare signal handler configuration
    g_debug("Signal handler setup");
    struct sigaction sigact;
    sigact.sa_handler = sigint_catcher;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;

    // Handle signals that terminate (from the POSIX.1-1990)
    sigaction(SIGHUP, &sigact, NULL);
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGPIPE, &sigact, NULL);
    sigaction(SIGALRM, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGUSR1, &sigact, NULL);
    sigaction(SIGUSR2, &sigact, NULL);

    // Handle signals that terminate (from the POSIX.1-2001)
#ifdef SIGPOLL
    sigaction(SIGPOLL, &sigact, NULL);
#endif
    sigaction(SIGPROF, &sigact, NULL);
    sigaction(SIGVTALRM, &sigact, NULL);

    return TRUE;
}

gboolean
cr_unset_cleanup_handler(G_GNUC_UNUSED GError **err)
{
    g_free(global_lock_dir);
    global_lock_dir = NULL;
    g_free(global_tmp_out_repo);
    global_tmp_out_repo = NULL;

    return TRUE;
}

gboolean
cr_block_terminating_signals(GError **err)
{
    assert(!err || *err == NULL);

    sigset_t intmask;

    sigemptyset(&intmask);
    sigaddset(&intmask, SIGHUP);
    sigaddset(&intmask, SIGINT);
    sigaddset(&intmask, SIGPIPE);
    sigaddset(&intmask, SIGALRM);
    sigaddset(&intmask, SIGTERM);
    sigaddset(&intmask, SIGUSR1);
    sigaddset(&intmask, SIGUSR2);
#ifdef SIGPOLL
    sigaddset(&intmask, SIGPOLL);
#endif
    sigaddset(&intmask, SIGPROF);
    sigaddset(&intmask, SIGVTALRM);

    if (sigprocmask(SIG_BLOCK, &intmask, NULL)) {
        g_set_error(err, CREATEREPO_C_ERROR, CRE_SIGPROCMASK,
                    "Cannot block terminating signals: %s", g_strerror(errno));
        return FALSE;
    }

    return TRUE;
}

gboolean
cr_unblock_terminating_signals(GError **err)
{
    assert(!err || *err == NULL);

    sigset_t intmask;

    sigemptyset(&intmask);
    sigaddset(&intmask, SIGHUP);
    sigaddset(&intmask, SIGINT);
    sigaddset(&intmask, SIGPIPE);
    sigaddset(&intmask, SIGALRM);
    sigaddset(&intmask, SIGTERM);
    sigaddset(&intmask, SIGUSR1);
    sigaddset(&intmask, SIGUSR2);
#ifdef SIGPOLL
    sigaddset(&intmask, SIGPOLL);
#endif
    sigaddset(&intmask, SIGPROF);
    sigaddset(&intmask, SIGVTALRM);

    if (sigprocmask(SIG_UNBLOCK, &intmask, NULL)) {
        g_set_error(err, CREATEREPO_C_ERROR, CRE_SIGPROCMASK,
                    "Cannot unblock terminating signals: %s", g_strerror(errno));
        return FALSE;
    }

    return TRUE;
}

gboolean
cr_lock_repo(const gchar *repo_dir,
             gboolean ignore_lock,
             gchar **lock_dir_p,
             gchar **tmp_repodata_dir_p,
             GError **err)
{
    assert(!err || *err == NULL);

    _cleanup_free_ gchar *lock_dir = NULL;
    _cleanup_error_free_ GError *tmp_err = NULL;

    lock_dir = g_build_filename(repo_dir, ".repodata/", NULL);
    *lock_dir_p = g_strdup(lock_dir);

    if (g_mkdir(lock_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
        if (errno != EEXIST) {
            g_set_error(err, CREATEREPO_C_ERROR, CRE_IO,
                        "Error while creating temporary repodata "
                        "directory: %s: %s", lock_dir, g_strerror(errno));
            return FALSE;
        }

        g_debug("Temporary repodata directory: %s already exists! "
                "(Another createrepo process is running?)", lock_dir);

        if (ignore_lock == FALSE) {
            g_set_error(err, CREATEREPO_C_ERROR, CRE_IO,
                        "Temporary repodata directory %s already exists! "
                        "(Another createrepo process is running?)", lock_dir);
            return FALSE;
        }

        // The next section takes place only if the --ignore-lock is used
        // Ugly, but user wants it -> it's his fault if something gets broken

        // Remove existing .repodata/
        g_debug("(--ignore-lock enabled) Let's remove the old .repodata/");
        if (cr_rm(lock_dir, CR_RM_RECURSIVE, NULL, &tmp_err)) {
            g_debug("(--ignore-lock enabled) Removed: %s", lock_dir);
        } else {
            g_critical("(--ignore-lock enabled) Cannot remove %s: %s",
                       lock_dir, tmp_err->message);
            g_set_error(err, CREATEREPO_C_ERROR, CRE_IO,
                        "Cannot remove %s (--ignore-lock enabled) :%s",
                        lock_dir, tmp_err->message);
            return FALSE;
        }

        // Try to create own - just as a lock
        if (g_mkdir(lock_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
            g_critical("(--ignore-lock enabled) Cannot create %s: %s",
                       lock_dir, g_strerror(errno));
            g_set_error(err, CREATEREPO_C_ERROR, CRE_IO,
                        "Cannot create: %s (--ignore-lock enabled): %s",
                        lock_dir, g_strerror(errno));
            return FALSE;
        } else {
            g_debug("(--ignore-lock enabled) Own and empty %s created "
                    "(serves as a lock)", lock_dir);
        }

        // To data generation use a different one
        _cleanup_free_ gchar *tmp_repodata_dir = NULL;
        _cleanup_free_ gchar *tmp = NULL;

        tmp = g_build_filename(repo_dir, ".repodata.", NULL);
        tmp_repodata_dir = cr_append_pid_and_datetime(tmp, "/");

        if (g_mkdir(tmp_repodata_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
            g_critical("(--ignore-lock enabled) Cannot create %s: %s",
                       tmp_repodata_dir, g_strerror(errno));
            g_set_error(err, CREATEREPO_C_ERROR, CRE_IO,
                        "Cannot create: %s (--ignore-lock enabled): %s",
                        tmp_repodata_dir, g_strerror(errno));
            return FALSE;
        } else {
            g_debug("(--ignore-lock enabled) For data generation is used: %s",
                    tmp_repodata_dir);
        }

        *tmp_repodata_dir_p = g_strdup(tmp_repodata_dir);
    } else {
        *tmp_repodata_dir_p = g_strdup(lock_dir);
    }

    return TRUE;
}

void
cr_setup_logging(gboolean quiet, gboolean verbose)
{
    if (quiet) {
        // Quiet mode
        GLogLevelFlags hidden_levels = G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO |
                                       G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_WARNING;
        g_log_set_default_handler (cr_log_fn, GINT_TO_POINTER(hidden_levels));
    } else if (verbose) {
        // Verbose mode
        g_log_set_default_handler (cr_log_fn, GINT_TO_POINTER(0));
    } else {
        // Standard mode
        GLogLevelFlags hidden_levels = G_LOG_LEVEL_DEBUG;
        g_log_set_default_handler (cr_log_fn, GINT_TO_POINTER(hidden_levels));
    }
}
