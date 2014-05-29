/* compex_gcc.cpp
 * --------------
 * A GCC plugin for dumping annotated type information in YAML format.
 *
 * Load with:  g++ -c -std=gnu++11 -fplugin=/path/to/compex_gcc.so \
 *                  -fplugin-arg-compex_gcc-<ARG>=<VALUE> ...
 *
 * Current options:
 *
 *   o=filename   Specify output filename for type information.
 *                Written to stdout if not specified or if specified as "-".
 *
 * Supported attributes:
 *
 *   [[compex::tag(...)]]   The zero or more arguments specified must each be
 *                          either string literals or integers. If a nonzero
 *                          number of arguments is specified, the arguments
 *                          form a list of literals which are attached as
 *                          metadata to the object to which the attribute
 *                          attaches.
 *
 *                          The attribute may be specified multiple times. The
 *                          arguments to each invocation are kept in separate
 *                          lists, which are then aggregated in a list of
 *                          lists. Note that this list will not contain any
 *                          empty lists.
 *
 *                          When used on structures, this also indicates that
 *                          the structure's type information should be dumped.
 *                          Structures are not dumped by default.
 */
#include "config.h"
#include "gcc-plugin.h"
#include "tree.h"
#include "cp/cp-tree.h"
#include "diagnostic.h"
#include "plugin.h"
#include "plugin-version.h"
#include "intl.h"
#include <stdio.h>
#include <stdint.h>
#include <unordered_set>

#define VERSION "compex_gcc v1"
#define LOGF(...) fprintf(stderr,    "# COMPEX_GCC: " __VA_ARGS__)
#define OUTF(...) fprintf(_output_f, "" __VA_ARGS__)

static uint32_t _counter = 0;
static FILE *_output_f = stdout;

static void _indent(int n) {
  for (int i=0;i<n;++i)
    fprintf(_output_f, "  ");
}

static const char *_access_to_str(void *access) {
       if (access == access_public_node)      return "public";
  else if (access == access_protected_node)   return "protected";
  else if (access == access_private_node)     return "private";
  else                                        return NULL;
}

/* _handle_tag_attr
 * ----------------
 * Called by gcc upon encountering [[compex::tag()]] attribute.
 */
static tree
_handle_tag_attr(tree *node, tree attr_name, tree attr_arguments,
                              int flags, bool *no_add_attrs) {
  *no_add_attrs = false;
  return NULL_TREE;
}

/* _dump_tags
 * ----------
 * Output tag metadata for a node.
 */
static void
_dump_tags(tree arg, int ind) {
  bool outt = false;

  for (tree tag = lookup_attribute("tag", TYPE_ATTRIBUTES(arg)); tag != NULL_TREE; tag = TREE_CHAIN(tag)) {
    tree tagargs = TREE_VALUE(tag);
    bool out = false;
    for (tree tagarg = tagargs; tagarg != NULL_TREE; tagarg = TREE_CHAIN(tagarg)) {
      int iv;
      tree v = TREE_VALUE(tagarg);
      if (!outt) {
        _indent(ind);
        OUTF("tags:\n");
        outt = true;
      }
      if (!out) {
        _indent(ind+1);
        OUTF("-\n");
        out = true;
      }
      switch (TREE_CODE(v)) {
        case STRING_CST:
          _indent(ind+2);
          OUTF("- %s\n", TREE_STRING_POINTER(v));
          break;
        case INTEGER_CST:
          if (tree_fits_shwi_p(v)) {
            iv = tree_to_shwi(v);
            _indent(ind+2);
            OUTF("- %d\n", iv);
          } else {
            LOGF("integer doesn't fit");
          }
          break;
        default:
          LOGF("unknown type code for attribute argument: %s\n", get_tree_code_name(TREE_CODE(v)));
          break;
      }
    }
  }
}

/* _mangle_typename
 * ----------------
 * Warning: uses static storage for returned string.
 */
const char *_mangle_typename(tree type) {
#define MANGLE_STR_LEN 1024
  static char name[MANGLE_STR_LEN] = {};
  static unsigned unk_i = 0;

  switch (TREE_CODE(type)) {
    case RECORD_TYPE:
      name[0] = 's';
      name[1] = '_';
      strncpy(name+2, IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(type))), MANGLE_STR_LEN-2);
      break;
    default:
      sprintf(name, "unknown_%u", ++unk_i);
      break;
  }
  return name;
}

static std::unordered_set<std::string> _mangled_names;
const char *_mangle_typename_def(tree type) {
  const char *v = _mangle_typename(type);
  _mangled_names.insert(v);
  return v;
}

const char *_mangle_typename_ref(tree type) {
  const char *v = _mangle_typename(type);
  if (_mangled_names.count(v))
    return v;
  else
    return NULL;
}

/* _finish_type
 * ------------
 * Output type information on nodes which have at least one compex::tag
 * attribute attached.
 */
static void
_finish_type(void *event_data, void *data) {
  tree type = (tree)event_data;

  if (TREE_CODE(type) != RECORD_TYPE)
    return;

  type = TYPE_MAIN_VARIANT(type);

  // TODO: find way to lookup in de:: namespace
  if (!lookup_attribute("compex_tag", TYPE_ATTRIBUTES(type)))
    return;

  if (!COMPLETE_TYPE_P(type)) {
    error(G_("COMPEX: incomplete finished type"));
    return;
  }

  tree decl = TYPE_NAME(type);
  const char *struct_name = IDENTIFIER_POINTER(DECL_NAME(decl));
  const char *field_name;
  tree sizeof_const, offset_const, boffset_const, oalign_const;
  tree fdeclname;
  int sizeof_v;
  unsigned offset_v, boffset_v, oalign_v;
  char fnamebuf[64];

  OUTF("%s: &%s !compex/struct\n", struct_name, _mangle_typename_def(type));
  OUTF("  $srcFile: %s\n", DECL_SOURCE_FILE(decl));
  OUTF("  $srcLine: %u\n", DECL_SOURCE_LINE(decl));

  sizeof_v = (tree_fits_shwi_p(TYPE_SIZE(type)) ? tree_to_shwi(TYPE_SIZE(type)) : -1);
  OUTF("  $sizeof: %u\n", sizeof_v);
  OUTF("  $alignof: %u\n", TYPE_ALIGN(type));

  _dump_tags(type, 1);

  tree biv = TYPE_BINFO(type);
  tree bi;
  size_t n = biv ? BINFO_N_BASE_BINFOS(biv) : 0;
  for (size_t i=0;i<n;++i) {
    bi = BINFO_BASE_BINFO(biv,i);
    OUTF("  base_%u$: !compex/base\n", i);

    OUTF("    access: %s\n",
      _access_to_str(BINFO_BASE_ACCESSES(biv) ? BINFO_BASE_ACCESS(biv, i) : access_public_node));

    if (BINFO_VIRTUAL_P(bi))
      OUTF("    virtual: true\n");

    tree btype  = TYPE_MAIN_VARIANT(BINFO_TYPE(bi));
    tree bdecl  = TYPE_NAME(btype);
    tree bid    = DECL_NAME(bdecl);
    OUTF("    name: %s\n", IDENTIFIER_POINTER(bid));
    const char *mref = _mangle_typename_ref(btype);
    if (mref)
      OUTF("    ref: *%s\n", mref);
  }

  for (tree arg = TYPE_FIELDS(type); arg != NULL_TREE; arg = TREE_CHAIN(arg)) {
    switch (TREE_CODE(arg)) {
      case FIELD_DECL:
        fdeclname = DECL_NAME(arg);
        if (fdeclname) {
          field_name = IDENTIFIER_POINTER(fdeclname);
        } else {
          sprintf(fnamebuf, "anon_%u$", ++_counter);
          field_name = fnamebuf;
        }
        sizeof_const = DECL_SIZE(arg);
        offset_const = DECL_FIELD_OFFSET(arg);
        boffset_const = DECL_FIELD_BIT_OFFSET(arg);
        sizeof_v = (tree_fits_shwi_p(sizeof_const) ? tree_to_shwi(sizeof_const) : -1);
        offset_v = (offset_const && tree_fits_uhwi_p(offset_const) ? tree_to_uhwi(offset_const) : -1);
        boffset_v = (boffset_const && tree_fits_uhwi_p(boffset_const) ? tree_to_uhwi(boffset_const) : -1);

        OUTF("  %s: !compex/field\n", field_name);
        if (fdeclname)
          OUTF("    name: %s\n", field_name);
        OUTF("    size: %d\n", sizeof_v);
        OUTF("    align: %d\n", DECL_ALIGN(arg));
        OUTF("    offset: %u\n", offset_v);
        OUTF("    boffset: %u\n", boffset_v);
        OUTF("    oalign: %u\n", DECL_OFFSET_ALIGN(arg));
        if (DECL_ARTIFICIAL(arg))
          OUTF("    artificial: true\n");
        if (!fdeclname)
          OUTF("    unknown: true\n");
        if (DECL_C_BIT_FIELD(arg))
          OUTF("    bitfield: true\n");
        _dump_tags(TREE_TYPE(arg), 2);
        break;
      case TYPE_DECL:
        break;
      default:
        LOGF("Unexpected member of a struct: %s, ignoring\n", get_tree_code_name(TREE_CODE(arg)));
        break;
    }
  }

  int i=0;
  for (tree arg = TYPE_METHODS(type); arg != NULL_TREE; arg = TREE_CHAIN(arg)) {
    if (TREE_CODE(arg) != FUNCTION_DECL) {
      LOGF("Got method which is not a function: %s, ignoring\n", get_tree_code_name(TREE_CODE(arg)));
      continue;
    }

    ++i;
    const char *method_name = IDENTIFIER_POINTER(DECL_NAME(arg));
    const char *mangled_name = IDENTIFIER_POINTER(DECL_ASSEMBLER_NAME(arg));
    OUTF("  method_%u$: !compex/method\n", i);
    OUTF("    name: %s\n", method_name);
    OUTF("    asm: %s\n", mangled_name);
    if (DECL_VIRTUAL_P(arg))
      OUTF("    virtual: true\n");
    if (DECL_ARTIFICIAL(arg))
      OUTF("    artificial: true\n");
    if (DECL_CONST_MEMFUNC_P(arg))
      OUTF("    const: true\n");
    if (DECL_STATIC_FUNCTION_P(arg))
      OUTF("    static: true\n");
    if (DECL_CONSTRUCTOR_P(arg))
      OUTF("    constructor: true\n");
    if (DECL_DESTRUCTOR_P(arg))
      OUTF("    destructor: true\n");
    if (DECL_COPY_CONSTRUCTOR_P(arg))
      OUTF("    copy_constructor: true\n");
    if (DECL_BASE_CONSTRUCTOR_P(arg))
      OUTF("    base_constructor: true\n");
    if (DECL_COMPLETE_CONSTRUCTOR_P(arg))
      OUTF("    complete_constructor: true\n");
    if (DECL_COMPLETE_DESTRUCTOR_P(arg))
      OUTF("    complete_destructor: true\n");
    if (DECL_OVERLOADED_OPERATOR_P(arg))
      OUTF("    operator: true\n");
    if (DECL_CONV_FN_P(arg))
      OUTF("    cast_operator: true\n");
    if (DECL_THUNK_P(arg))
      OUTF("    thunk: true\n");
    if (TYPE_NOTHROW_P(TREE_TYPE(arg)))
      OUTF("    nothrow: true\n");
    _dump_tags(TREE_TYPE(arg), 2);
  }
}

/* Attribute Registration
 * ----------------------
 */
static const attribute_spec _attributes[] = {
  { "compex_tag", 0, -1, false, true, false, _handle_tag_attr, false },
  NULL
};

static void
_register_attributes(void *event_data, void *user_data) {
  register_attribute(&_attributes[0]);

  /* register_scoped_attributes allows things like [[compex::tag]], but appears
   * to be buggy at this time (causes segfaults, even if nothing is done in the
   * attribute handler).
   */
  //register_scoped_attributes(_attributes, "compex");
}

/* plugin_init
 * -----------
 * Main plugin entrypoint called by gcc.
 */
static const struct plugin_info _plugin_info = {
  VERSION,
  VERSION ": Output C++ type information.",
};

int
plugin_init(plugin_name_args *info, plugin_gcc_version *ver) {
  const char *k, *v;

  // Check gcc version.
  if (!plugin_default_version_check(ver, &gcc_version)) {
    LOGF("Version mismatch.\n");
    return 1;
  }

  // Argument parsing.
  for (int i=0; i < info->argc; ++i) {
    k = info->argv[i].key, v = info->argv[i].value;
    if (!strcmp(k, "o")) {
      if (strcmp(v, "-")) {
        _output_f = fopen(v, "w");
        if (!_output_f) {
          LOGF("Could not open output file: %s\n", v);
          return 1;
        }
      }
    } else {
      LOGF("Unknown argument: %s\n", k);
      return 1;
    }
  }

  // Setup callbacks.
  register_callback(info->base_name, PLUGIN_INFO, NULL, (void*)&_plugin_info);
  register_callback(info->base_name, PLUGIN_ATTRIBUTES, &_register_attributes, NULL);
  register_callback(info->base_name, PLUGIN_FINISH_TYPE, &_finish_type, NULL);

  return 0;
}

int plugin_is_GPL_compatible;

// © 2014 Hugo Landau <hlandau@devever.net>        Licence: LGPLv3 or later
