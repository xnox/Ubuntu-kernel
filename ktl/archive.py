#!/usr/bin/env python
#

from   ktl.utils                import fileage
from   ktl.kernel               import *
from   launchpadlib.launchpad   import Launchpad   
from   urllib                   import urlopen
import json
import re

# KernelError
#
class KernelError(Exception):
    # __init__
    #
    def __init__(self, error):
        self.msg = error

# Kernel
#
class Archive:
    debug               = False

    # for Launchpad API
    cachedir = "/tmp/.launchpadlib/cache/"

    # How often should we fetch files? (seconds)
    #file_lifetime = 900 # 15 minutes
    file_lifetime = 60 # 1 minute (for testing)

    statuses = ['Pending', 'Published']

    # related to the kernel ppa
    __ppa_get_deleted       = False
    __ppa_get_obsolete      = False
    __ppa_get_superseded    = False
    ppa = None
    allppainfo = False
    teamname = 'canonical-kernel-team'
    ppaname = 'ppa'
    ppafilename = 'ckt-ppa.json'

    # related to the kernel distro archive
    __distro_get_deleted       = False
    __distro_get_obsolete      = False
    __distro_get_superseded    = False
    distro = None
    alldistroinfo = False
    distrofilename = 'distro-archive.json'

    # version
    #
    def ppa_versions(self, force = False):
        self.__fetch_ppa_if_needed(force)
        return self.ppa

    # version
    #
    def distro_versions(self, force = False):
        self.__fetch_distro_if_needed(force)
        return self.distro

    # __fetch_ppa_if_needed
    #
    def __fetch_ppa_if_needed(self, force):
        # see if there is a local json file that is recent
        # if there is, return the data
        age = fileage(self.ppafilename)

        if force or (not age) or (age > self.file_lifetime):
            # fetch from the PPA
            print 'Fetching from Launchpad'

            statuses = list(self.statuses)
            if self.__ppa_get_deleted:
                statuses.append('Deleted')
            if self.__ppa_get_obsolete:
                statuses.append('Obsolete')
            if self.__ppa_get_superseded:
                statuses.append('Superseded')

            jf = open(self.ppafilename, 'w+')

            outdict = {}
            lp = Launchpad.login_anonymously('kernel team tools', 'production', self.cachedir)
            person = lp.people[self.teamname]
            ppa = person.getPPAByName(name=self.ppaname)

            for astatus in statuses:
                psrc = ppa.getPublishedSources(status=astatus)
                for p in  psrc:
                    fd = urlopen(p.self_link)
                    sourceinfo = json.load(fd)

                    # Add some plain text fields for some info
                    sourceinfo['creator'] = sourceinfo['package_creator_link'].split('/')[-1].strip('~') 
                    sourceinfo['signer'] = sourceinfo['package_signer_link'].split('/')[-1].strip('~') 
                    rm = re.match('[0-9]\.[0-9]\.[0-9][0-9]', sourceinfo['source_package_version'])
                    version = rm.group(0)
                    sourceinfo['series'] = map_kernel_version_to_ubuntu_release[version]['name']
                    # And strip some things we don't care about
                    if not self.allppainfo:
                        for delkey in ['archive_link', 'distro_series_link', 'http_etag', 'package_maintainer_link', \
                                           'resource_type_link', 'package_creator_link', 'package_signer_link', \
                                           'section_name', 'scheduled_deletion_date', 'removal_comment', 'removed_by_link']:
                            del sourceinfo[delkey]

                    key = p.source_package_name + '-' + p.source_package_version
                    outdict[key] = sourceinfo
            jf.write(json.dumps(outdict, sort_keys=True, indent=4))
            jf.close()

            self.ppa = outdict
        else:
            # read from the local file
            f = open(self.ppafilename, 'r')
            # read it
            self.ppa = json.load(f)
            f.close()
        return


    # __fetch_distro_if_needed
    #
    def __fetch_distro_if_needed(self, force):
        # see if there is a local json file that is recent
        # if there is, return the data
        age = fileage(self.distrofilename)

        if force or (not age) or (age > self.file_lifetime):
            # fetch from the PPA
            print 'Fetching from Distro archives'

            statuses = list(self.statuses)
            if self.__distro_get_deleted:
                statuses.append('Deleted')
            if self.__distro_get_obsolete:
                statuses.append('Obsolete')
            if self.__distro_get_superseded:
                statuses.append('Superseded')

            jf = open(self.ppafilename, 'w+')

            lp = Launchpad.login_anonymously('kernel team tools', 'production', self.cachedir)
            outdict = {}

            # get a list of the series that we want info about
            #for key, info in map_release_number_to_ubuntu_release.items():
            #    if not info['supported']:
            #        continue

            #archive = lp.distributions['ubuntu'].getSeries(name_or_version=info['name']).main_archive
            archive = lp.distributions['ubuntu'].getArchive(name='primary')

            for astatus in statuses:
                for pname in kernel_package_names:
                    for key, release in map_release_number_to_ubuntu_release.items():
                        if not release['supported']:
                            if self.debug:
                                print 'DEBUG: Fetching from archive, skipping release ', release['name']
                            continue

                        series = lp.distributions['ubuntu'].getSeries(name_or_version=release['name'])
                        psrc = archive.getPublishedSources(status=astatus, exact_match = True, source_name = pname, distro_series = series)
                        #psrc = archive.getPublishedSources(status=astatus, exact_match = True, source_name = pname)
                        for p in  psrc:
                            fd = urlopen(p.self_link)
                            sourceinfo = json.load(fd)
                            #print json.dumps(sourceinfo, sort_keys=True, indent=4)

                            # Add some plain text fields for some info
                            field = sourceinfo['package_creator_link']
                            if field:
                                sourceinfo['creator'] = field.split('/')[-1].strip('~') 
                            else:
                                sourceinfo['creator'] = 'Unknown'
                            field = sourceinfo['package_signer_link']
                            if field:
                                sourceinfo['signer'] = field.split('/')[-1].strip('~') 
                            else:
                                sourceinfo['signer'] = 'Unknown'
                            rm = re.match('[0-9]\.[0-9]\.[0-9][0-9]', sourceinfo['source_package_version'])
                            version = rm.group(0)
                            try:
                                sourceinfo['series'] = map_kernel_version_to_ubuntu_release[version]['name']
                            except:
                                sourceinfo['series'] = 'Unknown'
                            # And strip some things we don't care about
                            if not self.allppainfo:
                                for delkey in ['archive_link', 'distro_series_link', 'http_etag', 'package_maintainer_link', \
                                                   'resource_type_link', 'package_creator_link', 'package_signer_link', \
                                                   'section_name', 'scheduled_deletion_date', 'removal_comment', 'removed_by_link']:
                                    del sourceinfo[delkey]

                            key = p.source_package_name + '-' + p.source_package_version
                            print '    found: ', key
                            outdict[key] = sourceinfo
            jf.write(json.dumps(outdict, sort_keys=True, indent=4))
            jf.close()
            self.ppa = outdict
        else:
            # read from the local file
            f = open(self.ppafilename, 'r')
            # read it
            self.ppa = json.load(f)
            f.close()
        return

