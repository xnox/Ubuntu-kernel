#!/usr/bin/python
#==============================================================================
# Author: Stefan Bader <stefan.bader@canonical.com>
# Copyright (C) 2010
#
# This script is distributed under the terms and conditions of the GNU General
# Public License, Version 3 or later. See http://www.gnu.org/copyleft/gpl.html
# for details.
#==============================================================================
import sys, os
from subprocess import *

#------------------------------------------------------------------------------
# Returns a list which contains all remote repositories of the current tree.
#------------------------------------------------------------------------------
def GitListRemotes():
	list = []
	stdout = Popen("git remote", shell=True, stdout=PIPE).stdout
	for line in stdout:
		list.append(line.strip())
	stdout.close()

	return list

#------------------------------------------------------------------------------
# Helper function which will return a list containing the branch names.
#
# remote: Include remote branches if True
#------------------------------------------------------------------------------
def GitListBranches(remote=False):
	list = []
	cmd = "git branch"
	if remote == True:
		cmd += " -a"
	stdout = Popen(cmd, shell=True, stdout=PIPE).stdout
	for line in stdout:
		if line[0] == "*":
			line = line[1:]
		list.append(line.strip())
	stdout.close()

	return list

#------------------------------------------------------------------------------
# Returns a string containing the output of "git describe".
#
# opts can contain additional options.
#------------------------------------------------------------------------------
def GitDescribe(object, opts=None):
	info = None
	cmd = "git describe"
	if opts:
		cmd += " " + opts
	cmd += " " + object
	p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
	for line in p.stdout:
		info = line.rstrip()
		break
	p.stdout.close()
	p.stderr.close()

	return info

#------------------------------------------------------------------------------
# Returns the name of the currently checked out branch or None
#------------------------------------------------------------------------------
def GitGetCurrentBranch():
	branch = None
	stdout = Popen("git branch", shell=True, stdout=PIPE).stdout
	for line in stdout:
		if line[0] == "*":
			branch = line[1:].strip()
			break
	stdout.close()

	return branch

#------------------------------------------------------------------------------
# Return a list of files found
#
# opts: String containing git ls-files arguments
#------------------------------------------------------------------------------
def GitListFiles(opts=None):
	list = []
	cmd = "git ls-files"
	if opts:
		cmd += " " + opts
	stdout = Popen(cmd, shell=True, stdout=PIPE).stdout
	for line in stdout:
		list.append(line.rstrip())
	stdout.close()

	return list

#------------------------------------------------------------------------------
# Get the SHA1 (or None on error) of the commit that is the common base for
# both given branches.
#------------------------------------------------------------------------------
def GitMergeBase(branch1, branch2):
	base = None
	cmd = "git merge-base {0} {1}".format(branch1, branch2)
	p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
	for line in p.stdout:
		base = line.strip()
		break
	p.stdout.close()
	p.stderr.close()

	return base

#------------------------------------------------------------------------------
# Return a list containing the output of "git log"
#
# opts: string containing arguments (can be empty)
#------------------------------------------------------------------------------
def GitLog(opts=None):
	list = []
	cmd = "git log"
	if opts and opts != "":
		cmd += " " + opts
	p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
	for line in p.stdout:
		list.append(line.rstrip())
	p.stdout.close()
	p.stderr.close()

	return list
	
#------------------------------------------------------------------------------
# Read and return the contents of a file in git. Default is the currently
# active branch. This can be overridden by providing a sha1.
#------------------------------------------------------------------------------
def GitCatFile(file, sha1=None):
	lines = []

	if sha1:
		cmd = "git show {0}:{1}".format(sha1, file)
	else:
		cmd = "git show HEAD:{0}".format(file)

	p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
	for line in p.stdout:
		if line.startswith("tree ") or line == "\n":
			continue
		lines.append(line)
	p.stdout.close()
	p.stderr.close()

	return lines

#------------------------------------------------------------------------------
# Return the output of git diff as an array of lines.
#------------------------------------------------------------------------------
def GitDiff(filename=None, gitrange=None):
	lines = []
	cmd = "git diff"
	if gitrange:
		cmd += " " + gitrange
	if filename:
		cmd += " " + filename
	p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
	for line in p.stdout:
		lines.append(line)
	p.stdout.close()
	p.stderr.close()

	return lines
