#!/usr/bin/env python
#
# Checkbox Results Document

from couchdbkit.schema import Document
from couchdbkit.schema.properties import *

class CRDocument(Document):
    # What is happening that caused this document to be added
    # to the database? Is it an event such as SCALE or Atlanta
    # Linux Fest or something else?
    event = StringProperty()

    # A dictionary of test-name, succeed/failuer/skipped pairs
    #
    testResults = DictProperty()

# vi:set ts=4 sw=4 expandtab:
