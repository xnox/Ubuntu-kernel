#!/usr/bin/env python
#

from ktl.git                            import Git, GitError
from ktl.ubuntu                         import Ubuntu

#
# Warning - using the following dictionary to get the series name from the kernel version works for the linux package,
# but not for some others (some ARM packages are known to be wrong). This is because the ARM kernels used for some
# series are not the same as the main kernels
#

# KernelError
#
class KernelError(Exception):
    # __init__
    #
    def __init__(self, error):
        self.msg = error

# Kernel
#
class Kernel:
    debug               = False
    __makefile_contents = ''
    __version           = ''   # kernel version
    __series_version    = ''
    __series_name       = ''

    # version
    #
    @classmethod
    def version(cls):
        cls.__fetch_if_needed()
        return cls.__version

    # series_name
    #
    @classmethod
    def series_name(cls):
        cls.__fetch_if_needed()
        return cls.__series_name

    # release
    #
    @classmethod
    def release(cls):
        return cls.series_name()

    # __fetch_if_needed
    #
    @classmethod
    def __fetch_if_needed(cls):
        current_branch = Git.current_branch()
        version        = ''
        patchlevel     = ''
        sublevel       = ''
        extraversion   = ''
        if cls.__makefile_contents == '':
            try:
                cl_path = 'Makefile'
                cls.__makefile_contents = Git.show(cl_path, branch=current_branch)

                for line in cls.__makefile_contents:
                    # Ths only works because we know what the first 4 lines of the Makefile
                    # should be.
                    #
                    if version == '':
                        if 'VERSION' in line:
                            variable, value = line.split('=')
                            version = value.strip()
                            continue
                    elif patchlevel == '':
                        if 'PATCHLEVEL' in line:
                            variable, value = line.split('=')
                            patchlevel = value.strip()
                            continue
                    elif sublevel == '':
                        if 'SUBLEVEL' in line:
                            variable, value = line.split('=')
                            sublevel = value.strip()
                            continue
                    elif extraversion == '':
                        if 'EXTRAVERSION' in line:
                            variable, value = line.split('=')
                            extraversion = value.strip()
                            if extraversion == '':
                                # not used, just need to set something to continue
                                extraversion = 'XXX'
                            # for 3.0 and after, sublevel takes the meaning of what was extraversion
                            if int(version) >= 3:
                                sublevel = '0'
                            cls.__version = version + '.' + patchlevel + '.' + sublevel

                            ubuntu = Ubuntu()
                            rec = ubuntu.lookup(cls.__version)
                            cls.__series_name    = rec['name']
                            cls.__series_version = rec['series_version']
                            if cls.__series_name == '':
                                raise KernelError("Failed to find a series with the given kernel version.")
                        else:
                            raise KernelError("The makefile didn't contain the expected 4 lines.")
                    else:
                        break

            except GitError:
                raise KernelError('Failed to find the makefile.')

        return


# vi:set ts=4 sw=4 expandtab:
