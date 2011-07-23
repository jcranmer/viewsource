#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

#include <iostream>
#include <map>
#include <set>
#include <stdio.h>
#include <stdlib.h>

const char *guards[][2] = {
  { "FunctionDecl::isInlineDefinitionExternallyVisible",
    "d->isInlined()" },
  { "FunctionDecl::isReservedGlobalPlacementOperator",
    "d->getDeclName().getNameKind() == DeclarationName::CXXOperatorName" },
  { "NamedDecl::isCXXInstanceMember", "d->isCXXClassMember()" },
  { "VarDecl::getInstantiatedFromStaticDataMember", "d->isStaticDataMember()" },
  { "VarDecl::getTemplateSpecializationKind", "d->isStaticDataMember()" },
  { "VarDecl::isInitICE", "d->isInitKnownICE()" }
};

using namespace clang;
using namespace std;

class ASTDumper;

class ASTDumper : public ASTConsumer,
    public RecursiveASTVisitor<ASTDumper> {
private:
  CXXRecordDecl *declClass,
                *declContextClass,
                *typeLocClass;
public:
  ASTDumper(CompilerInstance &ci) {}

  // All we need is to follow the final declaration.
  virtual void HandleTranslationUnit(ASTContext &ctx) {
    TraverseDecl(ctx.getTranslationUnitDecl());
    cout << "#if 0\n";
    emitDeclVisitor(declClass);
    cout << "#endif\n";
  }

  virtual bool VisitCXXRecordDecl(CXXRecordDecl *d) {
    std::string className = d->getNameAsString();
    if (className == "Decl")
      declClass = d;
    else if (className == "DeclContext")
      declContextClass = d;
    else if (className == "TypeLoc")
      typeLocClass = d;
    if (className != "RecursiveASTVisitor")
      return true;
    CXXRecordDecl::method_iterator it;
    for (it = d->method_begin(); it != d->method_end(); it++) {
      std::string value = it->getNameAsString();
      if (value.substr(0, 5) != "Visit")
        continue;
      // What type are we visiting?
      QualType type = it->getParamDecl(0)->getType();
      if (PointerType::classof(type.getTypePtr())) {
        const CXXRecordDecl *decl = type->getCXXRecordDeclForPointerType();
        if (decl->isDerivedFrom(declClass))
          emitDeclVisitor(decl);
      } else {
        const CXXRecordDecl *decl = type->getAsCXXRecordDecl();
        if (decl && decl->isDerivedFrom(typeLocClass))
          emitTypeLocVisitor(decl);
      }
    }
    return false;
  }

  void printGuard(std::string type, std::string method) {
    std::string verify = type + "::" + method;
    for (size_t i = 0; i <sizeof(guards) / sizeof(*guards); i++) {
      if (verify == guards[i][0]) {
        cout << "  if (" << guards[i][1] << ")\n  ";
        return;
      }
    }
  }

  void emitDeclVisitor(const CXXRecordDecl *decl) {
    std::string type = decl->getNameAsString();
    cout << "virtual bool Visit" << type << "(" << type << " *d) {\n";
    cout << "  object->m_realName = \"" << type << "\";\n";
    if (decl->isDerivedFrom(declContextClass))
      cout << "  VisitDeclContext(d);\n";
    emitInterestingValues(decl, "d->");
    cout << "  return true;\n}\n" << endl;
  }

  void emitTypeLocVisitor(const CXXRecordDecl *decl) {
    std::string type = decl->getNameAsString();
    cout << "virtual bool Visit" << type << "(" << type << " l) {\n";
    cout << "  object->m_realName = \"" << type << "\";\n";
    emitInterestingValues(decl, "l.");
    cout << "  return true;\n}\n" << endl;
  }

  void emitInterestingValues(const CXXRecordDecl *decl, const char *invoke) {
    std::string type = decl->getNameAsString();
    CXXRecordDecl::method_iterator it;
    std::string last;
    for (it = decl->method_begin(); it != decl->method_end(); it++) {
      if (!it->isInstance() || it->getDeclKind() != Decl::Kind::CXXMethod ||
          it->getNumParams() != 0 ||
          it->getAccess() != AccessSpecifier::AS_public)
        continue;
      std::string name = it->getNameAsString();
      if (name == last) continue;
      last = name;
      std::string valueName, thunk;
      if (name.substr(0, 2) == "is")
        valueName = name.substr(2);
      else if (name.substr(0, 3) == "has")
        valueName = name;
      else if (name.substr(0, 3) == "get") {
        // How do we need to thunk?
        const Type *res = it->getResultType().getTypePtr()
          ->getUnqualifiedDesugaredType();
        const CXXRecordDecl *real = res->getCXXRecordDeclForPointerType();
        if (res->isIntegralOrEnumerationType()) {
          valueName = name.substr(3);
        } else if (real) {
          std::string retName = real->getNameAsString();
          if (real->isDerivedFrom(declClass))
            thunk = "thunk";
          else if (real == declContextClass)
            thunk = "thunk2";
          else if (retName == "TypeSourceInfo")
            thunk = "thunk";
        } else if ((real = res->getAsCXXRecordDecl())) {
          std::string retName = real->getNameAsString();
          if (retName == "SourceLocation" || retName == "SourceRange")
            thunk = " ";
          else if (retName == "basic_string") {
            // Remove AsString
            valueName = name.substr(3, name.length() - 11);
          }
        }

        if (!thunk.empty())
          valueName = name.substr(3);
      }
      if (!valueName.empty()) {
        printGuard(type, name);
        cout << "  object->m_values[\"" << valueName << "\"] = ";
        cout << thunk << "(" << invoke << name << "());\n";
      } else if (name.size() > 6 && name.substr(name.size() - 6) == "_begin") {
        // Gahh... special case this method
        if (type == "ObjCInterfaceDecl" && name == "all_declared_ivar_begin")
          continue;
        std::string base = name.substr(0, name.size() - 6);
        printGuard(type, name);
        cout << "  object->m_values[\"" << base << "s\"] = ";
        cout << "makeList(" << invoke << base << "_begin(), " << invoke <<
          base << "_end());\n";
      }
    }
  }
};

std::string indent;
void pushIndent() {indent += "  ";}
void popIndent() {indent.erase(indent.length() - 2);}
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
X("gen-file", "Dump clang ASTs");
