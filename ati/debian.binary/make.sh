FGL_PUBLIC=firegl
target_define=FGL_RX


if [ -z "${CC}" ]; then 
	CC=gcc
fi

uname_r=`uname -r`
uname_v=`uname -v`
uname_s=`uname -s`
uname_m=`uname -m`
uname_a=`uname -a`

XF_ROOT=/usr/X11R6                                                                    
XF_BIN=$XF_ROOT/bin
OS_MOD=/lib/modules


if [ -z "${SOURCE_PREFIX}" ]; then
  SOURCE_PREFIX=.
fi
if [ "${SOURCE_PREFIX}" != "/" ]; then
  SOURCE_PREFIX=`echo $SOURCE_PREFIX | sed -e 's,/$,,g'`
fi

if [ -z "${LIBIP_PREFIX}" ]; then
  LIBIP_PREFIX=.
fi
if [ "${LIBIP_PREFIX}" != "/" ]; then
  LIBIP_PREFIX=`echo $LIBIP_PREFIX | sed -e 's,/$,,g'`
fi


if [ -z "${KSRC}" ]; then
linuxincludes=/usr/src/linux/include
  if [ -d /lib/modules/${uname_r}/build/include ];
  then
    linuxincludes=/lib/modules/${uname_r}/build/include
  fi
else
  linuxincludes=${KSRC}/include
fi

drmincludes=${linuxincludes}/../drivers/char/drm
drmdefines="-DFIREGL_410"


if [ $CHECK_P3 -ne 0 ]
then

$XF_BIN/cpu_check >/dev/null                                                      
case "$?" in                                                                      
    0) iii=     ;;                                                                
    1) iii=     ;;                                                                
    2) iii=.iii ;;                                                                
    3) iii=     ;;                                                                
    4) iii=     ;;                                                                
    5) iii=.iii ;;                                                                
    6) iii=.iii ;;                                                                
    *) iii=     ;;                                                                
esac

else
  iii=
fi

src_file=$linuxincludes/linux/agp_backend.h
if [ -e $src_file ];
then
  AGP=1
  def_agp=-D__AGP__
  echo "file $src_file says: AGP=$AGP"                             >> $logfile
fi

if [ $AGP = 0 ]
then
  echo "assuming default: AGP=$AGP"                                >> $logfile
fi

SMP=0

# config/smp.h may contain this: #define CONFIG_SMP 1 | #undef  CONFIG_SMP
src_file=$linuxincludes/config/smp.h
if [ ! -e $src_file ];
then
  echo "Warning:"                                                  >> $logfile
  echo "kernel includes at $linuxincludes not found or incomplete" >> $logfile
  echo "file: $src_file"                                           >> $logfile
  echo ""                                                          >> $logfile
else
  if [ `cat $src_file | grep "#undef" | grep "CONFIG_SMP" -c` = 0 ]
  then
    SMP=`cat $src_file | grep CONFIG_SMP | cut -d' ' -f3`
    echo "file $src_file says: SMP=$SMP"                           >> $logfile
  fi
fi

# 4.
# linux/autoconf.h may contain this: #define CONFIG_SMP 1
src_file=$linuxincludes/linux/autoconf.h
if [ ! -e $src_file ];
then
  echo "Warning:"                                                  >> $logfile
  echo "kernel includes at $linuxincludes not found or incomplete" >> $logfile
  echo "file: $src_file"                                           >> $logfile
  echo ""                                                          >> $logfile
else
  if [ `cat $src_file | grep "#undef" | grep "CONFIG_SMP" -c` = 0 ]
  then
    SMP=`cat $src_file | grep CONFIG_SMP | cut -d' ' -f3`
    echo "file $src_file says: SMP=$SMP"                           >> $logfile
  fi
fi

if [ $SMP = 0 ]
then
  echo "assuming default: SMP=$SMP"                                >> $logfile
fi

# act on final result
if [ ! $SMP = 0 ]
then
  smp="-SMP"
  def_smp=-D__SMP__
fi


# ==============================================================
# resolve if we are running a MODVERSIONS enabled kernel

MODVERSIONS=0

if [ $DEMAND_BZIMAGE -gt 0 ]
then

# 1.
# config/modversions.h may contain this: #define CONFIG_MODVERSIONS 1 | #undef  CONFIG_MODVERSIONS
src_file=$linuxincludes/config/modversions.h
if [ ! -e $src_file ];
then
  echo "Warning:"                                                  >> $logfile
  echo "kernel includes at $linuxincludes not found or incomplete" >> $logfile
  echo "file: $src_file"                                           >> $logfile
  echo ""                                                          >> $logfile
else
  if [ 1 -eq 1 ]
  then
    # create a helper source file and preprocess it
    tmp_src_file=tmpsrc.c
	tmp_pre_file=tmppre.pre
    echo "#include <$src_file>"                                      > $tmp_src_file
	${CC} -E -nostdinc -dM -I$linuxincludes $tmp_src_file 			 > $tmp_pre_file

    if [ `cat $tmp_pre_file | grep "1" | grep "#define" | grep "CONFIG_MODVERSIONS" -c` = 1 ]
    then
      MODVERSIONS=`cat $tmp_pre_file | grep CONFIG_MODVERSIONS | cut -d' ' -f3`
      echo "file $src_file says: MODVERSIONS=$MODVERSIONS"           >> $logfile
    fi

    rm -f $tmp_src_file $tmp_pre_file
  else
    if [ `cat $src_file | grep "#undef" | grep "CONFIG_MODVERSIONS" -c` = 0 ]
    then
      MODVERSIONS=`cat $src_file | grep CONFIG_MODVERSIONS | cut -d' ' -f3`
      echo "file $src_file says: MODVERSIONS=$MODVERSIONS"           >> $logfile
    fi
  fi
fi

fi

# 2.
# linux/autoconf.h may contain this: #define CONFIG_MODVERSIONS 1
src_file=$linuxincludes/linux/autoconf.h
if [ ! -e $src_file ];
then
  echo "Warning:"                                                  >> $logfile
  echo "kernel includes at $linuxincludes not found or incomplete" >> $logfile
  echo "file: $src_file"                                           >> $logfile
  echo ""                                                          >> $logfile
else
  if [ `cat $src_file | grep "#undef" | grep "CONFIG_MODVERSIONS" -c` = 0 ]
  then
    MODVERSIONS=`cat $src_file | grep CONFIG_MODVERSIONS | cut -d' ' -f3`
    echo "file $src_file says: MODVERSIONS=$MODVERSIONS"           >> $logfile
  fi
fi

if [ $MODVERSIONS = 0 ]
then
  echo "assuming default: MODVERSIONS=$MODVERSIONS"                >> $logfile
fi

# act on final result
if [ ! $MODVERSIONS = 0 ]
then
  def_modversions="-DMODVERSIONS -include $linuxincludes/linux/modversions.h"
fi


# ==============================================================
# check for required source and lib files

file=${SOURCE_PREFIX}/${FGL_PUBLIC}_public.c
if [ ! -e $file ];
then 
  echo "$file: required file is missing in build directory" | tee -a $logfile
  exit 1
fi
file=${SOURCE_PREFIX}/${FGL_PUBLIC}_public.h
if [ ! -e $file ];
then 
  echo "$file: required file is missing in build directory" | tee -a $logfile
  exit 1
fi

# break down OsRelease string into its components
major=`echo $OsRelease | sed -n -e s/"^\([[:digit:]]*\)\.\([[:digit:]]*\)\.\([[:digit:]]*\)\(.*\)"/"\\1"/p`
minor=`echo $OsRelease | sed -n -e s/"^\([[:digit:]]*\)\.\([[:digit:]]*\)\.\([[:digit:]]*\)\(.*\)"/"\\2"/p`
patch=`echo $OsRelease | sed -n -e s/"^\([[:digit:]]*\)\.\([[:digit:]]*\)\.\([[:digit:]]*\)\(.*\)"/"\\3"/p`
extra=`echo $OsRelease | sed -n -e s/"^\([[:digit:]]*\)\.\([[:digit:]]*\)\.\([[:digit:]]*\)\(.*\)"/"\\4"/p`

