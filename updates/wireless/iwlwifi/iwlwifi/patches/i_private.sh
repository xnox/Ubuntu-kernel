#!/usr/bin/env /bin/bash
# Fix kernels before 2.6.19 don't have inode->i_private issue
sed -i -e "s:inode->i_private:inode->u.generic_ip:g" \
    $1/*.{c,h}
