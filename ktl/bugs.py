#!/usr/bin/env python
#

import re
from ktl.kernel                         import map_release_number_to_ubuntu_release as ubuntu_series_db
from ktl.kernel                         import map_kernel_version_to_ubuntu_release as ubuntu_version_db
from ktl.utils                          import stdo

# DeltaTime
#
class DeltaTime():
    # __init__
    #
    def __init__(self, date, now):
        delta = now - date.replace(tzinfo=None)
        seconds_diff = (delta.days * 86400) + delta.seconds

        sec_p_hr = 60 * 60
        self.__hours = delta.seconds / sec_p_hr
        dhs = self.__hours * sec_p_hr
        remaining_seconds = delta.seconds - dhs

        self.__minutes = (delta.seconds - dhs) / 60
        dhm = self.__minutes * 60
        self.__seconds = remaining_seconds - dhm

        if delta.days < 1:
            self.__days = 0
        else:
            self.__days = delta.days

        return

    @property
    def days(self):
        return self.__days

    @property
    def hours(self):
        return self.__hours

    @property
    def minutes(self):
        return self.__minutes

    @property
    def seconds(self):
        return self.__seconds

# Bugs
#
class Bugs():
    """
    A class for collecting methods that may be used by multiple scripts that
    work on Launchpad bugs.
    """

    verbose = False

    @classmethod
    def vout(cls, msg):
        if cls.verbose:
            stdo(msg)

    # has_required_logs
    #
    @classmethod
    def has_required_logs(cls, bug):
       """
       Examines a bug and determines if it has all the logs that the kernel
       team requires.
       """
       return False

    # calculate_bug_gravity
    #
    @classmethod
    def calculate_bug_gravity(cls, bug, now):
        gravity = 0

        # Calculate a value based on how long before now the bug was created. The longer
        # ago, the lower the value.
        #
        ago = DeltaTime(bug.date_created, now)
        if   ago.days <  7: gravity += 1000
        elif ago.days < 14: gravity += 500
        elif ago.days < 21: gravity += 250
        elif ago.days < 30: gravity += 100

        ago = DeltaTime(bug.date_last_message, now)
        if   ago.days <  7: gravity += 1000
        elif ago.days < 14: gravity += 500
        elif ago.days < 21: gravity += 250
        elif ago.days < 30: gravity += 100

        return gravity

    # ubuntu_series_lookup
    #
    # Map from a kernel version to the name of a ubuntu release, this is used to
    # tag bugs.
    #
    @classmethod
    def ubuntu_series_lookup(cls, version):
        cls.vout(' . Looking up the series name for (%s)\n' % version)
        series_name = ''
        series_version = ''
        if series_name == '':
            m = re.search('([0-9]+)\.([0-9]+)\.([0-9]+)\-([0-9]+)\-(.*?)', version)
            if m is not None:
                kver = "%s.%s.%s" % (m.group(1), m.group(2), m.group(3))
                if kver in ubuntu_version_db:
                    series_name    = ubuntu_version_db[kver]['name']
                    series_version = ubuntu_version_db[kver]['number']
                    cls.vout('    - found kernel version in the db\n')
            else:
                cls.vout('    - didn\'t match kernel version pattern\n')

        if series_name == '':
            m = re.search('([0-9+)\.([0-9]+)', version)
            if m is not None:
                dnum = m.group(1)
                if dnum in ubuntu_series_db:
                    series_name   = ubuntu_series_db[dnum]['name']
                    series_version = dnum
                    cls.vout('    - found series version in the db\n')
            else:
                cls.vout('    - didn\'t match series version pattern\n')

        cls.vout('    - returning (%s)\n' % series_name)
        return (series_name, series_version)

    @classmethod
    def ubuntu_series_version_lookup(cls, series_name):
        cls.vout(' . Looking up the series version for (%s)\n' % series_name)
        retval = ''
        for version in ubuntu_version_db:
            if series_name == ubuntu_version_db[version]['name']:
                retval = version
                break
        return retval

    # find_distro_in_title
    #
    # Scan title for a pattern that looks like a distro name or version, and
    # return the newest release version found.
    #
    @classmethod
    def find_distro_in_title(cls, ars_bug):
        results = []
        for rel_num, rel in ubuntu_series_db.iteritems():
            pat = "(%s|[^0-9\.\-]%s[^0-9\.\-])" %(rel['name'], rel_num.replace(".", "\."))
            regex = re.compile(pat, re.IGNORECASE)
            if regex.search(ars_bug.title):
                results.append(rel['name'])
        return results

    # find_series_in_description
    #
    @classmethod
    def find_series_in_description(cls, bug):
        """
        Look in the bugs description to see if we can determine which distro the
        the user is running (hardy/intrepid/jaunty/karmic/lucid/etc.).
        """
        cls.vout(' . Looking for the series in the description\n')
        series_name = ''
        series_version = ''

        desc_lines = bug.description.split('\n')
        for line in desc_lines:
            # Sometimes there is a "DistroRelease:" line in the description.
            #
            m = re.search('DistroRelease:\s*(.*)', line)
            if m is not None:
                (series_name, series_version) = cls.ubuntu_series_lookup(m.group(1))
                break

            # Sometimes there is the results of 'uname -a' or a dmesg in
            # the description.
            #
            m = re.search('Linux version ([0-9]+)\.([0-9]+)\.([0-9]+)\-([0-9]+)\-(.*?) .*', line)
            if m is not None:
                kernel_version = "%s.%s.%s-%s-%s" % (m.group(1), m.group(2), m.group(3), m.group(4), m.group(5))
                (series_name, series_version) = cls.ubuntu_series_lookup(kernel_version)
                break

            if 'Description:' in line:
                m = re.search('Description:\s*([0-9]+\.[0-9]+)', line)
                if m is not None:
                    (series_name, series_version) = cls.ubuntu_series_lookup(m.group(1))
                    break

            if 'Release:' in line:
                m = re.search('Release:\s+([0-9]+\.[0-9]+)', line)
                if m is not None:
                    (series_name, series_version) = cls.ubuntu_series_lookup(m.group(1))
                    break

            # Sometimes it's just in the description
            #
            m = re.search('Ubuntu ((hardy|intrepid|jaunty|karmic|lucid|maverick|natty|oneiric)) [0-9]+\.[0-9]+', line)
            if (m != None):
                series_name = m.group(1)
                series_version = cls.ubuntu_series_version_lookup(series_name)

        if series_name == '':
            cls.vout('     - Not found\n')
        return (series_name, series_version)

    # find_distro_in_attachments
    #
    # Look through the various files attached to the bug, by the original
    # submitter/owner and see if we can determine the distro from there.
    #
    @classmethod
    def find_distro_in_attachments(cls, bug):
        cls.vout(' . Looking for the series in the attachments\n')
        series_name = ''
        series_version = ''
        kernel_version = ''

        try:
            owner = bug.owner
            if owner is None:
                raise # Will get eaten at the bottom

            owner = bug.owner.display_name.encode('utf-8')
            for attachment in bug.bug.attachments_collection:
                try:
                    # Short circuit the loop, if the attachment isn't from the bug
                    # submitter, we don't really care.
                    #
                    if (attachment.message.owner.display_name.encode('utf-8') != owner):
                        continue

                    # Dmesg.txt / dmesg.log
                    #
                    m = re.search('[Dd]mesg[.txt|.log]*', attachment.title)
                    if m != None:
                        cls.vout('     - Dmesg.log\n')
                        for line in attachment.data:
                            m = re.search('Linux version ([0-9]+)\.([0-9]+)\.([0-9]+)\-([0-9]+)\-(.*?) .*', line)
                            if (m != None):
                                cls.vout('       - found\n')
                                kernel_version = "%s.%s.%s-%s-%s" % (m.group(1), m.group(2), m.group(3), m.group(4), m.group(5))
                                break
                        if kernel_version != '':
                            (series_name, series_version) = cls.ubuntu_series_lookup(kernel_version)
                            break

                    # alsa-info
                    #
                    if series_name == '':
                        if 'alsa-info' in attachment.title:
                            cls.vout('     - alsa-info.log\n')
                            for line in attachment.data:
                                m = re.search('Kernel release:\s+([0-9]+)\.([0-9]+)\.([0-9]+)\-([0-9]+)\-(.*?)', line)
                                if (m != None):
                                    cls.vout('       - found\n')
                                    kernel_version = "%s.%s.%s-%s-%s" % (m.group(1), m.group(2), m.group(3), m.group(4), m.group(5))
                                    break
                            if kernel_version != '':
                                (series_name, series_version) = cls.ubuntu_series_lookup(kernel_version)
                                break

                    # xorg.0.log
                    #
                    if series_name == '':
                        m = re.search('[Xx]org\.0\.log.*', attachment.title)
                        if m != None:
                            cls.vout('     - Xorg.0.log\n')
                            for line in attachment.data:
                                m = re.search('Linux ([0-9]+)\.([0-9]+)\.([0-9]+)\-([0-9]+)\-(.*?) .*', line)
                                if (m != None):
                                    cls.vout('       - found\n')
                                    kernel_version = "%s.%s.%s-%s-%s" % (m.group(1), m.group(2), m.group(3), m.group(4), m.group(5))
                                    break
                            if kernel_version != '':
                                (series_name, series_version) = cls.ubuntu_series_lookup(kernel_version)
                                break

                except:
                    pass # Just eat any exceptions
        except:
            pass # Just eat any exceptions

        return (series_name, series_version)

    # determine_series
    #
    # Try to figure out which distro version the bug submitter is running
    # and has file the bug against.
    #
    @classmethod
    def determine_series(cls, bug):
        result = ''
        (series_name, series_version) = cls.find_series_in_description(bug)

        if (series_name == ''):
            (series_name, series_version) = cls.find_distro_in_attachments(bug)

        return (series_name, series_version)

# vi:set ts=4 sw=4 expandtab:
