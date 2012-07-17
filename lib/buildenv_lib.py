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
from git_lib import *

'''GetDebianDir(sha1=None)

	Get the directory where the debian config files are located. Usually
	this is "./debian", but for abstracted debian it might be an alternate
	directory.

	sha1	Can be used to select a specific commit or branch in history.
'''
def GetDebianDir(sha1=None):
	debian = None
	#----------------------------------------------------------------------
	# First try the contents of 'debian/debian.env'
	#----------------------------------------------------------------------
	for line in GitCatFile(os.path.join("debian", "debian.env"), sha1=sha1):
		if "DEBIAN=" in line:
			return line.split("=", 1)[1].strip()

	#----------------------------------------------------------------------
	# Older versions had a declaration in 'debian/rules'
	#----------------------------------------------------------------------
	for line in GitCatFile(os.path.join("debian", "rules"), sha1=sha1):
		if "DEBIAN=" in line:
			return line.split("=", 1)[1].strip()

	#----------------------------------------------------------------------
	# Oh, well. So it must be 'debian'.
	#----------------------------------------------------------------------
	return "debian"

'''GetPackageInfo(sha1=None, prev=False)

	Returns a list of four elements (name, version, pocket and options)
	of the changelog.

	sha1	can be used to return the values at that point in history or
		on another branch.
	prev	if set to True will return data of the previous release in
		that changelog.
'''
def GetPackageInfo(sha1=None, prev=False):
	changelog = os.path.join(GetDebianDir(sha1=sha1), "changelog")
	for line in GitCatFile(changelog, sha1=sha1):
		if line[0] != " " and line[0] != "	" and line[0] != "\n":
			(name, ver, pocket, opt) = line.rstrip().split(" ")
			ver = ver[1:-1]
			if pocket[-1] == ";":
				pocket = pocket[0:-1]
			if prev == False:
				return [name, ver, pocket, opt]
			prev = False
	return None

'''GetUploadVersion(series, package, pocket="updates")

	Returns the version number of the package in the given series.
	By default the version number in the updates pocket is returned.
'''
def GetUploadVersion(series, package, pocket="updates"):
	if not pocket in [ "release", "updates", "security", "proposed" ]:
		raise NameError(pocket)
	cmd = "rmadison --arch=source " + package
	version = None
	stdout = Popen(cmd, shell=True, stdout=PIPE).stdout
	for line in stdout:
		fields = line.strip().split("|")
		rpocket = fields[2].strip().split("/")[0]
		if pocket == "release" and rpocket == series:
			version = fields[1].strip()
			break
		if rpocket == series + "-" + pocket:
			version = fields[1].strip()
			break
	stdout.close()

	return version

'''VersionGetABI(version)

	Return the number representing the ABI from a package version.
'''
def VersionGetABI(version):
	abi = 0
	if re.match("\d+\.\d+\.\d+-\d+\.", version):
		abi = int(version.split("-", 1)[1].split(".", 1)[0])
	return abi

'''BumpABI()

	Increment the ABI number of the package (must be called from topdir)
'''
def VersionBumpABI(version):
	if re.match("\d+\.\d+\.\d+-\d+\.", version):
		(v1, v2) = version.split("-", 1)
		(abi, rel) = v2.split(".", 1)
		abi = int(abi) + 1
		version = "{0}-{1}.{2}".format(v1, abi, rel)
	return version

'''ModifyChangelog(BumpABI=, BumpRelease=, SetRelease=)

	Modify the top entry of a changelog

	@BumpABI	increments the ABI number if true
	@BumpRelease	increments the release number if true
	@SetRelease	set the release/pocket to the specified value

	returns True if modifications were done, False otherwise
'''
def ModifyChangelog(BumpABI=False, BumpRelease=False, SetRelease=None):
	changelog = os.path.join(GetDebianDir(), "changelog")

	if not BumpABI and not BumpRelease and not SetRelease:
		return False

	ifile = open(changelog, "r")
	try:
		ofile = open(changelog + ".new", "w")
	except:
		ifile.close()
		raise

	handled = False
	linenr = 1
	for line in ifile.readlines():
		if linenr == 1:
			handled = True
			line = line.rstrip()
			(name, version, pocket, opts) = line.split(" ")
			version = version[1:-1]

			if BumpABI:
				oldversion = version
				version = VersionBumpABI(oldversion)
				if version == oldversion:
					break

			if BumpRelease:
				raise ValueError

			if SetRelease:
				pocket = SetRelease + ";"

			line = "{0} ({1}) {2} {3}\n".format(
				name, version, pocket, opts)
			handled = True

		if not handled:
			break
		ofile.write(line)
		linenr += 1
	ofile.close()
	ifile.close()

	if not handled:
		os.unlink(changelog + ".new")
	else:
		os.rename(changelog + ".new", changelog)

	return handled

def BumpABI():
	return ModifyChangelog(BumpABI=True)

'''GetPackageName()
'''
def GetPackageName():
	changelog = os.path.join(GetDebianDir(), "changelog")
	pkg = None

	try:
		file = open(changelog, "r")
		pkg = file.readline().strip().split()[0]
		file.close()
	except:
		pass

	return pkg

'''RunScript(script, host=None, interpreter=None)

	Execute the given script either locally or on a remote host

	@script		string containing the script
	@host		name of the host to execute on
	@interpreter	run the command/script by this interpreter (default
			is /bin/sh)
	@timeout	only used when running remotely, this is the ssh
			timeout

	Returns triple (<returncode>, <stdout>, <stderr>)
		stdout and stderr are strings
'''
def RunScript(script, host=None, interpreter=None, timeout=60,
		getoutput=True):
	if not interpreter:
		interpreter = "/bin/sh"
	cmd = []
	if host:
		cmd.extend([ "ssh", "-oConnectTimeout=" + str(timeout), host ])
	cmd.append(interpreter)

	if getoutput:
		p = Popen(cmd, stdin=PIPE, stdout=PIPE, stderr=PIPE)
		(pout, perr) = p.communicate(input=script)
	else:
		p = Popen(cmd, stdin=PIPE)
		(pout, perr) = p.communicate(input=script)
	
	return (p.returncode, pout, perr)