if [ "$1" = "verbose" ]
then
  echo OsRelease=$OsRelease  | tee -a $logfile
  echo major=$major          | tee -a $logfile
  echo minor=$minor          | tee -a $logfile
  echo patch=$patch          | tee -a $logfile
  echo extra=$extra          | tee -a $logfile
  echo SMP=$SMP              | tee -a $logfile
  echo smp=$smp              | tee -a $logfile
  echo iii=$iii              | tee -a $logfile
  echo AGP=$AGP              | tee -a $logfile
fi

major_minor=$major.$minor.
major_minor_grep=$major[.]$minor[.]

echo .   >>$logfile

# determine compiler version
cc_version_string=`${CC} -v 2>&1 | grep -v "specs from" -v | grep -v "Thread model" | grep -v "Configured with"`
cc_version=`echo $cc_version_string | sed -e s/egcs-//g | sed -n -e 's/\(^gcc version\)[[:space:]]*\([.0123456789]*\)\(.*\)/\2/'p`
cc_version_major=`echo $cc_version | cut -d'.' -f1`
cc_version_minor=`echo $cc_version | cut -d'.' -f2`

echo CC=${CC} >> $logfile
echo cc_version=$cc_version >> $logfile
if [ "$1" = "verbose" ]
then
    echo CC=${CC}
    echo cc_version=$cc_version
fi

# try to symlink the compiler matching ip-library
lib_ip_base=${LIBIP_PREFIX}/lib${MODULE}_ip.a

# remove existing symlink first
if [ -L $lib_ip_base ];
then
  # remove that symlink to create a new one in next paragraph
  rm -f ${lib_ip_base}
else
  if [ -e $lib_ip_base ];
  then
    echo "Error: the ip-library is present as some file - thats odd!" | tee -a $logfile
    # comment out the below line if you really want to use this local file
    if [ -z "${LIBIP_PREFIX}" ]; then
	    exit 1
    fi
  fi
fi

# if there is no ip-lib file then deterimine which symlink to setup
if [ ! -e $lib_ip_base ];
then
    if [ -e ${lib_ip_base}.GCC$cc_version ];
    then
        # we do have an ip-lib that exactly matches the users compiler
        ln -s ${lib_ip_base}.GCC$cc_version ${lib_ip_base}
        echo "found exact match for ${CC} and the ip-library" >> $logfile
    else
        # there is no exact match for the users compiler
        # try if we just provide a module that matches the compiler major number
        for lib_ip_major in `ls -1 ${lib_ip_base}.GCC$cc_version_major* 2>/dev/null`;
        do
            # just the last matching library does server our purposes - ease of coding
            rm -f ${lib_ip_base}
            ln -s ${lib_ip_major} ${lib_ip_base}
        done
        
        # after the loop there should be a file or a symlink or whatever
        if [ ! -e ${lib_ip_base} ]
        then
            echo "ls -l ${lib_ip_base}*" >>$logfile
            ls -l ${lib_ip_base}* 2>/dev/null >>$logfile
            echo "Error: could not resolve matching ip-library." | tee -a $logfile
            exit 1
        else
            echo "found major but not minor version match for ${CC} and the ip-library" >> $logfile
        fi
    fi
fi

# log a few stats
echo "ls -l ${lib_ip_base}"     >> $logfile
      ls -l ${lib_ip_base}      >> $logfile

# assign result (is not really a variable in current code)
core_lib=${lib_ip_base}

#echo "lib file name was resolved to: $core_lib" >> $logfile
#if [ "$1" = "verbose" ]
#then
#  echo "lib file name was resolved to: $core_lib"
#fi
#if [ ! -e $core_lib ];
#then 
#  echo "required lib file is missing in build directory" | tee -a $logfile
#  exit 1
#fi

echo .  >> $logfile


