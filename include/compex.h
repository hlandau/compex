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
 * Currently only the GCC syntax is supported on GCC.
 *
 * Since clang does not support custom attributes added by plugins, the annotation
 * attribute has to be reused:
 *
 *    __attribute__((annotate(...)))
 *
 */
#if __COMPEX__ && defined(__cplusplus)
#  if defined(__clang__)
#    define COMPEX_TAG(...)  __attribute__((annotate("compex_tag " #__VA_ARGS__)))
#  else
#    define COMPEX_TAG(...)  __attribute__((compex_tag(__VA_ARGS__)))
#  endif
#else
#  define COMPEX_TAG(...)
#endif

// 2014 Hugo Landau <hlandau@devever.net>          Public Domain
