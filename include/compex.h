#pragma once
/* compex.h
 * --------
 * Macros for the use of compex.
 */

/* Tagging methods:
 *    [[scope::name(...)]]          Problems: no scoped lookup_attribute function
 *
 *    [[name(...)]]                 Problems: appears buggy, G++ crashes on
 *                                  some code even if the attribute handler doesn't do anything
 *
 *    __attribute__((name(...)))    Nonstandard syntax, but this doesn't matter
 *                                  anyway.
 *
 * Currently only the GCC syntax is supported.
 */
#if __COMPEX__ && defined(__cplusplus)
#  define COMPEX_TAG(...)  __attribute__((compex_tag(__VA_ARGS__)))
#else
#  define COMPEX_TAG(...)
#endif

// © 2014 Hugo Landau <hlandau@devever.net>        Licence: LGPLv3 or later
