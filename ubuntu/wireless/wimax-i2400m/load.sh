#! /bin/bash
#
# Module loader script
#
# Copyright (C) 2005-2006 Intel Corporation
# Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License version
# 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
#
# This is just needed if you are lazy enough as not to install the
# modules in the main modules directory.

progname=$(basename $0)

function help
{
    cat <<EOF
Usage: $progname [OPTIONS] OPERATION [GROUP]

Loads/unloads WiMax & drivers modules

Options:

-h|--help	This help
-o|--other      Load also other modules described by ../*/.modules

Operation:

load|lo|l	Loads modules (default)

unload|un|u	Unloads modules

print|pr|p      Print the list of modules in load order


Group:
wimax		Applies operation only to wimax stack modules
i2400m		Applies operation to i2400m modules
all		Applies operation to all modules

This script looks for all the files called '.modules' in the actual
directory and if -o is specified, all directories in the parent (ie:
../*/.modules) and loads the modules listed in there.

The .modules file follows the following format:

NN relative-path/file.ko group

All the modules file names from all the .modules files are prefixed
the ../dir where they originate and then sorted according to NN. Then
they are loaded in that order.


EOF
}

function in_load_group
{
   local loc_group=$1

   for v in $groups
   do
       if [ "$v" == "$loc_group" ] || [ "$v" == "all" ]; then
           echo 1
           return
       fi
   done
   echo 0
}

function check_or_load_mods
{
	local num_args=$#
	local load=$1
	shift

	while [ $num_args -ge 2 ]; do
		local mod=$1
		local group=$2
		local should_load=`in_load_group $group`
		if [ -r $mod ] && [ $should_load -eq 1 ]
		then
			if [ "$load" == "load" ]
			then
				echo loading $mod
				insmod $mod || true;
			elif [ "$load" == "unload" ]
			then
				echo unloading $mod
				rmmod $mod || true
			else
				echo will load $mod
			fi
		else
			echo ignoring $mod [not present or not in selected group]
		fi
		shift 2
		num_args=`expr $num_args - 2`
	done
}

set -u

# Chew the command line
do_other=0
while true
  do
  [ $# -eq 0 ] && break
  case "$1" in
      -h|--help)
          help;
          exit 0;
          ;;
      -o|--other)
          do_other=1;
          exit 0;
          ;;
      -*|--*)
          echo "E: unknown option $1" 1>&2;
          exit 1;
          ;;
      *)
          break;
          ;;
  esac
done

cmd=${1:-load}
shift
groups=${*:-"all"}
# make sure modfiles are absolute paths
if [ $do_other = 1 ]
then
    modfiles="$(pwd)/../*/.modules"
else
    modfiles="$(pwd)/.modules"
fi

case $cmd in
    load|lo|l)
        modprobe firmware_class >& /dev/null || true
        mods="$(awk '/[0-9]+ / {			\
			dir = FILENAME; 		\
			gsub("/([^/]+)$", "/", dir);	\
			print $1 " " dir "" $2,$3;	\
		     }' $modfiles 			\
            | sort -u 					\
            | cut -d' ' -f2,3)"
	check_or_load_mods load $mods
        ;;
    print|pr|p)
        mods="$(awk '/[0-9]+ / {			\
			dir = FILENAME; 		\
			gsub("/([^/]+)$", "/", dir);	\
			print $1 " " dir "" $2,$3;	\
		     }' $modfiles 			\
            | sort -u 					\
            | cut -d' ' -f2,3)"
	check_or_load_mods check $mods
        ;;
    unload|unlo|un|u)
        # Note the -r in sort to reverse the sorting
        mods="$(awk '/[0-9]+ / {			\
			dir = FILENAME; 		\
			gsub("/([^/]+)$", "/", dir);	\
			print $1 " " dir "" $2,$3;	\
		     }' $modfiles 			\
            | sort -ru 					\
            | cut -d' ' -f2,3)"
	check_or_load_mods unload $mods
        ;;
    *)
        echo "E: unknown operation $cmd" 1>&2;
        exit 1;
esac

