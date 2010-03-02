#!/usr/bin/python
#==============================================================================
# Author: Stefan Bader <stefan.bader@canonical.com>
# Copyright (C) 2010
#
# This script is distributed under the terms and conditions of the GNU General
# Public License, Version 3 or later. See http://www.gnu.org/copyleft/gpl.html
# for details.
#==============================================================================
import sys, os, re
from subprocess import *
from zlib import crc32

class Patch:
	"""Class to store patch data into."""
	hash = 0
	series = None
	package = None
	filename = None
	content = []

	def __init__(self, filename):
		"""Create a new instance from a filename."""
		self.load(filename)

	def load(self, filename):
		"""Load the contents of a patch from a file."""
		try:
			ifile = open(filename, "r")
		except:
			return False

		self.hash = 0
		self.content = []
		self.filename = filename
		for line in ifile.readlines():
			self.content.append(line)

			#------------------------------------------------------
			# For the purpose of hash calculation, skip some lines
			# or replace things to be ignored.
			#------------------------------------------------------
			if line.startswith("From"):
				continue
			if line.startswith("index"):
				continue
			if line.startswith("@@"):
				line = re.sub("^@@.*@@", "", line)
			if re.search("\[PATCH [0-9]+\/[0-9]+\]", line):
				line = re.sub("\[PATCH .+\]", "[PATCH]", line)

			self.hash = crc32(line, self.hash)
		ifile.close()

		return True
