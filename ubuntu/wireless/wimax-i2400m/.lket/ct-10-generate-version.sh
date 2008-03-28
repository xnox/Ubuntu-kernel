
# Crappy version support--we'll change this eventually -- for now, we
# list the milestone in the minor number and the changeset id if we
# find one.
#
# FIXME: it'd be nice to have something to extract the node from a GIT
# tree.
base_version=1.1
version_file_name=${version_file_name:-drivers/net/wimax/version.h}

if [ -f $ESRC/.hg_archival.txt ];
then
    node=-$(awk '$1 == "node:" { print $2; }' $ESRC/.hg_archival.txt)
elif [ -d $ESRC/.hg ] && hg --help >& /dev/null;
then
    node=-$(hg -R $ESRC --verbose id | cut -d' ' -f1)
else
    node=
fi
echo "I: compile-time version is $base_version$node" 1>&2
echo "$version_file_name:#define WIMAX_VERSION \"$base_version$node\""
