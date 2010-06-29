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
from cvetracker_lib import *
import cve_lib

#------------------------------------------------------------------------------
# A work item is either a CVE or a bug which is considered a security related
# issue. Each work item can have a set of packages that are affected.
#------------------------------------------------------------------------------
class DebianPackage:
	"""Class to store per package information"""
	name = ""
	family = ""
	version = dict()

class WorkItem:
	"""This class stores information about a work item (bug or CVE)"""
	rawdata = dict()
	packages = dict()
	valid = False
	msg = []

	def _key_to_records(self, data, key):
		if data.has_key(key):
			if type(data[key]) == type([]):
				r = [key + ":"]
				for line in data[key]:
					r.append(" " + line)
			else:
				if data[key]:
					r = [key + ": " + data[key]]
				else:
					r = [key + ":"]
		else:
			r = None
		return r
	def __init__(self, filename=None):
		"""Creates a new work item, optionally loading from file"""
		if filename:
			self.load(filename)
	def __str__(self):
		"""Convert the class into string representation"""
		s =  "Class WorkItem(\"{0}\")\n".format(self.get_candidate())
		s += str(self.rawdata) + "\n"
		s += str(self.msg) + "\n"
		s += str(self.packages)
		return s
	def list_records(self):
		"""Convert the class into records"""
		header_fields = [
			"PublicDateAtUSN", "Candidate", "PublicDate",
			"References", "Description", "Ubuntu-Description",
			"Notes", "Bugs", "Priority", "Discovered-by",
			"Assigned-to"
		]
		l = []
		data = self.rawdata
		for key in header_fields:
			item = self._key_to_records(data, key)
			if item:
				l.extend(item)

		data = self.packages
		for pkg in data.keys():
			l.append("")
			for key in ["Patches", "Tags", "Priority"]:
				if not data[pkg].has_key(key):
					continue
				field = key + "_" + pkg + ":"
				if type(data[pkg][key]) == type([]):
					l.append(field)
					for line in data[pkg][key]:
						l.append(" " + line)
				else:
					value = data[pkg][key]
					l.append(field + " " + value)
			for key in ["upstream"] + cve_lib.releases + ["devel"]:
				if not data[pkg]["status"].has_key(key):
					continue
				field = key + "_" + pkg + ": "
				value = data[pkg]["status"][key]
				l.append(field + value)
		return l
	def get_candidate(self):
		if self.rawdata.has_key("Candidate"):
			return self.rawdata["Candidate"]
		else:
			return None
	def set_candidate(self, name):
		"""Sets the candidate field"""
		valid_prefixes = [ "CVE-", "UEM-", "EMB-", "BUG-" ]
		if name[0:4] in valid_prefixes:
			self.candidate = name
			return
		err = "{0} is not a valid candidate name ".format(name)
		err += "(must start with {0})".format(valid_prefixes)
		raise NameError, err
	def get_assignee(self):
		if self.rawdata.has_key("Assigned-to"):
			return self.rawdata["Assigned-to"]
		else:
			return None
	def set_assignee(self, name):
		self.rawdata["Assigned-to"] = name
	def get_priority(self):
		"""Get the priority of the item"""
		# For now, throw away comments on it
		if self.rawdata.has_key("Priority"):
			return self.rawdata["Priority"].split()[0]
		else:
			return None
	def get_status(self, pkg, series):
		if not self.packages.has_key(pkg):
			return "DNE"
		if series == cve_lib.devel_release:
			series = "devel"
		if self.packages[pkg]["status"].has_key(series):
			return self.packages[pkg]["status"][series].split()[0]
		else:
			return "DNE"
	def set_status(self, pkg, series, status):
		if series == cve_lib.devel_release:
			series = "devel"
		if self.packages[pkg]["status"].has_key(series):
			st = self.packages[pkg]["status"][series]
			if " " in st:
				st = status + " " + st.split(" ", 1)[1]
			else:
				st = status
		else:
			st = status
		self.packages[pkg]["status"][series] = st
	def list_packages(self):
		return sorted(self.packages.keys())
	def validate(self):
		self.msg = []
		valid = True
		for field in ["Candidate", "PublicDate", "Description"]:
			if not self.rawdata.has_key(field):
				err = "{0} undefined".format(field)
				self.msg.append(err)
				valid = False
			elif len(self.rawdata[field]) == 0:
				err = "{0} unset".format(field)
				self.msg.append(err)
				valid = False
		self.valid = valid

		return valid
	def save(self, filename):
		if not self.validate():
			return False
		try:
			ofile = open(filename, "w")
			for line in self.list_records():
				ofile.write(line + "\n")
			ofile.close()
		except:
			return False
		else:
			return True
	def load(self, filename):
		"""Loads the contents of a file and populates the class"""
		immediate_fields = [
			"Assigned-to", "PublicDate", "PublicDateAtUSN", "CRD",
			"Discovered-by", "Candidate", "Priority", "Tags"
		]
		list_fields = [
			"Description", "Ubuntu-Description", "References",
			"Bugs", "Notes", "Patches"
		]
		fields_seen = []
		field = None
		tgt = None

		if not os.path.exists(filename):
			raise ValueError, "File does not exist: %s" % (filename)
		for line in file(filename).readlines():
			line = line.rstrip()

			#------------------------------------------------------
			# Ignore blank or commened lines
			#------------------------------------------------------
			if len(line) == 0 or line.startswith("#"):
				continue

			#------------------------------------------------------
			# Lines starting with a blank belong to the field
			# seen last.
			#------------------------------------------------------
			if line.startswith(" "):
				if not tgt or not tgt.has_key(field):
					print "bad line: %s" % (line)
					continue

				tgt[field].append(line[1:])
				continue

			#------------------------------------------------------
			# Field names are before the first ":", the values
			# after it
			#------------------------------------------------------
			try:
				field, value = line.split(":", 1)
			except ValueError, e:
				print "bad line: %s" % (line)
				continue
			field = field.strip()
			if field in fields_seen:
				print "dup: %s" % (field)
			else:
				fields_seen.append(field)
			value = value.strip()

			#------------------------------------------------------
			# If the name of the field contains an underscore, it
			# is a <field>_<package> combination.
			#------------------------------------------------------
			if "_" in field:
				field, pkg = field.split("_", 1)
				self.packages.setdefault(pkg, dict())
				tgt = self.packages[pkg]
			else:
				tgt = self.rawdata

			if field in list_fields:
				#----------------------------------------------
				# List fields start as a single line, the
				# values are added by later entries.
				#----------------------------------------------
				tgt.setdefault(field, [])
				continue
			elif field in immediate_fields:
				#----------------------------------------------
				# Immediate fields have their value in th same
				# line.
				#----------------------------------------------
				tgt[field] = value
			elif field in cve_lib.releases + ["upstream", "devel"]:
				#----------------------------------------------
				# Special status fields <release>_<package>
				# with the value following immediately.
				#----------------------------------------------
				if tgt == self.rawdata:
					print "need a package for %s" % (field)
					continue
				tgt.setdefault("status", dict())
				tgt["status"][field] = value
			else:
				print "unknown field: {0}".format(field)

		self.validate()

def WorkItemReleaseName():
	return "release"

def WorkItemNameValid(name):
	valid = 0
	if name.startswith("CVE-"):
		if re.match("CVE-[0-9]{4}-[0-9]{4}$", name):
			valid = 1
	elif name.startswith("BUG-"):
		if re.match("BUG-[0-9]+$", name):
			valid = 1
	elif name == WorkItemReleaseName():
		valid = 1

	return valid
	
