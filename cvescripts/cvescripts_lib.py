#!/usr/bin/python
#==============================================================================
# Author: Stefan Bader <stefan.bader@canonical.com>
# Copyright (C) 2010
#
# This script is distributed under the terms and conditions of the GNU General
# Public License, Version 3 or later. See http://www.gnu.org/copyleft/gpl.html
# for details.
#==============================================================================
import sys, os, ConfigParser, optparse
from subprocess import *
from getopt import *

cvescripts_cfg = os.path.expanduser(os.path.join('~', '.cvescripts.cfg'))
repo_host = "kernel.ubuntu.com"
http_host = "chinstrap.ubuntu.com"
repo_path = os.path.join("/home", "kernel-ppa", "security")

#------------------------------------------------------------------------------
# Little helper to remove quotes and expand the '~' constructs in paths.
#------------------------------------------------------------------------------
def ParsePath(path):
	if path[0] == '"' and path[-1] == '"':
		path = path[1:-1]
	if path[0] == '~':
		path = os.path.expanduser(path)
	return path

#------------------------------------------------------------------------------
# Read in the config file and set variables.
#------------------------------------------------------------------------------
# key, val = config.item("section")
# val = config.get("section", "name")
config = ConfigParser.ConfigParser()
try:
	config.read(cvescripts_cfg)
except:
	print "EE: Configuration file {0} not found!".format(cvescripts_cfg)
	sys.exit(1)
else:
	pass

#------------------------------------------------------------------------------
# One required entry in the "Paths" section is the location of the
# ubuntu-cve-tracker bazar branch.
#------------------------------------------------------------------------------
try:
	cvetracker_dir = ParsePath(config.get("Paths", "cvetracker"))
except:
	print "No {0} in {1}!".format("cvetracker", "Paths")
	sys.exit(1)
else:
	pass


#------------------------------------------------------------------------------
# There might or might not be a local clone of the upstream linux repo. This
# will be used to speed up cloning and decrease disk usage.
#------------------------------------------------------------------------------
try:
	kernel_reference = ParsePath(config.get("Paths", "kernelreference"))
except:
	kernel_reference = ""
else:
	pass

#------------------------------------------------------------------------------
# Set up login, ircnick, fullname and email for the current user.
#------------------------------------------------------------------------------
my_local_login = os.getlogin()
try:
	my_login = config.get(my_local_login, "login")
except:
	my_login = os.getlogin()
else:
	pass
try:
	my_ircnick = config.get(my_local_login, "ircnick")
except:
	my_ircnick = my_local_login
else:
	pass
try:
	my_fullname = config.get(my_local_login, "fullname")
except:
	my_fullname = os.getenv("DEBFULLNAME")
	if not my_fullname:
		print "EE: fullname is not set in config or environment"
		sys.exit(1)
else:
	pass
try:
	my_email = config.get(my_local_login, "email")
except:
	my_email = os.getenv("DEBEMAIL")
	if not my_email:
		print "EE: email is not set in config or environment"
		sys.exit(1)
else:
	pass

#------------------------------------------------------------------------------
# Libraries shared by all kteam-tools will be found here (../lib)
#------------------------------------------------------------------------------
cvescripts_common_lib = os.path.dirname(os.path.abspath(sys.argv[0]))
cvescripts_common_lib = os.path.dirname(cvescripts_common_lib)
cvescripts_common_lib = os.path.join(cvescripts_common_lib, "lib")
sys.path.insert(0, cvescripts_common_lib)
from git_lib import *

#------------------------------------------------------------------------------
# Now with the path to the ubuntu-cve-tracker we can import libraries from
# them.
#------------------------------------------------------------------------------
sys.path.insert(0, os.path.join(cvetracker_dir, "scripts"))
import cve_lib

#def ListRepos(series):
#	repos = []
#	path = os.path.join(repo_path, series)
#	cmd = "ssh {0} ls -1 {1}".format(repo_host, path)
#
#	p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
#	for line in p.stdout:
#		repos.append(line.strip())
#	p.stdout.close()
#	p.stderr.close()
#
#	return repos

def ListSupportedSeries():
	series = cve_lib.releases
	for eol in cve_lib.eol_releases:
		if eol in series:
			series.remove(eol)

	return series

#------------------------------------------------------------------------------
# PkgList is a dictionary containing per series information:
# PkgList[<series>][<package-shortname>] = <package-name>
#------------------------------------------------------------------------------
PkgList = dict()
PkgList["dapper"] = dict([
        ( "linux", "linux-source-2.6.15" ),
        ( "lbm", "linux-backports-modules-2.6.15" )
])
PkgList["hardy"] = dict([
	( "linux", "linux" ),
	( "lbm", "linux-backports-modules-2.6.24" ),
	( "lrm", "linux-restricted-modules-2.6.24" ),
	( "lum", "linux-ubuntu-modules-2.6.24" )
])
PkgList["intrepid"] = dict([
	( "linux", "linux" ),
	( "lbm", "linux-backports-modules-2.6.27" ),
	( "lrm", "linux-restricted-modules" )
])
PkgList["jaunty"] = dict([
	( "linux", "linux" ),
	( "lbm", "linux-backports-modules-2.6.28" ),
	( "lrm", "linux-restricted-modules" )
])
PkgList["karmic"] = dict([
	( "linux", "linux" ),
	( "lbm", "linux-backports-modules-2.6.31" ),
])
PkgList["lucid"] = dict([
	( "linux", "linux" ),
	( "lbm", "linux-backports-modules-2.6.32" )
])

#------------------------------------------------------------------------------
# Make sure the given directory exists. Bail out if it cannot be created.
#------------------------------------------------------------------------------
def AssertDir(dir):
	if not os.path.isdir(dir):
		print "LL: Create " + dir + " ...",
		try:
			os.makedirs(dir)
		except:
			print "failed!"
			sys.exit(1)
		else:
			print "ok"

#------------------------------------------------------------------------------
# Return the path for saving workitem data/patches. Make sure path does exist.
#------------------------------------------------------------------------------
def GetSaveDir(base, name):
	savedir = os.path.join(base, "workitems", name)
	AssertDir(savedir)

	return savedir

#------------------------------------------------------------------------------
# Return a list of all working repositories (all series, all packages)
#------------------------------------------------------------------------------
def ListWorkareas():
	list = []
	for series in ListSupportedSeries():
		if not series in PkgList.keys():
			continue
		for package in sorted(PkgList[series].keys()):
			list.append(os.path.join(series, package))

	return list

#------------------------------------------------------------------------------
# Verify whether all repositories are clean and return True or False
#------------------------------------------------------------------------------
def IsWorkareaClean(branch=None):
	clean = True
	owd = os.getcwd()
	for area in ListWorkareas():
		os.chdir(area)
		files = GitListFiles(opts="--others --modified")
		if files:
			clean = False
			print "WW: Workarea not clean " + area
			for file in files:
				print "WW: - " + file
		if branch:
			if GitGetCurrentBranch() != branch:
				print "WW: Not on branch ", branch, area
				clean = False
		os.chdir(owd)

	return clean

def GetHttpLink():
	return "http://{0}/~{1}/CVEs/".format(http_host, my_login)

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
				r = [key + ": " + data[key]]
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
		for pkg in sorted(data.keys()):
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

