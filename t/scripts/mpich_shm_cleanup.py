#!/usr/bin/env python3
#
# Remove shared memory that MPICH leaked and nothing is using.
#
# MPICH 5.0's ch4 device names its node-local segments
#   /dev/shm/mpich_shm_<id>_<nodeid> and /dev/shm/mpich_vci_<id>_<nodeid>
# where <id> is a hash of the PMI namespace, and unlinks them only in
# MPI_Finalize, so every job that aborts leaks them (pmodels/mpich#8148).
# The flux-pmix namespace is the jobid, and jobids are not unique across
# Flux instances, so a leaked segment can be opened by a later job in
# another instance that hashes to the same name, which then hangs in
# MPI_Init.  (MPICH 4.3 uses unique mkstemp() names instead and unlinks
# its init segment right after the init barrier, so it leaves nothing
# for this script to find.)
#
# For the same reason a segment cannot be matched to a job by name: a
# job in another instance running at the same time may own a segment
# with that name.  Instead, remove a segment only if no process has it
# open or mapped, as the kernel sees it.  A write lease (F_SETLEASE) is
# granted only when no other open file description refers to the file,
# and a mapping holds one, so the check covers processes this script
# cannot see in /proc, such as those in another PID namespace sharing
# /dev/shm.  The segment is unlinked while the lease is held, so no
# process can be using it at that moment.  A process that opens the
# name during that window blocks on the lease and then gets the
# unlinked segment, but it can only be a job colliding with a leaked
# segment, which hangs in MPI_Init either way: MPICH creates with
# O_EXCL, so a job opens an existing segment only as a non-root rank.
# If a lease cannot be taken for any reason other than "in use", the
# segment is left alone.
#
# Only segments owned by this user are considered (a lease requires
# ownership, and MPICH creates them mode 0600).  Always exit 0: cleanup
# must not fail a test.

import errno
import fcntl
import os
import re
import signal
import sys

SHM_DIR = "/dev/shm"
PATTERN = re.compile(r"^mpich_(shm|vci)_[0-9a-f]+_[0-9]+$")

# Python added fcntl.F_SETLEASE in 3.9; the value is from <fcntl.h>.
F_SETLEASE = getattr(fcntl, "F_SETLEASE", 1024)


def candidates():
    """Return paths of this user's MPICH segments."""
    uid = os.getuid()
    paths = []
    for name in os.listdir(SHM_DIR):
        if not PATTERN.match(name):
            continue
        path = os.path.join(SHM_DIR, name)
        try:
            if os.stat(path).st_uid == uid:
                paths.append(path)
        except FileNotFoundError:
            pass
    return sorted(paths)


def remove_if_unused(path):
    """Unlink path if no other process has it open or mapped.

    Return True if removed, False if in use or already gone.  Raise
    OSError if it cannot be told whether the segment is in use.
    """
    try:
        fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW)
    except FileNotFoundError:
        return False
    try:
        try:
            fcntl.fcntl(fd, F_SETLEASE, fcntl.F_WRLCK)
        except OSError as e:
            if e.errno in (errno.EAGAIN, errno.EBUSY):
                return False
            raise
        try:
            os.unlink(path)
        except FileNotFoundError:
            return False
        finally:
            fcntl.fcntl(fd, F_SETLEASE, fcntl.F_UNLCK)
        return True
    finally:
        os.close(fd)


def main():
    # A lease break signals the holder with SIGIO, whose default action
    # is to terminate.  The lease is released right after the unlink, so
    # the signal carries nothing this script needs.
    signal.signal(signal.SIGIO, signal.SIG_IGN)
    removed = 0
    skipped = 0
    for path in candidates():
        try:
            if remove_if_unused(path):
                removed += 1
        except OSError as e:
            skipped += 1
            print(f"mpich_shm_cleanup: {path}: {e.strerror}", file=sys.stderr)
    if removed > 0:
        print(f"mpich_shm_cleanup: removed {removed} unused segment(s)")
    if skipped > 0:
        print(
            f"mpich_shm_cleanup: left {skipped} segment(s) that could not"
            " be checked",
            file=sys.stderr,
        )


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"mpich_shm_cleanup: {e}", file=sys.stderr)
    sys.exit(0)
