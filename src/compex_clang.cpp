/* compex_clang.cpp
 * -----------------
 * A clang plugin for dumping annotated type information in YAML format.
 *
 * Load with:  clang++ -c \
 *               -Xclang -load -Xclang /path/to/compex_clang.so \
 *               -Xclang -plugin -Xclang compex_clang \
 *               [-Xclang -plugin-arg-compex_clang -Xclang -<ARG>=<VALUE>] ...
 *
 * Current options:
 *
 *   o=filename   Specify output filename for type information.
 *                Written to stdout if not specified or if specified as "-".
 *
 *   a            Print information about all types, not just tagged types.
 *
 * Supported attributes:
 *
 *   __attribute__((annotate("compex_tag ...")))
 *
 *     Since clang's plugin interface does not currently support adding new
 *     attributes, the generic annotate attribute is used. A single string
 *     must be specified which should start with "compex_tag " to disambiguate
 *     between multiple uses of annotate.
 *
 *     The attribute may be specified multiple times. The arguments to each
 *     invocation are kept separately and then aggregated in a list.
 *
 *     When used on structures, this also indicates that the structure's type
 *     information should be dumped. Structures are not dumped by default.
 *
 */

#define __STDC_CONSTANT_MACROS
#define __STDC_FORMAT_MACROS
#define __STDC_LIMIT_MACROS
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/AST/AST.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/Frontend/CompilerInstance.h>
#include <llvm/Support/raw_ostream.h>
#include <tuple>

#define BEGIN_NS(X) namespace X {
#define END_NS }
using namespace clang;

BEGIN_NS()

/* Consumer
 * --------
 */
struct Consumer :public ASTConsumer {
  Consumer(CompilerInstance &ci, raw_ostream *out);

  virtual bool HandleTopLevelDecl(DeclGroupRef dg);
  void SetDumpAll(bool dumpAll);

protected:
  raw_ostream &_Indent();
  bool _ShouldDump(const NamedDecl *d);
  void _HandleLocation(SourceLocation loc);
  void _HandleAttrs(const Decl *d);

  void _HandleNamedDecl(const NamedDecl *d);
  void _HandleRecordDecl(const RecordDecl *d);
  void _HandleFieldDecl(const FieldDecl *d);
  void _HandleFunctionDecl(const FunctionDecl *d);
  void _HandleParamDecl(const ParmVarDecl *d);
  void _HandleBaseSpecifier(const CXXBaseSpecifier *b);

  struct indent {
    indent(Consumer &c) :_c(c) {
      ++_c._indent;
    }
    ~indent() {
      --_c._indent;
    }
    Consumer &_c;
  };

  CompilerInstance &_ci;
  ASTContext &_ctx;
  raw_ostream *_out;
  int _indent;
  bool _dumpAll = false;
};
#define INDENT_SCOPE() indent _indenter(*this)

Consumer::Consumer(CompilerInstance &ci, raw_ostream *out)
  :_ci(ci), _ctx(ci.getASTContext()), _indent(0), _out(out) {}

raw_ostream &Consumer::_Indent() {
  for (int i=0; i<_indent; ++i)
    *_out << "  ";
  return *_out;
}

void Consumer::SetDumpAll(bool dumpAll) {
  _dumpAll = dumpAll;
}

bool Consumer::HandleTopLevelDecl(DeclGroupRef dg) {
  for (const Decl *d :dg) {
    const NamedDecl *nd = dyn_cast<NamedDecl>(d);
    if (!nd)
      continue;
    _HandleNamedDecl(nd);
  }

  _out->flush();
  return true;
}

void Consumer::_HandleLocation(SourceLocation loc) {
  auto &smgr = _ci.getSourceManager();
  _Indent() << "$srcFile: " << smgr.getBufferName(loc) << "\n";
  _Indent() << "$srcLine: " << smgr.getSpellingLineNumber(loc) << "\n";
}

bool Consumer::_ShouldDump(const NamedDecl *nd) {
  if (_dumpAll)
    return true;

  for (const Attr *a :nd->attrs()) {
    auto aa = dyn_cast<AnnotateAttr>(a);
    if (!aa)
      continue;

    if (aa->getAnnotation().startswith("compex_tag"))
      return true;
  }

  return false;
}

void Consumer::_HandleNamedDecl(const NamedDecl *nd) {
  auto kind = nd->getKind();
  const char *kindStr = NULL;

  if (!_ShouldDump(nd))
    return;

  switch (kind) {
    case Decl::Record:
    case Decl::CXXRecord:
    {
      _Indent() << nd->getNameAsString() << ": !compex/struct\n";
      auto rd = dyn_cast<RecordDecl>(nd);
      rd = rd->getDefinition();
      if (rd)
        _HandleRecordDecl(rd);
      break;
    }
    case Decl::Function:
    {
      _Indent() << nd->getNameAsString() << ": !compex/function\n";
      auto f = dyn_cast<FunctionDecl>(nd);
      if (f)
        _HandleFunctionDecl(f);
    }
    default:
      return;
  }
}

void Consumer::_HandleRecordDecl(const RecordDecl *d) {
  INDENT_SCOPE();
  _HandleLocation(d->getLocation());
  auto cxx_d = dyn_cast<CXXRecordDecl>(d);
  for (const FieldDecl *f :d->fields()) {
    _Indent() << f->getNameAsString() << ": !compex/field\n";
    _HandleFieldDecl(f);
  }

  if (cxx_d) {
    unsigned i=0;
    for (const CXXBaseSpecifier &b :cxx_d->bases()) {
      _Indent() << "base_" << i++ << "$: !compex/base\n";
      _HandleBaseSpecifier(&b);
    }

    i = 0;
    for (const CXXConstructorDecl *m :cxx_d->ctors()) {
      _Indent() << "method_" << i++ << "$: !compex/method\n";
      _HandleFunctionDecl(m);
    }
    for (const CXXMethodDecl *m :cxx_d->methods()) {
      _Indent() << "method_" << i++ << "$: !compex/method\n";
      _HandleFunctionDecl(m);
    }
  }

  _HandleAttrs(d);
}

void Consumer::_HandleFieldDecl(const FieldDecl *f) {
  INDENT_SCOPE();
  uint64_t size, align;
  QualType t = f->getType();
  std::tie(size,align) = _ctx.getTypeInfo(t);
  _Indent() << "name: " << f->getNameAsString() << "\n";
  _Indent() << "type: " << t.getAsString() << "\n";
  _Indent() << "size: " << size << "\n";
  _Indent() << "align: " << align << "\n";
  _Indent() << "offset: " << _ctx.getFieldOffset(f) << "\n";
  _HandleAttrs(f);
}

void Consumer::_HandleFunctionDecl(const FunctionDecl *f) {
  INDENT_SCOPE();
  const CXXMethodDecl *m = dyn_cast<CXXMethodDecl>(f);
  const CXXConstructorDecl *c = dyn_cast<CXXConstructorDecl>(f);
  const CXXDestructorDecl *d = dyn_cast<CXXDestructorDecl>(f);

  _Indent() << "name: " << f->getNameAsString() << "\n";

  if (f->isConstexpr())
    _Indent() << "constexpr: true\n";
  if (f->isDeleted())
    _Indent() << "deleted: true\n";
  if (f->isExternC())
    _Indent() << "externc: true\n";
  if (f->isNoReturn())
    _Indent() << "noreturn: true\n";
  if (f->isVariadic())
    _Indent() << "varargs: true\n";
  if (f->isImplicit())
    _Indent() << "implicit: true\n";

  if (m) {
    if (m->isStatic())
      _Indent() << "static: true\n";
    if (m->isConst())
      _Indent() << "const: true\n";
    if (m->isVirtual())
      _Indent() << "virtual: true\n";
  }

  if (c) {
    _Indent() << "constructor: true\n";
    if (c->isExplicit())
      _Indent() << "explicit: true\n";
    if (c->isDefaultConstructor())
      _Indent() << "default: true\n";
    if (c->isCopyConstructor())
      _Indent() << "copy: true\n";
    if (c->isMoveConstructor())
      _Indent() << "move: true\n";
  }

  if (d)
    _Indent() << "destructor: true\n";

  _Indent() << "args:\n";
  {
    INDENT_SCOPE();
    for (const ParmVarDecl *p :f->params()) {
      _Indent() << "- !compex/param\n";
      _HandleParamDecl(p);
    }
  }

  _HandleAttrs(f);
}

void Consumer::_HandleParamDecl(const ParmVarDecl *p) {
  INDENT_SCOPE();
  QualType t = p->getType();
  _Indent() << "name: " << p->getNameAsString() << "\n";
  _Indent() << "type: " << t.getAsString() << "\n";

  _HandleAttrs(p);
}

void Consumer::_HandleBaseSpecifier(const CXXBaseSpecifier *b) {
  INDENT_SCOPE();
  QualType t = b->getType();
  _Indent() << "type: " << t.getAsString() << "\n";
  if (b->isVirtual())
    _Indent() << "virtual: true\n";
}
  
void Consumer::_HandleAttrs(const Decl *d) {
  _Indent() << "attrs:\n";
  for (const Attr *a :d->attrs()) {
    INDENT_SCOPE();
    _Indent() << "-\n";
    {
      INDENT_SCOPE();
      _Indent() << "name: " << a->getSpelling() << "\n";
      auto aa = dyn_cast<AnnotateAttr>(a);
      if (aa)
        _Indent() << "value: " << aa->getAnnotation() << "\n";
    }
  }
}

/* Plugin
 * ------
 */
struct Plugin :public PluginASTAction {
  ASTConsumer *CreateASTConsumer(CompilerInstance &ci, llvm::StringRef x);
  bool ParseArgs(const CompilerInstance &ci, const std::vector<std::string> &args);
  void PrintHelp(llvm::raw_ostream &ros);

protected:
  std::string _outputfn;
  llvm::raw_fd_ostream *_outfd;
  llvm::raw_ostream *_out;
  bool _dumpAll = false;
};

ASTConsumer
*Plugin::CreateASTConsumer(CompilerInstance &ci, llvm::StringRef x) {
  std::string errinfo;
  if (_outputfn.size()) {
    _outfd = new llvm::raw_fd_ostream(_outputfn.c_str(), errinfo, llvm::sys::fs::F_None);
    _out = _outfd;
  } else
    _out = &llvm::outs();

  if (!_out) {
    llvm::errs() << "Could not open compex output file: " << errinfo << "\n";
    return NULL;
  }

  auto c = new Consumer(ci, _out);

  if (_dumpAll)
    c->SetDumpAll(true);

  return c;
}

bool Plugin::ParseArgs(
    const CompilerInstance &ci,
    const std::vector<std::string> &args) {
  for (auto &arg :args) {
    if (arg.size() > 3 && arg.substr(0,3) == "-o=") {
      if (_outputfn.size()) {
        llvm::errs() << "compex_clang: Output filename must not be specified more than once\n";
        return false;
      }

      _outputfn = arg.substr(3);
    } else if (arg == "-a")
      _dumpAll = true;
    else
      PrintHelp(llvm::errs());
  }

  return true;
}

void Plugin::PrintHelp(llvm::raw_ostream &ros) {
  ros << "compex_clang\n";
  ros << "  Supported options:\n";
  ros << "  [-Xclang] -plugin-arg-compex_clang [-Xclang] -o=<output filename>   (default: stdout)\n";
  ros << "    Write YAML output to the specified file instead of stdout.\n";
  ros << "  [-Xclang] -plugin-arg-compex_clang [-Xclang] -a\n";
  ros << "    Dump information for all types, not just tagged types.\n";
  ros << "\n";
}

END_NS

/* Autoregistration
 * ----------------
 */
static FrontendPluginRegistry::Add<Plugin> _plugin("compex_clang", "Type information dumping plugin.");

// © 2015 Hugo Landau <hlandau@devever.net>      UoI-NCSA License
