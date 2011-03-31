#!/usr/bin/env python
#

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

    # __init__
    #
    def __init__(self):
        return

    # has_required_logs
    #
    def has_required_logs(self):
       """
       Examines a bug and determines if it has all the logs that the kernel
       team requires.
       """
       return False

    # calculate_bug_gravity
    #
    @classmethod
    def calculate_bug_gravity(self, bug, now):
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

# vi:set ts=4 sw=4 expandtab:
