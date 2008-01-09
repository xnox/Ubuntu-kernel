/* automatically generated from crt.c */
#ifdef CONFIG_X86_64

WIN_FUNC_DECL(_alldiv, 2)
WIN_FUNC_DECL(_allmul, 2)
WIN_FUNC_DECL(_allrem, 2)
WIN_FUNC_DECL(_allshl, 2)
WIN_FUNC_DECL(_allshr, 2)
WIN_FUNC_DECL(_aulldiv, 2)
WIN_FUNC_DECL(_aullmul, 2)
WIN_FUNC_DECL(_aullrem, 2)
WIN_FUNC_DECL(_aullshl, 2)
WIN_FUNC_DECL(_aullshr, 2)
WIN_FUNC_DECL(rand, 0)
WIN_FUNC_DECL(swprintf, 12)
WIN_FUNC_DECL(_win_atoi, 1)
WIN_FUNC_DECL(_win_isprint, 1)
WIN_FUNC_DECL(_win_memchr, 3)
WIN_FUNC_DECL(_win_memcmp, 3)
WIN_FUNC_DECL(_win_memcpy, 3)
WIN_FUNC_DECL(_win_memmove, 3)
WIN_FUNC_DECL(_win_memset, 3)
WIN_FUNC_DECL(_win__snprintf, 12)
WIN_FUNC_DECL(_win_snprintf, 12)
WIN_FUNC_DECL(_win_sprintf, 12)
WIN_FUNC_DECL(_win_srand, 1)
WIN_FUNC_DECL(_win_strchr, 2)
WIN_FUNC_DECL(_win_strcmp, 2)
WIN_FUNC_DECL(_win_strcpy, 2)
WIN_FUNC_DECL(_win_stricmp, 2)
WIN_FUNC_DECL(_win_strlen, 1)
WIN_FUNC_DECL(_win_strncat, 3)
WIN_FUNC_DECL(_win_strncmp, 3)
WIN_FUNC_DECL(_win_strncpy, 3)
WIN_FUNC_DECL(_win_strrchr, 2)
WIN_FUNC_DECL(_win_strstr, 2)
WIN_FUNC_DECL(_win_tolower, 1)
WIN_FUNC_DECL(_win_toupper, 1)
WIN_FUNC_DECL(_win_towlower, 1)
WIN_FUNC_DECL(_win_towupper, 1)
WIN_FUNC_DECL(_win__vsnprintf, 4)
WIN_FUNC_DECL(_win_vsnprintf, 4)
WIN_FUNC_DECL(_win_vsprintf, 3)
WIN_FUNC_DECL(_win_wcscat, 2)
WIN_FUNC_DECL(_win_wcscmp, 2)
WIN_FUNC_DECL(_win_wcscpy, 2)
WIN_FUNC_DECL(_win_wcsicmp, 2)
WIN_FUNC_DECL(_win_wcslen, 1)
WIN_FUNC_DECL(_win_wcsncpy, 3)
#endif
struct wrap_export crt_exports[] = {
   
   WIN_SYMBOL(_alldiv,2),
   WIN_SYMBOL(_allmul,2),
   WIN_SYMBOL(_allrem,2),
   WIN_SYMBOL(_allshl,2),
   WIN_SYMBOL(_allshr,2),
   WIN_SYMBOL(_aulldiv,2),
   WIN_SYMBOL(_aullmul,2),
   WIN_SYMBOL(_aullrem,2),
   WIN_SYMBOL(_aullshl,2),
   WIN_SYMBOL(_aullshr,2),
   WIN_SYMBOL(rand,0),
   WIN_SYMBOL(swprintf,12),
   WIN_WIN_SYMBOL(atoi,1),
   WIN_WIN_SYMBOL(isprint,1),
   WIN_WIN_SYMBOL(memchr,3),
   WIN_WIN_SYMBOL(memcmp,3),
   WIN_WIN_SYMBOL(memcpy,3),
   WIN_WIN_SYMBOL(memmove,3),
   WIN_WIN_SYMBOL(memset,3),
   WIN_WIN_SYMBOL(_snprintf,12),
   WIN_WIN_SYMBOL(snprintf,12),
   WIN_WIN_SYMBOL(sprintf,12),
   WIN_WIN_SYMBOL(srand,1),
   WIN_WIN_SYMBOL(strchr,2),
   WIN_WIN_SYMBOL(strcmp,2),
   WIN_WIN_SYMBOL(strcpy,2),
   WIN_WIN_SYMBOL(stricmp,2),
   WIN_WIN_SYMBOL(strlen,1),
   WIN_WIN_SYMBOL(strncat,3),
   WIN_WIN_SYMBOL(strncmp,3),
   WIN_WIN_SYMBOL(strncpy,3),
   WIN_WIN_SYMBOL(strrchr,2),
   WIN_WIN_SYMBOL(strstr,2),
   WIN_WIN_SYMBOL(tolower,1),
   WIN_WIN_SYMBOL(toupper,1),
   WIN_WIN_SYMBOL(towlower,1),
   WIN_WIN_SYMBOL(towupper,1),
   WIN_WIN_SYMBOL(_vsnprintf,4),
   WIN_WIN_SYMBOL(vsnprintf,4),
   WIN_WIN_SYMBOL(vsprintf,3),
   WIN_WIN_SYMBOL(wcscat,2),
   WIN_WIN_SYMBOL(wcscmp,2),
   WIN_WIN_SYMBOL(wcscpy,2),
   WIN_WIN_SYMBOL(wcsicmp,2),
   WIN_WIN_SYMBOL(wcslen,1),
   WIN_WIN_SYMBOL(wcsncpy,3),
   {NULL, NULL}
};