# ==============================================================
# make clean
echo cleaning... | tee -a $logfile
if [ -e ${FGL_PUBLIC}_public.o ]
then 
  rm -f ${FGL_PUBLIC}_public.o 2>&1 | tee -a $logfile
fi
if [ -e ${MODULE}.o ]
then
  rm -f ${MODULE}.o     2>&1 | tee -a $logfile
fi

#if [ -e agpgart_fe.o ]
#then 
#  rm -f agpgart_fe.o 2>&1 | tee -a $logfile
#fi
if [ -e agpgart_be.o ]
then 
  rm -f agpgart_be.o 2>&1 | tee -a $logfile
fi
if [ -e agp3.o ]
then 
  rm -f agp3.o 2>&1 | tee -a $logfile
fi
if [ -e i7505-agp.o ]
then 
  rm -f i7505-agp.o 2>&1 | tee -a $logfile
fi
if [ -e nvidia-agp.o ]
then 
  rm -f nvidia-agp.o 2>&1 | tee -a $logfile
fi

if [ -e patch/linux ]
then
  if [ -e patch/linux/highmem.h ]
  then
    rm -f patch/linux/highmem.h
  fi
  rmdir patch/linux 2>/dev/null
fi

if [ -e patch/drivers/char/drm ]
then
  if [ -e patch/drivers/char/drm/drm_proc.h ]
  then
    rm -f patch/drivers/char/drm/drm_proc.h
  fi
  if [ -e patch/drivers/char/drm/drmP.h ]
  then
    rm -f patch/drivers/char/drm/drmP.h
  fi
  if [ -e patch/drivers/char/drm/drm_os_linux.h ]
  then
    rm -f patch/drivers/char/drm/drm_os_linux.h
  fi
  # cleanup any patchfile remainder
  rm -f patch/drivers/char/drm/*.orig patch/drivers/char/drm/*.rej
  # cleanup dirs
  rmdir patch/drivers/char/drm
  rmdir patch/drivers/char
  rmdir patch/drivers
fi

if [ -e patch ]
then
  rmdir patch 2>/dev/null
fi

# ==============================================================
# apply header file patches
# suppress known warning in specific header file
patch_includes=

srcfile=${linuxincludes}/linux/highmem.h
if [ -e ${srcfile} ]
then
  echo "patching 'highmem.h'..." | tee -a $logfile
  mkdir -p patch/include/linux
  cat ${srcfile} | sed -e 's/return kmap(bh/return (char*)kmap(bh/g' >patch/include/linux/highmem.h
  patch_includes="${patch_includes} -Ipatch/include"
fi

srcfile=${drmincludes}/drmP.h
if [ -e ${srcfile} ]
then
 if [ `cat ${srcfile} | grep -c "\"[\" DRM_NAME \":\" __FUNCTION__ \"] \" fmt ,"` -eq 0 ]
 then
  echo "skipping patch for 'drmP.h', not needed" | tee -a $logfile
  # a local copy is still needed for propper compile of patched drm_os_linux.h
  mkdir -p patch/drivers/char/drm
  cd patch/drivers/char/drm
  if [ -e ${drmincludes}/drm_proc.h ]
  then
    cp ${drmincludes}/drm_proc.h drm_proc.h
  fi
  cp ${srcfile} drmP.h
  cd ${current_wd}
 else
  echo "patching 'drmP.h'..." | tee -a $logfile
  mkdir -p patch/drivers/char/drm
  cd patch/drivers/char/drm

  # we need both of below files, even if we dont apply any changes to the first.
  # this will work around the inclusion concept and fix the gcc3.2 warnings
  cp ${drmincludes}/drm_proc.h drm_proc.h
  cat <<-begin_end | sed -e 's/\\x/\\/g' >drmP.h.patch
--- drmP.h.orig	2002-10-15 20:30:21.000000000 +0200
+++ drmP.h	2002-10-15 20:31:58.000000000 +0200
@@ -255,9 +255,9 @@
 
 				/* Macros to make printk easier */
 #define DRM_ERROR(fmt, arg...) \x
