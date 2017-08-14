#include "clang/Driver/Options.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/Lookup.h"


#include <set>
#include <sstream>
#include <vector>
#include <map>
#include <tuple>

using namespace std;
using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace llvm;

/** Options **/
static llvm::cl::OptionCategory MyToolCategory("my-lib-tool options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

static std::string OutputFile("outputCpp.cpp");
static std::string InputFile;

/** Classes to be mapped to C **/
struct OutputStreams{
//	string headerString;
	string bodyString;

//	llvm::raw_string_ostream HeaderOS;
	llvm::raw_string_ostream BodyOS;

	OutputStreams() : bodyString(""), BodyOS(bodyString){};
};


//vector<string> ClassList = {"AAA", "BBB"};

//map<string, int> funcList;


class DynamicIDHandler : public clang::ExternalSemaSource {
public:
    DynamicIDHandler(clang::Sema *Sema)
    : m_Sema(Sema), m_Context(Sema->getASTContext()) {}
    ~DynamicIDHandler() = default;
    
    /// \brief Provides last resort lookup for failed unqualified lookups
    ///
    /// If there is failed lookup, tell sema to create an artificial declaration
    /// which is of dependent type. So the lookup result is marked as dependent
    /// and the diagnostics are suppressed. After that is's an interpreter's
    /// responsibility to fix all these fake declarations and lookups.
    /// It is done by the DynamicExprTransformer.
    ///
    /// @param[out] R The recovered symbol.
    /// @param[in] S The scope in which the lookup failed.
    virtual bool LookupUnqualified(clang::LookupResult &R, clang::Scope *S) {
        DeclarationName Name = R.getLookupName();
//        std::cout << Name.getAsString() << "\n";
         IdentifierInfo *II = Name.getAsIdentifierInfo();
         SourceLocation Loc = R.getNameLoc();
         bool res = Loc.isValid();
        
//         VarDecl *Result =
//              VarDecl::Create(m_Context, R.getSema().getFunctionLevelDeclContext(),
//                              Loc, Loc, II, m_Context.DependentTy,
//                              /*TypeSourceInfo*/ 0, SC_None, SC_None);

         if (1) {
//           R.addDecl(Result);
           // Say that we can handle the situation. Clang should try to recover
           return true;
         } else{
           return false;
         }
    }
    
private:
    clang::Sema *m_Sema;
    clang::ASTContext &m_Context;
};



/** Handlers **/
class classMatchHandler: public MatchFinder::MatchCallback{
public:
	classMatchHandler(OutputStreams& os): OS(os){}

	tuple<string, string, bool, bool> determineCType(const QualType& qt){

		string CType = "";
		string CastType = ""; //whether this should be casted or not
		bool 	isPointer = false;
		bool 	shoulReturn = true;

		//if it is builtint type use it as is
		if(qt->isBuiltinType() || (qt->isPointerType() && qt->getPointeeType()->isBuiltinType())){
			CType = qt.getAsString();
			if(qt->isVoidType())
				shoulReturn = false;
		//if it is a CXXrecordDecl then return a pointer to WName*
		}else if(qt->isRecordType()){
			const CXXRecordDecl* crd = qt->getAsCXXRecordDecl();
			string recordName = crd->getNameAsString();
			CType = recordName;
//			CastType = recordName+ "*";

		}else if( (qt->isReferenceType() || qt->isPointerType()) && qt->getPointeeType()->isRecordType()){
			isPointer = true; //to properly differentiate among cast types
			const CXXRecordDecl* crd = qt->getPointeeType()->getAsCXXRecordDecl();
			string recordName = crd->getNameAsString();
            if (recordName.length()) {
                if (qt->isReferenceType()) {
                    CType = recordName + "&";
                }
                else {
                    CType = recordName + "*";
                }
            } else {
                CType = qt.getAsString();
            }
		}
        if (!CType.length()) {
            CType = qt.getAsString();
        }
        //should this function return?
        if (!CType.compare("_Bool")) {
            CType = "bool";
        }
        
		return make_tuple(CType, CastType, isPointer, shoulReturn);
	}
    
    void run2(const MatchFinder::MatchResult &Result){
        if (const FunctionDecl *cmd = Result.Nodes.getNodeAs<FunctionDecl>("staticFuncDecl")){
            string returnType = "";
            string returnCast = "";
            bool shouldReturn, isPointer;
            stringstream functionBody;
            string separator = "";
            string bodyEnd;
            
            string methodName = cmd->getNameAsString();
            const QualType qt = cmd->getReturnType();
            std::tie(returnType, returnCast, isPointer, shouldReturn) = determineCType(qt);
            
            if(qt->isVoidType())
                functionBody << "return;";
            else
                functionBody << "return 0;";
            
            
            stringstream funcname;
            
            funcname << returnType << " " <<  methodName;
            funcname << "(";
            
            for(unsigned int i=0; i<cmd->getNumParams(); i++)
            {
                const QualType qt = cmd->parameters()[i]->getType();
                std::tie(returnType, returnCast, isPointer, shouldReturn) = determineCType(qt);
                
                funcname << separator << returnType << " ";
                funcname << cmd->parameters()[i]->getQualifiedNameAsString() << "";
                separator = ", ";
            }
            funcname << ")";
            
            //			OS.HeaderOS << funcname.str() << ";\n";
            
            OS.BodyOS << funcname.str() << "{\n    ";
            OS.BodyOS << functionBody.str();
            OS.BodyOS << bodyEnd << "\n}\n\n" ;
        }
    }

	virtual void run(const MatchFinder::MatchResult &Result){
        if (const FunctionDecl *cmd1 = Result.Nodes.getNodeAs<FunctionDecl>("staticFuncDecl")) {
            SourceManager &srcMgr = Result.Context->getSourceManager();
            
            // 只处理当前文件,不处被包含文件的方法处理
            string fileName = srcMgr.getFilename(cmd1->getLocation()).str();
            if (fileName.rfind(InputFile)==string::npos) {
                return;
            }

            // 先判断是否是类中的方法
            if (const CXXMethodDecl *cmd = dyn_cast<CXXMethodDecl>(cmd1)) {
                
                string methodName = "";
                string className = cmd->getParent()->getDeclName().getAsString();
                string returnType = "";
                string returnCast = "";
                bool shouldReturn, isPointer;
                string self = "";//"W" + className + "* self";
                string separator = "";
                string bodyEnd = "";
                
                std::stringstream functionBody;
                
                //ignore operator overloadings
                if(cmd->isOverloadedOperator())
                    return;
                
                if (cmd->hasInlineBody())
                    return;
                
                if(cmd->isPure())
                    return;
                
                //constructor
                if (const CXXConstructorDecl* ccd = dyn_cast<CXXConstructorDecl>(cmd)) {
                    if(ccd->isCopyConstructor() || ccd->isMoveConstructor()) return;
                    methodName = className;
                    returnType = "";
                    self = "";
                    separator = "";
                    //				bodyEnd += "))";
                }else if (isa<CXXDestructorDecl>(cmd)) {
                    methodName = "~" + className;
                    returnType = "";
                    //				functionBody << " delete reinterpret_cast<"<<className << "*>(self)";
                }else{
                    methodName = cmd->getNameAsString();
                    const QualType qt = cmd->getReturnType();
                    std::tie(returnType, returnCast, isPointer, shouldReturn) = determineCType(qt);
                    if(shouldReturn)
                        functionBody << "return 0;";
                    else
                        functionBody << "return;";
                    
                    //				bodyEnd += ")";
                }
                
                std::stringstream funcname;
                if (returnType.length()) {
                    funcname << returnType << " " << className << "::" << methodName;
                }
                else {
                    funcname << className << "::" << methodName;
                }
                
                //auto it = funcList.find(funcname.str());
                //
                //if(it != funcList.end()){
                //	it->second++;
                //	funcname << "_" << it->second ;
                //}else{
                //	funcList[funcname.str()] = 0;
                //}
                
                funcname << "(" << self;
                
                for(unsigned int i=0; i<cmd->getNumParams(); i++)
                {
                    const QualType qt = cmd->parameters()[i]->getType();
                    std::tie(returnType, returnCast, isPointer, shouldReturn) = determineCType(qt);
                    funcname << separator << returnType << " ";
                    funcname << cmd->parameters()[i]->getQualifiedNameAsString() << "";
                    
    			    // if(i !=0 )
    			    // 	functionBody << separator;
    			    // if(returnCast == "")
    			    // 	functionBody << cmd->parameters()[i]->getQualifiedNameAsString();
    			    // else{
    			    // 	if(!isPointer)
    			    // 		functionBody << "*";
    			    // 	functionBody << "reinterpret_cast<" << returnCast << ">("<< cmd->parameters()[i]->getQualifiedNameAsString() << ")";

    			    // }
                    separator = ", ";
                }
                funcname << ")";
                
                //			OS.HeaderOS << funcname.str() << ";\n";
                
                OS.BodyOS << funcname.str() << "{\n    ";
                OS.BodyOS << functionBody.str();
                OS.BodyOS << bodyEnd << "\n}\n\n" ;
            }
            else {
                run2(Result);
            }
        }
	}
	virtual void onEndOfTranslationUnit(){}
private:
	OutputStreams& OS;

};


/****************** /Member Functions *******************************/
// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser. It registers a couple of matchers and runs them on
// the AST.
class MyASTConsumer: public ASTConsumer {
public:
	MyASTConsumer(OutputStreams& os) : OS(os),
			HandlerForClassMatcher(os) {
		// Add a simple matcher for finding 'if' statements.

//		for(string& className : ClassList){
			//oss.push_back(std::move(os))

                DeclarationMatcher classMatcher = functionDecl().bind("staticFuncDecl");//cxxMethodDecl(isPublic()).bind("publicMethodDecl");
			Matcher.addMatcher(classMatcher, &HandlerForClassMatcher);
//            DeclarationMatcher functionMatcher = functionDecl().bind("staticFuncDecl");
//            Matcher.addMatcher(functionMatcher, &HandlerForFunctionMatcher);
//		}

	}

	void HandleTranslationUnit(ASTContext &Context) override {
		// Run the matchers when we have the whole TU parsed.
		Matcher.matchAST(Context);
	}

private:
	OutputStreams& OS;
	classMatchHandler HandlerForClassMatcher;
//    functionHandler HandlerForFunctionMatcher;

	MatchFinder Matcher;
};

// For each source file provided to the tool, a new FrontendAction is created.
 class MyFrontendAction: public ASTFrontendAction {
public:
	MyFrontendAction() {
    }

//     void ExecuteAction() override {
//        CompilerInstance &CI = getCompilerInstance();
//         CI.createSema(TU_Complete, nullptr);
//         Sema &sema = CI.getSema();
////         sema.Initialize();
//         DynamicIDHandler handler(&sema);
//         sema.addExternalSource(&handler);
//             
//         return ASTFrontendAction::ExecuteAction();
//     }
//
	void EndSourceFileAction() override {
		StringRef bodyFile(OutputFile.c_str());

        // Open the output file
        std::error_code EC;
//        llvm::raw_fd_ostream HOS(headerFile, EC, llvm::sys::fs::F_None);
//        if (EC) {
//            llvm::errs() << "while opening '" << headerFile<< "': "
//            << EC.message() << '\n';
//            exit(1);
//        }
        llvm::raw_fd_ostream BOS(bodyFile, EC, llvm::sys::fs::F_None);
        if (EC) {
            llvm::errs() << "while opening '" << bodyFile<< "': "
            << EC.message() << '\n';
            exit(1);
        }

//		OS.BodyOS<< "#ifdef __cplusplus\n"
//						"}\n"
//						"#endif\n";

		OS.BodyOS.flush();
		BOS<< OS.bodyString << "\n";

	}

	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
			StringRef file) override {

		return llvm::make_unique<MyASTConsumer>(OS);
	}

private:
	OutputStreams OS;
};

int main(int argc, const char **argv) {
	// parse the command-line args passed to your code
	CommonOptionsParser op(argc, argv, MyToolCategory);

	// create a new Clang Tool instance (a LibTooling environment)
	ClangTool Tool(op.getCompilations(), op.getSourcePathList());
    
    std::vector<std::string> files = op.getSourcePathList();
    InputFile = files[0];
    
	// run the Clang Tool, creating a new FrontendAction
	return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
}
