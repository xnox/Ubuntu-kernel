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
# opt: String containing git ls-files arguments
#------------------------------------------------------------------------------
def GitListFiles(opt):
	list = []
	stdout = Popen("git ls-files " + opt, shell=True, stdout=PIPE).stdout
	for line in stdout:
		list.append(line.strip())
	stdout.close()

	return list

