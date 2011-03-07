#!/usr/bin/env python
#

from ktl.git                            import Git, GitError

#
# Warning - using the following dictionary to get the series name from the kernel version works for the linux package,
# but not for some others (some ARM packages are known to be wrong). This is because the ARM kernels used for some
# series are not the same as the main kernels
#

map_kernel_version_to_ubuntu_release = {
    '2.6.38' : { 'number' : '11.04', 'name'  : 'natty', 'sha1' : '', 'md5' : '' },
    '2.6.35' : { 'number' : '10.10', 'name'  : 'maverick', 'sha1' : 'a2422f9281766ffe2f615903712819b9b0d9dd52', 'md5' : '62001687bd94d1c0dd9a3654c64257d6' },
    '2.6.32' : { 'number' : '10.04', 'name'  : 'lucid',    'sha1' : '298cbfdb55fc64d1135f06b3bed3c8748123c183', 'md5' : '4b1f6f6fac43a23e783079db589fc7e2' },
    '2.6.31' : { 'number' : '9.10',  'name'  : 'karmic',   'sha1' : '6b19c2987b0e2d74dcdca2aadebd5081bc143b72', 'md5' : '16c0355d3612806ef87addf7c9f8c9f9' },
    '2.6.28' : { 'number' : '9.04',  'name'  : 'jaunty',   'sha1' : '92d6a293200566646fbb9215e0633b4b9312ad38', 'md5' : '062c29b626a55f09a65532538a6184d4' },
    '2.6.27' : { 'number' : '8.10',  'name'  : 'intrepid', 'sha1' : '', 'md5' : '' },
    '2.6.24' : { 'number' : '8.04',  'name'  : 'hardy',    'sha1' : 'ccccdc4759fd780a028000a1b7b15dbd9c60363b', 'md5' : 'e4aad2f8c445505cbbfa92864f5941ab' },
    '2.6.22' : { 'number' : '7.10',  'name'  : 'gutsy',    'sha1' : '', 'md5' : '' },
    '2.6.20' : { 'number' : '7.04',  'name'  : 'feisty',   'sha1' : '', 'md5' : '' },
    '2.6.15' : { 'number' : '6.06',  'name'  : 'dapper',   'sha1' : '', 'md5' : '' }
}

map_release_number_to_ubuntu_release = {
    '11.04'  : { 'kernel' : '2.6.38', 'name' : 'natty', 'supported' : False,
                 # adjust packages when this goes live
                 'packages' : ['linux', 'linux-ti-omap4', 'linux-mvl-dove', 'linux-ec2',
                               'linux-meta', 'linux-ports-meta', 'linux-meta-ec2', 'linux-meta-mvl-dove', 'linux-meta-ti-omap4'
                               ]},
    '10.10'  : { 'kernel' : '2.6.35', 'name' : 'maverick', 'supported' : True,
                 'packages' : ['linux', 'linux-ti-omap4', 'linux-mvl-dove',
                               'linux-meta', 'linux-ports-meta', 'linux-meta-mvl-dove', 'linux-meta-ti-omap4',
                               'linux-backports-modules-2.6.35']},
    '10.04'  : { 'kernel' : '2.6.32', 'name' : 'lucid', 'supported' : True,
                 'packages' : ['linux', 'linux-fsl-imx51', 'linux-mvl-dove', 'linux-ec2',
                               'linux-meta', 'linux-ports-meta', 'linux-meta-ec2', 'linux-meta-mvl-dove', 'linux-meta-fsl-imx51',
                               'linux-backports-modules-2.6.32', 'linux-lts-backport-maverick', 'linux-meta-lts-backport-maverick' #, 'linux-lts-backport-natty'
                               ]},
    '9.10'   : { 'kernel' : '2.6.31', 'name' : 'karmic', 'supported' : True,
                 'packages' : ['linux', 'linux-fsl-imx51', 'linux-mvl-dove', 'linux-ec2',
                               'linux-meta', 'linux-ports-meta', 'linux-meta-ec2', 'linux-meta-mvl-dove', 'linux-meta-fsl-imx51',
                               'linux-backports-modules-2.6.31']},
    '9.04'   : { 'kernel' : '2.6.28', 'name' : 'jaunty', 'supported' : False, 'packages' : []},
    '8.10'   : { 'kernel' : '2.6.27', 'name' : 'intrepid', 'supported' : False, 'packages' : []},
    '8.04'   : { 'kernel' : '2.6.24', 'name' : 'hardy', 'supported' : True,
                 'packages' : ['linux',
                               'linux-meta', 'linux-backports-modules-2.6.24', 'linux-ubuntu-modules', 'linux-restricted-modules'
                               ]},
    '7.10'   : { 'kernel' : '2.6.22', 'name' : 'gutsy', 'supported' : False, 'packages' : []},
    '7.04'   : { 'kernel' : '2.6.20', 'name' : 'feisty', 'supported' : False, 'packages' : []},
    '6.06'   : { 'kernel' : '2.6.15', 'name' : 'dapper', 'supported' : True,
                 'packages' : ['linux-source-2.6.15', 'linux-backports-modules-2.6.15']},
}

kernel_package_names = [
    'linux',
    'linux-ti-omap4', # maverick, natty
    'linux-mvl-dove', # maverick, karmic, lucid
    'linux-fsl-imx51', # karmic, lucid
    'linux-ec2',
    'linux-meta',
    'linux-meta-ec2',
    'linux-meta-mvl-dove', # maverick, karmic, lucid
    'linux-meta-ti-omap4', # maverick, natty
    'linux-meta-fsl-imx51', # karmic, lucid ?
    'linux-ports-meta',
    'linux-source-2.6.15',
    'linux-backports-modules-2.6.15',
    'linux-backports-modules-2.6.31',
    'linux-backports-modules-2.6.32',
    'linux-restricted-modules-2.6.24',
    'linux-ubuntu-modules-2.6.24',
    'linux-backports-modules-2.6.35',
    'linux-lts-backport-maverick',
    'linux-lts-backport-natty',
    'linux-meta-lts-backport-maverick',
    'linux-meta-lts-backport-natty',
]

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