#lp.distributions['ubuntu'].getSeries(name_or_version='maverick').main_archive

#(12:05:09 PM) pitti: >>> lp.distributions['ubuntu'].getSeries(name_or_version='maverick').main_archive.getPublishedSources(exact_match=True, source_name='linux')[0].source_package_version
#(12:05:11 PM) pitti: u'2.6.38-1.28'

#(12:07:39 PM) pitti: >>> maverick.main_archive.getPublishedSources(exact_match=True, source_name='linux', pocket='Updates')[0].source_package_version
#(12:07:39 PM) pitti: u'2.6.35-25.44'
#(12:07:41 PM) pitti: that seems to work

#(12:08:29 PM) pitti: and you have to specify distro_series as well
#(12:08:36 PM) pitti: >>> maverick.main_archive.getPublishedSources(exact_match=True, source_name='linux', pocket='Release', distro_series=maverick)[0].source_package_version
#(12:08:37 PM) pitti: u'2.6.35-22.33'
#(12:08:46 PM) pitti: >>> maverick.main_archive.getPublishedSources(exact_match=True, source_name='linux', pocket='Updates', distro_series=maverick)[0].source_package_version
#(12:08:47 PM) pitti: u'2.6.35-25.44'


# vi:set ts=4 sw=4 expandtab:
