
set -o pipefail
.lket/mk-bug-defs $ESRC/bugs | sed 's|^|drivers/net/wimax/bugs.h:|'
