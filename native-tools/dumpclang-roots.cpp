#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

#include <iostream>
#include <map>
#include <set>
#include <stdio.h>
#include <stdlib.h>

using namespace clang;
using namespace std;

class ReflectObject;

struct ReflectValue {
#define REFLECTABLE_VALUES \
  REFLECTABLE(string, string) \
  REFLECTABLE(bool, bool) \
  REFLECTABLE(SourceRange, range) \
  REFLECTABLE(SourceLocation, location) \
  REFLECTABLE(vector<ReflectValue>, list) \
  REFLECTABLE(ReflectObject *, object)

  enum {
#define REFLECTABLE(type, typestr) k##typestr,
    REFLECTABLE_VALUES
#undef REFLECTABLE
    kLastValue
  } tag;
#define REFLECTABLE(type, typestr) type m##typestr;
    REFLECTABLE_VALUES
#undef REFLECTABLE
#define REFLECTABLE(type, typestr) \
  ReflectValue(type && a##typestr) \
  : tag(k##typestr), m##typestr(a##typestr) {} \
  ReflectValue(type & a##typestr) \
  : tag(k##typestr), m##typestr(a##typestr) {} \
  ReflectValue &operator =(type &&a##typestr) { \
    tag = k##typestr; \
    m##typestr = a##typestr; \
    return *this; \
  }
  REFLECTABLE_VALUES
#undef REFLECTABLE
  ReflectValue() : tag(kLastValue) {}
  ReflectValue(const char *str) : tag(kstring), mstring(str) {}
  ReflectValue &operator =(const char *str) {
    tag = kstring;
    mstring = str;
    return *this;
  }
};

class ReflectObject {
public:
  // A suitable name for the object
  const char *m_realName;
  map<const char *, ReflectValue> m_values;
};
// The list of all Declarations
map<Decl*, ReflectObject*> declMap;
map<TypeLoc, ReflectObject*> typeLocMap;

class ASTDumper;

class ASTDumper : public ASTConsumer,
    public RecursiveASTVisitor<ASTDumper> {
private:
  SourceManager &sm;

  ReflectObject *object;
public:
  ASTDumper(CompilerInstance &ci) :
      sm(ci.getSourceManager()) {
  }

  ReflectObject *thunk(const Decl *d) {
    if (d == NULL)
      return NULL;
    ReflectObject *res = declMap[const_cast<Decl*>(d)];
    if (res == NULL) {
      declMap[const_cast<Decl*>(d)] = res = new ReflectObject;
      res->m_realName = NULL;
    }
    return res;
  }
  ReflectObject *thunk(TypeSourceInfo *tsi) {
    ReflectObject *obj = new ReflectObject;
    obj->m_realName = "<em>Name not found</em>";
    obj->m_values["Type"] = (ReflectObject*)NULL; // QualType
    obj->m_values["TypeLoc"] = thunk(tsi->getTypeLoc());
    return obj;
  }
  ReflectObject *thunk(TypeLoc t) {
    ReflectObject *res = typeLocMap[t];
    if (res == NULL) {
      typeLocMap[t] = res = new ReflectObject;
      res->m_realName = NULL;
    }
    return res;
  }
  ReflectObject *thunk2(DeclContext *d) {
    if (!d) return NULL;
    return thunk(Decl::castFromDeclContext(d));
  }

  ReflectValue listify(const Decl *d) {
    return ReflectValue(thunk(d));
  }
  ReflectValue listify(const SourceLocation loc) {
    return ReflectValue(loc);
  }
  ReflectValue listify(const CXXBaseSpecifier &spec) {
    ReflectObject *obj = new ReflectObject;
    obj->m_realName = "CXXBaseSpecifier";
    obj->m_values["SourceRange"] = spec.getSourceRange();
    obj->m_values["Virtual"] = spec.isVirtual();
    obj->m_values["BaseOfClass"] = spec.isBaseOfClass();
    obj->m_values["PackExpansion"] = spec.isPackExpansion();
    obj->m_values["InheritConstructors"] = spec.getInheritConstructors();
    obj->m_values["EllipsisLoc"] = spec.getEllipsisLoc();
    obj->m_values["AccessSpecifier"] = spec.getAccessSpecifier();
    obj->m_values["AccessSpecifierAsWritten"] =
      spec.getAccessSpecifierAsWritten();
    return ReflectValue(obj);
  }
  // XXX!
  ReflectValue listify(QualType t) {return ReflectValue((ReflectObject*)NULL);}
  ReflectValue listify(CXXCtorInitializer *t) {return ReflectValue((ReflectObject*)NULL);}
  ReflectValue listify(const BlockDecl::Capture &t) {return ReflectValue((ReflectObject*)NULL);}
  template <typename T>
  ReflectValue makeList(T begin, T end) {
    vector<ReflectValue> list;
    for (T it = begin; it != end; it++) {
      list.push_back(listify(*it));
    }
    return ReflectValue(list);
  }
  
  // All we need is to follow the final declaration.
  virtual void HandleTranslationUnit(ASTContext &ctx) {
    TraverseDecl(ctx.getTranslationUnitDecl());

    // Output the data
    cout << "// HandleTranslationUnit" << endl;
    set<ReflectObject*> seen;
    printObject(declMap[ctx.getTranslationUnitDecl()], seen);
    printf("\n");
  }

  virtual bool VisitDecl(Decl *d) {
    object = thunk(d);
    object->m_realName = "Decl";
    object->m_values["DeclKindName"] = d->getDeclKindName();
    if (!TranslationUnitDecl::classof(d)) {
      object->m_values["SourceRange"] = d->getSourceRange();
      object->m_values["Location"] = d->getLocation();
      object->m_values["DeclContext"] = thunk2(d->getDeclContext());
      object->m_values["NonClosureContext"] = thunk2(d->getNonClosureContext());
      object->m_values["inAnonymousNamespace"] = d->isInAnonymousNamespace();
    }

    return true;
  }
  virtual bool VisitTypeLoc(TypeLoc l) {
    object = thunk(l);
    object->m_realName = "TypeLoc";
    object->m_values["BeginLoc"] = l.getBeginLoc();
    object->m_values["EndLoc"] = l.getEndLoc();
    object->m_values["SourceRange"] = l.getSourceRange();
    object->m_values["LocalSourceRange"] = l.getLocalSourceRange();
    object->m_values["NextTypeLoc"] = thunk(l.getNextTypeLoc());
    object->m_values["UnqualifiedLoc"] = thunk(l.getUnqualifiedLoc());
    object->m_values["IgnoreParens"] = thunk(l.IgnoreParens());
    return true;
  }
  void VisitDeclContext(DeclContext *dc) {
    object->m_values["Parent"] = thunk2(dc->getParent());
    object->m_values["LexicalParent"] = thunk2(dc->getLexicalParent());
    object->m_values["LookupParent"] = thunk2(dc->getLookupParent());
    object->m_values["decls"] = makeList(dc->decls_begin(), dc->decls_end());
  }

#include "dumpclang-gen-info.h"
  void printObject(ReflectObject *object, set<ReflectObject *> &seen);
  void printSourceLocation(SourceLocation loc);
  void printValue(ReflectValue &value, set<ReflectObject *> &seen);
};

std::string cgiEscape(const char *str) {
  std::string result;
  while (*str) {
    if (*str == '<') result += "&lt;";
    else if (*str == '>') result += "&gt;";
    else if (*str == '&') result += "&amp;";
    else result += *str;
    str++;
  }
  return result;
}

std::string indent;
void pushIndent() {indent += "  ";}
void popIndent() {indent.erase(indent.length() - 2);}
void ASTDumper::printObject(ReflectObject *object, set<ReflectObject *> &seen) {
  if (!object) {
    printf("null");
    return;
  }
  if (!seen.insert(object).second) {
    printf("<a href=\"#%p\">Object %p</a>", object, object);
    return;
  }
  pushIndent();
  printf("<a id=\"%p\">{</a>\n%s", object, indent.c_str());
  printf("_kind: \"%s\"", object->m_realName);
  for (map<const char *, ReflectValue>::iterator it = object->m_values.begin();
      it != object->m_values.end(); it++) {
    printf(",\n%s", indent.c_str());
    printf("\"%s\": ", it->first);
    printValue(it->second, seen);
  }
  popIndent();
  printf("\n%s}", indent.c_str());
}

void ASTDumper::printSourceLocation(SourceLocation loc) {
  SourceLocation instantiation = sm.getInstantiationLoc(loc);
  SourceLocation spelling = sm.getSpellingLoc(loc);
  printf("{\n");
#define PRINT_LOC(name) \
  do { \
    printf("%s  " #name ": {\n", indent.c_str()); \
    printf("%s    file: \"%s\",\n", indent.c_str(), \
      cgiEscape(sm.getBufferName(name)).c_str()); \
    pair<FileID, unsigned> lpair = sm.getDecomposedLoc(name); \
    printf("%s    line: %d,\n", indent.c_str(), \
      sm.getLineNumber(lpair.first, lpair.second)); \
    printf("%s    column: %d,\n", indent.c_str(), \
      sm.getColumnNumber(lpair.first, lpair.second)); \
    printf("%s  },\n", indent.c_str()); \
  } while(false)
  PRINT_LOC(instantiation);
  if (spelling != instantiation)
    PRINT_LOC(spelling);
#undef PRINT_LOC
  PresumedLoc presumed = sm.getPresumedLoc(loc);
  printf("%s  presumed: {\n", indent.c_str());
  printf("%s    file: \"%s\",\n", indent.c_str(), presumed.getFilename());
  printf("%s    line: %d,\n", indent.c_str(), presumed.getLine());
  printf("%s    column: %d,\n", indent.c_str(), presumed.getColumn());
  printf("%s  }\n", indent.c_str());
  printf("%s}", indent.c_str());
}

void ASTDumper::printValue(ReflectValue &value, set<ReflectObject *> &seen) {
  switch (value.tag) {
    case ReflectValue::kstring:
      cout << '"' << value.mstring << '"';
      break;
    case ReflectValue::kbool:
      printf("%s", value.mbool ? "true" : " false");
      break;
    case ReflectValue::krange:
      printf("[");
      printSourceLocation(value.mrange.getBegin());
      printf(",");
      printSourceLocation(value.mrange.getEnd());
      printf("]");
      break;
    case ReflectValue::klocation:
      printSourceLocation(value.mlocation);
      break;
    case ReflectValue::klist: {
      vector<ReflectValue>::iterator lit;
      pushIndent();
      printf("[");
      for (lit = value.mlist.begin(); lit < value.mlist.end();
          lit++) {
        if (lit == value.mlist.begin())
          printf("\n%s", indent.c_str());
        else
          printf(",\n%s", indent.c_str());
        printValue(*lit, seen);
      }
      popIndent();
      printf("]");
      break;
    }
    case ReflectValue::kobject:
      printObject(value.mobject, seen);
      break;
    default:
      exit(1);
  }
}

class DumpClangAction : public PluginASTAction {
protected:
  ASTConsumer *CreateASTConsumer(CompilerInstance &CI, llvm::StringRef f) {
    return new ASTDumper(CI);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string>& args) {
    return true;
  }
};

static FrontendPluginRegistry::Add<DumpClangAction>
X("dump-clang", "Dump clang ASTs");
