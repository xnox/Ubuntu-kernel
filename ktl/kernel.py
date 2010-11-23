#!/usr/bin/env python
#

from ktl.git                            import Git, GitError
from re                                 import search

map_kernel_version_to_ubuntu_release = {
    '2.6.35' : { 'number' : '10.10', 'name'  : 'maverick' },
    '2.6.32' : { 'number' : '10.04', 'name'  : 'lucid'    },
    '2.6.31' : { 'number' : '9.10',  'name'  : 'karmic'   },
    '2.6.28' : { 'number' : '9.04',  'name'  : 'jaunty'   },
    '2.6.27' : { 'number' : '8.10',  'name'  : 'intrepid' },
    '2.6.24' : { 'number' : '8.04',  'name'  : 'hardy'    },
    '2.6.22' : { 'number' : '7.10',  'name'  : 'gutsy'    },
    '2.6.20' : { 'number' : '7.04',  'name'  : 'feisty'   }
}

map_release_number_to_ubuntu_release = {
    '10.10'  : { 'kernel' : '2.6.35', 'name' : 'maverick' },
    '10.04'  : { 'kernel' : '2.6.32', 'name' : 'lucid'    },
    '9.10'   : { 'kernel' : '2.6.31', 'name' : 'karmic'   },
    '9.04'   : { 'kernel' : '2.6.28', 'name' : 'jaunty'   },
    '8.10'   : { 'kernel' : '2.6.27', 'name' : 'intrepid' },
    '8.04'   : { 'kernel' : '2.6.24', 'name' : 'hardy'    },
    '7.10'   : { 'kernel' : '2.6.22', 'name' : 'gutsy'    },
    '7.04'   : { 'kernel' : '2.6.20', 'name' : 'feisty'   }
}

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
    __version           = ''
    __release           = ''

    # version
    #
    @classmethod
    def version(cls):
        cls.__fetch_if_needed()
        return cls.__version

    # release
    #
    @classmethod
    def release(cls):
        cls.__fetch_if_needed()
        return cls.__release

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
                    if version == '':
                        if 'VERSION' in line:
                            variable, value = line.split(' = ')
                            version = value
                            continue
                    elif patchlevel == '':
                        if 'PATCHLEVEL' in line:
                            variable, value = line.split(' = ')
                            patchlevel = value
                            continue
                    elif sublevel == '':
                        if 'SUBLEVEL' in line:
                            variable, value = line.split(' = ')
                            sublevel = value
                            continue
                    elif extraversion == '':
                        cls.__version = version.strip() + '.' + patchlevel.strip() + '.' + sublevel.strip()

                        if cls.__version in map_kernel_version_to_ubuntu_release:
                            cls.__release = map_kernel_version_to_ubuntu_release[cls.__version]['name']
                        break
                    else:
                        break

            except GitError:
                raise KernelError('Failed to find the makefile.')

        return


# vi:set ts=4 sw=4 expandtab:
