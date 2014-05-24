#pragma once
/* compex.h
 * --------
 * Macros for the use of compex.
 */

#if __COMPEX__ && defined(__cplusplus)
#  define COMPEX_TAG(...)  [[compex::tag(__VA_ARGS__)]]
#else
#  define COMPEX_TAG(...)
#endif

// © 2014 Hugo Landau <hlandau@devever.net>        Licence: LGPLv3 or later
