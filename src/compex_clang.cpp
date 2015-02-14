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
  Consumer(CompilerInstance &ci);
  virtual bool HandleTopLevelDecl(DeclGroupRef dg);

protected:
  raw_ostream &_Indent();
  void _HandleNamedDecl(const NamedDecl *d);
  void _HandleLocation(SourceLocation loc);
  void _HandleRecordDecl(const RecordDecl *d);
  void _HandleFieldDecl(const FieldDecl *d);
  void _HandleFunctionDecl(const FunctionDecl *d);
  void _HandleParamDecl(const ParmVarDecl *d);
  void _HandleBaseSpecifier(const CXXBaseSpecifier *b);
  void _HandleAttrs(const Decl *d);

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
  int _indent;
};
#define INDENT_SCOPE() indent _indenter(*this)

Consumer::Consumer(CompilerInstance &ci)
  :_ci(ci), _ctx(ci.getASTContext()), _indent(0) {}

raw_ostream &Consumer::_Indent() {
  for (int i=0; i<_indent; ++i)
    llvm::errs() << "  ";
  return llvm::errs();
}

bool Consumer::HandleTopLevelDecl(DeclGroupRef dg) {
  for (const Decl *d :dg) {
    const NamedDecl *nd = dyn_cast<NamedDecl>(d);
    if (!nd)
      continue;
    _HandleNamedDecl(nd);
  }

  return true;
}

void Consumer::_HandleLocation(SourceLocation loc) {
  auto &smgr = _ci.getSourceManager();
  _Indent() << "$srcFile: " << smgr.getBufferName(loc) << "\n";
  _Indent() << "$srcLine: " << smgr.getSpellingLineNumber(loc) << "\n";
}

void Consumer::_HandleNamedDecl(const NamedDecl *nd) {
  auto kind = nd->getKind();
  const char *kindStr = NULL;

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

/*
  method_1$: !compex/method
    name: SubClass
    asm: _ZN8SubClassC4Ev
    artificial: true
    constructor: true
    nothrow: true
  method_2$: !compex/method
    name: __base_ctor 
    asm: _ZN8SubClassC2Ev
    artificial: true
    constructor: true
    base_constructor: true
    nothrow: true
  method_3$: !compex/method
    name: __comp_ctor 
    asm: _ZN8SubClassC1Ev
    artificial: true
    constructor: true
    complete_constructor: true
    nothrow: true
  method_4$: !compex/method
    name: DoSomething
    asm: _ZN8SubClass11DoSomethingEii
    virtual: true
  method_5$: !compex/method
    name: DoSomething
    asm: _ZNK8SubClass11DoSomethingEii
    virtual: true
    const: true
    tags:
      -
        - special_method
  method_6$: !compex/method
    name: stuff
    asm: _ZN8SubClass5stuffEv
    static: true
  method_7$: !compex/method
    name: f1
    asm: _ZN8SubClass2f1Ev
  method_8$: !compex/method
    name: f2
    asm: _ZN8SubClass2f2Ev
    nothrow: true
    */
/* Plugin
 * ------
 */
struct Plugin :public PluginASTAction {
  ASTConsumer *CreateASTConsumer(CompilerInstance &ci, llvm::StringRef x);
  bool ParseArgs(const CompilerInstance &ci, const std::vector<std::string> &args);
  void PrintHelp(llvm::raw_ostream &ros);
};

ASTConsumer
*Plugin::CreateASTConsumer(CompilerInstance &ci, llvm::StringRef x) {
  return new Consumer(ci);
}

bool Plugin::ParseArgs(
    const CompilerInstance &ci,
    const std::vector<std::string> &args) {
  for (auto &arg :args) {
    llvm::errs() << "CompexClang arg: " << arg << "\n";
  }

  if (!args.empty() && args[0] == "help")
    PrintHelp(llvm::errs());

  return true;
}

void Plugin::PrintHelp(llvm::raw_ostream &ros) {
  ros << "compex_clang\n";
  ros << "  Supported options:\n";
  ros << "  [-Xclang] -plugin-arg-compex_clang-o=<output filename>   (default: stdout)\n";
  ros << "\n";
}

END_NS

static FrontendPluginRegistry::Add<Plugin> _plugin("compex_clang", "Reflection plugin.");