-	printk(KERN_ERR "[" DRM_NAME ":" __FUNCTION__ "] *ERROR* " fmt , ##arg)
+	printk(KERN_ERR "[" DRM_NAME ":%s] *ERROR* " fmt, __FUNCTION__ , ##arg)
 #define DRM_MEM_ERROR(area, fmt, arg...) \x
-	printk(KERN_ERR "[" DRM_NAME ":" __FUNCTION__ ":%s] *ERROR* " fmt , \x
+	printk(KERN_ERR "[" DRM_NAME ":%s:%s] *ERROR* " fmt, __FUNCTION__, \x
 	       DRM(mem_stats)[area].name , ##arg)
 #define DRM_INFO(fmt, arg...)  printk(KERN_INFO "[" DRM_NAME "] " fmt , ##arg)
 
@@ -266,7 +266,7 @@
 	do {								\x
 		if ( DRM(flags) & DRM_FLAG_DEBUG )			\x
 			printk(KERN_DEBUG				\x
-			       "[" DRM_NAME ":" __FUNCTION__ "] " fmt ,	\x
+			       "[" DRM_NAME ":%s] " fmt, __FUNCTION__ ,  \x
 			       ##arg);					\x
 	} while (0)
 #else
begin_end
  
  cp ${srcfile} drmP.h
  patch -p0 -E <drmP.h.patch
  rm -f drmP.h.patch
  cd ${current_wd}
  patch_includes="${patch_includes} -Ipatch/drivers/char/drm"
 fi
fi

srcfile=${drmincludes}/drm_os_linux.h
if [ -e ${srcfile} ]
then
 if [ `cat ${srcfile} | grep -c "#warning"` -eq 0 ]
 then
  echo "skipping patch for 'drm_os_linux.h', not needed" | tee -a $logfile
 else
  echo "patching 'drm_os_linux.h'..." | tee -a $logfile
  mkdir -p patch/drivers/char/drm
  cd patch/drivers/char/drm
  cat ${srcfile} | grep -v "#warning" > drm_os_linux.h
  cd ${current_wd}
  patch_includes="${patch_includes} -Ipatch/drivers/char/drm"
 fi
fi

# ==============================================================
# defines for all targets
def_for_all="-DATI_AGP_HOOK -DATI -DFGL -D${target_define} -DFGL_CUSTOM_MODULE -DPAGE_ATTR_FIX=$PAGE_ATTR_FIX"

# defines for specific os and cpu platforms
if [ "${uname_m}" = "x86_64" ]; then
	def_machine="-mcmodel=kernel"
fi

if [ "${uname_m}" = "ia64" ]; then
        def_machine="-ffixed-r13 -mfixed-range=f12-f15,f32-f127"
fi

# determine which build system we should use
# note: we do not support development kernel series like the 2.5.xx tree
if [ $major -gt 2 ]; then
    kernel_is_26x=1
else
  if [ $major -eq 2 ]; then
    if [ $minor -gt 5 ]; then
        kernel_is_26x=1
    else
        kernel_is_26x=0
    fi
  else
    kernel_is_26x=0
  fi
fi

if [ $kernel_is_26x -eq 1 ]; then
    kmod_extension=.ko
else
    kmod_extension=.o
fi


# ==============================================================
# resolve if we are running a kernel with the new VMA API 
# that was introduced in linux-2.5.3-pre1
# or with the previous one that at least was valid for linux-2.4.x

if [ $kernel_is_26x -gt 0 ];
then
  echo "assuming new VMA API since we do have kernel 2.6.x..." | tee -a $logfile
  def_vma_api_version=-DFGL_LINUX253P1_VMA_API
  echo "def_vma_api_version=$def_vma_api_version"                   >> $logfile
else
  echo "probing for VMA API version..." | tee -a $logfile
  
  # create a helper source file and try to compile it into an objeckt file
  tmp_src_file=tmp_vmasrc.c
  tmp_obj_file_240=tmp_vma240.o
  tmp_obj_file_253=tmp_vma253.o
  tmp_log_file_240=tmp_vma240.log
  tmp_log_file_253=tmp_vma253.log
  cat > $tmp_src_file <<-begin_end
/* this is a generated file */
#define __KERNEL__
#include <linux/mm.h>

int probe_vma_api_version(void) {
#ifdef FGL_LINUX253P1_VMA_API
  struct vm_area_struct *vma;
#endif
  unsigned long from, to, size;
  pgprot_t prot;
  
  return (
    remap_page_range(
#ifdef FGL_LINUX253P1_VMA_API
      vma,
#endif
      from, to, size, prot)
    );
}
begin_end

  # check for 240 API compatibility
  ${CC} -I$linuxincludes $tmp_src_file                                -c -o $tmp_obj_file_240 &> $tmp_log_file_240
  cc_ret_vma_240=$?
  echo "cc_ret_vma_240 = $cc_ret_vma_240"                           >> $logfile
    
  # check for 253 API compatibility
  ${CC} -I$linuxincludes $tmp_src_file -DFGL_LINUX253P1_VMA_API       -c -o $tmp_obj_file_253 &> $tmp_log_file_253
  cc_ret_vma_253=$?
  echo "cc_ret_vma_253 = $cc_ret_vma_253"                           >> $logfile
    
  # classify and act on results
  # (the check is designed so that exactly one version should succeed and the rest should fail)
  def_vma_api_version=
  if [ $cc_ret_vma_240 -eq 0 ]
  then
    if [ $cc_ret_vma_253 -eq 0 ]
    then
      echo "check results are inconsistent!!!"                      | tee -a $logfile
      echo "exactly one check should work, but not multiple checks."| tee -a $logfile
      echo "aborting module build."                                 | tee -a $logfile
      exit 1
    else
      # the kernel tree does contain the 240 vma api version
      def_vma_api_version=-DFGL_LINUX240_VMA_API
    fi
  else
    if [ $cc_ret_vma_253 -eq 0 ]
    then
      # the kernel tree does contain the 253 vma api version
      def_vma_api_version=-DFGL_LINUX253P1_VMA_API
    else
      echo "check results are inconsistent!!!"                      | tee -a $logfile
      echo "none of the probed versions did succeed."               | tee -a $logfile
      echo "aborting module build."                                 | tee -a $logfile
      exit 1
    fi
  fi
  
  echo "def_vma_api_version=$def_vma_api_version"                   >> $logfile
    
  # cleanup intermediate files
  rm -f $tmp_src_file $tmp_obj_file_240 $tmp_obj_file_253 $tmp_log_file_240 $tmp_log_file_253
fi


# ==============================================================
# make agp kernel module (including object files) and check results

if [ $kernel_is_26x -gt 0 ]; then
    echo "doing Makefile based build for kernel 2.6.x and higher"   | tee -a $logfile
    cd 2.6.x
    make PAGE_ATTR_FIX=$PAGE_ATTR_FIX 2>&1                          | tee -a $logfile
    res=$?
    cd ..
    if [ $res -eq 0 ]; then
        echo "build succeeded with return value $res"               | tee -a $logfile
    else
        echo "build failed with return value $res"                  | tee -a $logfile
        exit 1
    fi
    if [ -e ${MODULE}${kmod_extension} ]; then
        rm -f ${MODULE}${kmod_extension}
    fi
    ln -s 2.6.x/${MODULE}${kmod_extension}
    TERMINAL_HINT=0
else
    echo "doing script based build for kernel 2.4.x and similar"    | tee -a $logfile
    
WARNINGS="-Wall -Wwrite-strings -Wpointer-arith -Wcast-align -Wstrict-prototypes"
if [ $cc_version_major -ge 3 ];
then
  if [ $cc_version_major -eq 3 ];
  then
    if [ $cc_version_minor -ge 3 ];
    then
      # gcc 3.3 or higher is too verbose for us when using the -Wall option
      WARNINGS="-Wwrite-strings -Wpointer-arith -Wcast-align -Wstrict-prototypes"
    fi
  else
    # gcc 3.3 or higher is too verbose for us when using the -Wall option
    WARNINGS="-Wwrite-strings -Wpointer-arith -Wcast-align -Wstrict-prototypes"
  fi
fi

#SRC=${SOURCE_PREFIX}/agpgart_fe.c
#DST=agpgart_fe.o
#echo "compiling '$SRC'..." | tee -a $logfile
#cc_cmd="${CC} ${WARNINGS} -O2 -D__KERNEL__ -DMODULE -fomit-frame-pointer $def_for_all -D$MODULE $def_smp $def_modversions $patch_includes -I$linuxincludes -c $SRC -o $DST"
#echo "$cc_cmd" >> $logfile
#$cc_cmd 2>&1 | tee -a $logfile
#if [ ! -e $DST ] ;
#then
#  echo "compiling failed - object file was not generated" | tee -a $logfile
#  exit 1
#fi

SRC=${SOURCE_PREFIX}/agpgart_be.c
DST=agpgart_be.o
echo "compiling '$SRC'..." | tee -a $logfile
cc_cmd="${CC} ${WARNINGS} -O2 -D__KERNEL__ -DMODULE -fomit-frame-pointer $def_for_all $def_machine -D$MODULE $def_smp $def_modversions $patch_includes -I$linuxincludes -c $SRC -o $DST"
echo "$cc_cmd" >> $logfile
$cc_cmd 2>&1 | tee -a $logfile
if [ ! -e $DST ] ;
then
  echo "compiling failed - object file was not generated" | tee -a $logfile
  exit 1
fi

SRC=${SOURCE_PREFIX}/agp3.c
DST=agp3.o
echo "compiling '$SRC'..." | tee -a $logfile
cc_cmd="${CC} ${WARNINGS} -O2 -D__KERNEL__ -DMODULE -fomit-frame-pointer $def_for_all $def_machine -D$MODULE $def_smp $def_modversions $patch_includes -I$linuxincludes -c $SRC -o $DST"
echo "$cc_cmd" >> $logfile
$cc_cmd 2>&1 | tee -a $logfile
if [ ! -e $DST ] ;
then
  echo "compiling failed - object file was not generated" | tee -a $logfile
  exit 1
fi

SRC=${SOURCE_PREFIX}/i7505-agp.c
DST=i7505-agp.o
echo "compiling '$SRC'..." | tee -a $logfile
cc_cmd="${CC} ${WARNINGS} -O2 -D__KERNEL__ -DMODULE -fomit-frame-pointer $def_for_all $def_machine -D$MODULE $def_smp $def_modversions $patch_includes -I$linuxincludes -c $SRC -o $DST"
echo "$cc_cmd" >> $logfile
$cc_cmd 2>&1 | tee -a $logfile
if [ ! -e $DST ] ;
then
  echo "compiling failed - object file was not generated" | tee -a $logfile
  exit 1
fi

SRC=${SOURCE_PREFIX}/nvidia-agp.c
DST=nvidia-agp.o
echo "compiling '$SRC'..." | tee -a $logfile
cc_cmd="${CC} ${WARNINGS} -O2 -D__KERNEL__ -DMODULE -fomit-frame-pointer $def_for_all $def_machine -D$MODULE $def_smp $def_modversions $patch_includes -I$linuxincludes -c $SRC -o $DST"
echo "$cc_cmd" >> $logfile
$cc_cmd 2>&1 | tee -a $logfile
if [ ! -e $DST ] ;
then
  echo "compiling failed - object file was not generated" | tee -a $logfile
  exit 1
fi

# we don't need the agpgart module - skip that thing
# echo "linking agp module..." | tee -a $logfile
# ld="ld -r agpgart_fe.po agpgart_be.po -o agpgart.o"
# echo "$ld" >> $logfile
# $ld 2>&1 | tee -a $logfile
# if [ ! -e ${MODULE}.o ] ;
# then
#  echo "linking failed - kernel module was not generated" | tee -a $logfile
#  exit 1
# fi
#
# echo .  >> $logfile


# ==============================================================
# make custom kernel module and check results

SRC=${SOURCE_PREFIX}/${FGL_PUBLIC}_public.c
DST=${FGL_PUBLIC}_public.o
echo "compiling '$SRC'..." | tee -a $logfile
cc_cmd="${CC} ${WARNINGS} -O2 -D__KERNEL__ -DMODULE -fomit-frame-pointer $def_for_all $def_machine -D${MODULE} $def_vma_api_version $def_smp $def_modversions $def_agp $patch_includes -I$linuxincludes -I$drmincludes $drmdefines -c $SRC -o $DST"
echo "$cc_cmd" >> $logfile
$cc_cmd 2>&1 | tee -a $logfile | grep -v "warning: pasting would not give a valid preprocessing token"
if [ ! -e $DST ] ;
then
  echo "compiling failed - object file was not generated" | tee -a $logfile
  exit 1
fi

echo "linking of ${MODULE} kernel module..." | tee -a $logfile
if [ ! -z "${MODULE_NAME}" ]; then 
	module_version=.${MODULE_NAME}
fi
#ld="ld -r ${FGL_PUBLIC}_public.o agpgart_fe.o agpgart_be.o agp3.o i7505-agp.o nvidia-agp.o $core_lib -o ${MODULE}.o"
ld="ld -r ${FGL_PUBLIC}_public.o agpgart_be.o agp3.o i7505-agp.o nvidia-agp.o $core_lib -o ${MODULE}${module_version}.o"
echo "$ld" >> $logfile
$ld 2>&1 | tee -a $logfile
if [ ! -e ${MODULE}${module_version}.o ] ;
then
  echo "linking failed - kernel module was not generated" | tee -a $logfile
  exit 1
fi

# end of `else` for $kernel_is_26x
fi

echo .  >> $logfile


# ==============================================================
# install generated file at required location

TERMINAL_HINT=0
if [ `pwd | grep "$OS_MOD/${MODULE}/build_mod\$" -c` -gt 0 ]
then 
  echo duplicating results into driver repository...   | tee -a $logfile
  # duplicate generated file into respective kernel module subdir
  if [ $INSTALL_MODULE_SUBDIRS -eq 1 ];
  then
    target_dir=`pwd`/../$OsRelease$iii$smp
  else
    target_dir=`pwd`/..
  fi
  target_dir=`cd $target_dir;pwd`
  echo "target location: $target_dir" >> $logfile 
  if [ ! -e $target_dir ]
  then
    echo "creating target directory" >> $logfile
    mkdir $target_dir | tee -a $logfile
  fi
  
  echo "copying ${MODULE}${kmod_extension}" >> $logfile
  if [ $INSTALL_MODULE_SUBDIRS -eq 1 ];
  then
    cp -f ${MODULE}${kmod_extension} $target_dir | tee -a $logfile
  else
    cp -f ${MODULE}${kmod_extension} $target_dir/${MODULE}.$OsRelease$iii${kmod_extension} | tee -a $logfile
  fi
  
  echo "copying logfile of build" >> $logfile
  echo "*** end of build log ***" >> $logfile
  if [ $INSTALL_MODULE_SUBDIRS -eq 1 ];
  then
    cp -f $logfile $target_dir
  else
    cp -f $logfile $target_dir/make.$OsRelease$iii.log
  fi

  # terminal hint message
  if [ $INSTALL_MODULE_SUBDIRS -eq 0 ];
  then
    TERMINAL_HINT=1
  fi
else
  # the build was done from an external location - installation not intended
  echo "duplication skipped - generator was not called from regular lib tree" | tee -a $logfile 
fi

# ==============================================================
# finale

echo done.
echo ==============================

if [ $OPTIONS_HINTS -ne 0 ]; then

if [ $TERMINAL_HINT -eq 1 ];
then
  echo "You must change your working directory to $target_dir"
  echo "and then call ./make_install.sh in order to install the built module."
  echo ==============================
fi

fi

#EOF
