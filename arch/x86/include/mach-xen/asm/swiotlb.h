#ifdef CONFIG_X86_32
# include "swiotlb_32.h"
#else
# include_next <asm/swiotlb.h>
#endif
