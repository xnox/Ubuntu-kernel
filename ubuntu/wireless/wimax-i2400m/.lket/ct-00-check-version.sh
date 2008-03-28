
if ! kver=$(make -s -C $KSRC kernelversion)
then
    cat 1>&2 <<EOF
E:
E: Can't determine Linux kernel version!!!!
E:
EOF
    exit 1
fi

major=$(echo $kver | cut -d. -f1)
minor=$(echo $kver | cut -d. -f2)
release=$(echo $kver | cut -d. -f3 | cut -d- -f1)

if [ $major != 2 ]
then
    echo "E: unsupported Linux kernel major version $kver" 1>&2
    exit 1
fi

if [ $minor != 6 ]
then
    echo "E: unsupported Linux kernel minor version $kver" 1>&2
    exit 1
fi

if [ $release -lt 22 ] || [ $release -gt 24 ]
then
    echo "E: unsupported Linux kernel release version $kver" 1>&2
    exit 1
fi
exit 0
