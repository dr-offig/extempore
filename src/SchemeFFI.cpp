/*
 * Copyright (c) 2011, Andrew Sorensen
 *
 * All rights reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * Neither the name of the authors nor other contributors may be used to endorse
 * or promote products derived from this software without specific prior written
 * permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
///////////////////
// LLVM includes //
///////////////////

#include <fstream>
#include <iostream>
#include <sstream>

// must be included before anything which pulls in <Windows.h>
#include "llvm/ADT/StringExtras.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm-c/Core.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Bitcode/BitcodeReader.h"

#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/OptimizationLevel.h"

#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Error.h"

#include "SchemeFFI.h"
#include "AudioDevice.h"
#include "UNIV.h"
#include "TaskScheduler.h"
#include "SchemeProcess.h"
#include "SchemeREPL.h"
#include <unordered_set>
#include <unordered_map>
#include <atomic>
#include <string_view>

#ifdef _WIN32
#include <Windows.h>
#include <Windowsx.h>
#include <filesystem>
#include <fstream>
#else
#include <dlfcn.h>
#include <dirent.h>
#endif

#ifdef DYLIB
#include <cmrc/cmrc.hpp>
CMRC_DECLARE(xtm);
#endif

// setting this define should make call_compiled thread safe BUT ...
// also extremely SLOW !

#define LLVM_EE_LOCK

#include <regex>

////////////////////////////////

#include "pcre.h"

#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <time.h>
#endif

#ifdef _WIN32
//#include <unistd.h>
#include <malloc.h>
#elif __APPLE__
#include <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>
#include <AppKit/AppKit.h>
#endif

#ifdef _WIN32
#define PRINT_ERROR(format, ...)                \
    ascii_error();                   \
    printf(format , __VA_ARGS__);                       \
    ascii_normal()
#else
#define PRINT_ERROR(format, args...)            \
    ascii_error();                   \
    printf(format , ## args);                   \
    ascii_normal()
#endif

#include <queue>
//#include <unistd.h>
#include <EXTMutex.h>
#include <EXTLLVM.h>
namespace extemp { namespace SchemeFFI {
static llvm::Module* jitCompile(const std::string& String);
}}

namespace extemp {

namespace SchemeFFI {

static std::string formatLLVMType(llvm::Type* Type)
{
    if (auto* ST = llvm::dyn_cast<llvm::StructType>(Type)) {
        if (ST->hasName()) {
            llvm::StringRef name = ST->getName();
            auto dotPos = name.rfind('.');
            if (dotPos != llvm::StringRef::npos) {
                llvm::StringRef suffix = name.substr(dotPos + 1);
                bool isNumericSuffix = !suffix.empty() &&
                    std::all_of(suffix.begin(), suffix.end(), ::isdigit);
                if (isNumericSuffix) {
                    return "%" + name.substr(0, dotPos).str();
                }
            }
            return "%" + name.str();
        }
    }
    std::string result;
    llvm::raw_string_ostream ss(result);
    Type->print(ss);
    return ss.str();
}

#include "ffi/utility.inc"
#include "ffi/ipc.inc"
#include "ffi/assoc.inc"
#include "ffi/number.inc"
#include "ffi/sys.inc"
#include "ffi/sys_dsp.inc"
#include "ffi/sys_zone.inc"
#include "ffi/misc.inc"
#include "ffi/regex.inc"
#include "ffi/llvm.inc"
#include "ffi/clock.inc"

// Regex to strip numeric suffixes from type names (e.g., %String.323 -> %String)
static std::regex sDefineSymRegex("define[^\\n]+@([-a-zA-Z$._][-a-zA-Z$._0-9]*)", std::regex::optimize | std::regex::ECMAScript);
static std::regex sDeclareSymRegex("declare[^\\n]+@([-a-zA-Z$._][-a-zA-Z$._0-9]*)", std::regex::optimize | std::regex::ECMAScript);

// Regex patterns for extraction. These use (?:^|\n) to match line starts since
// MSVC doesn't support std::regex::multiline. The leading \n is consumed but
// that's fine for extraction - we only use the capture groups.
static std::regex sExternalGlobalRegex("(?:^|\\n)@([-a-zA-Z$._][-a-zA-Z$._0-9]*)\\s*=\\s*external\\s+global\\s+(\\S+)",
                                        std::regex::optimize);

static std::regex sExternalDeclareRegex("(?:^|\\n)\\s*declare\\s+cc\\s+0\\s+([^@]+)\\s+@([^(]+)\\(([^)]*)\\)(?:\\s+nounwind)?\\s*(?:$|\\n)",
                                        std::regex::optimize);

static std::regex sGlobalSymRegex("[ \t]@([-a-zA-Z$._][-a-zA-Z$._0-9]*)", std::regex::optimize);
static std::regex sGlobalVarDefRegex("(?:^|\\n)@([-a-zA-Z$._][-a-zA-Z$._0-9]*)\\s*=", std::regex::optimize);
static std::regex sTypeDefRegex("(?:^|\\n)\\s*(%[-a-zA-Z$._0-9]+)\\s*=\\s*type\\s+([^\\n]+)", std::regex::optimize);

// Line-based regex patterns for removal operations. These match from the start
// of a line (no leading \n) and are used with line-by-line processing.
static std::regex sTypeDefLineRegex("^\\s*(%[-a-zA-Z$._0-9]+)\\s*=\\s*type\\s+(.+)$", std::regex::optimize);
static std::regex sDeclareLineRegex("^\\s*declare[^\\n]+@([-a-zA-Z$._][-a-zA-Z$._0-9]*).*$", std::regex::optimize);
void initSchemeFFI(scheme* sc)
{
    static struct {
        const char* name;
        uint32_t    value;
    } integerTable[] = {
        { "*au:block-size*", UNIV::NUM_FRAMES },
        { "*au:samplerate*", UNIV::SAMPLE_RATE },
        { "*au:channels*", UNIV::CHANNELS },
        { "*au:in-channels*", UNIV::IN_CHANNELS },
    };
    for (auto& elem: integerTable) {
        scheme_define(sc, sc->global_env, mk_symbol(sc, elem.name), mk_integer(sc, elem.value));
    }
    static struct {
        const char*  name;
        foreign_func func;
    } funcTable[] = {
        UTILITY_DEFS,
        IPC_DEFS,
        ASSOC_DEFS,
        NUMBER_DEFS,
        SYS_DEFS,
        SYS_DSP_DEFS,
        SYS_ZONE_DEFS,
        MISC_DEFS,
        REGEX_DEFS,
        LLVM_DEFS,
        CLOCK_DEFS
    };
    for (auto& elem : funcTable) {
        scheme_define(sc, sc->global_env, mk_symbol(sc, elem.name), mk_foreign_func(sc, elem.func));
    }
}

static long long llvm_emitcounter = 0;

// Track user-defined type definitions for LLVM 21's opaque pointers.
// Each new type definition needs to be included in subsequent compilations.
static std::unordered_map<std::string, std::string> sUserTypeDefs;
static std::mutex sUserTypeDefsMutex;

// Track external global variables for declaration in subsequent compilations.
// Maps global name to its type string (e.g., "SAMPLE_RATE" -> "i32").
static std::unordered_map<std::string, std::string> sExternalGlobals;
static std::mutex sExternalGlobalsMutex;

// Track external library function declarations (from bind-lib) for inclusion in subsequent compilations.
// Maps function name to its full declaration string (e.g., "sf_close" -> "declare i32 @sf_close(i8*)").
static std::unordered_map<std::string, std::string> sExternalLibFunctions;
static std::mutex sExternalLibFunctionsMutex;
static std::unordered_map<std::string, std::regex> sFuncDeclRegexCache;

// Track built-in types from the base runtime (bitcode.ll) to avoid duplicate definitions.
static std::unordered_set<std::string> sBuiltinTypes;

// Get user type definitions, filtered to exclude those already in the IR.
static std::string getUserTypeDefsStringFiltered(const std::string& existingIR) {
    std::lock_guard<std::mutex> lock(sUserTypeDefsMutex);
    std::string result;
    for (const auto& kv : sUserTypeDefs) {
        std::string typeDef = kv.first + " = type " + kv.second;
        // Check if this type is already defined.
        if (existingIR.find(typeDef) == std::string::npos) {
            result += typeDef + "\n";
        }
    }
    return result;
}

// Get external globals, filtered to exclude those already in the IR.
static std::string getExternalGlobalsStringFiltered(const std::string& existingIR) {
    std::lock_guard<std::mutex> lock(sExternalGlobalsMutex);
    std::string result;
    for (const auto& kv : sExternalGlobals) {
        std::string globalDecl = "@" + kv.first + " = external global " + kv.second;
        // Check if this global is already declared.
        if (existingIR.find(globalDecl) == std::string::npos) {
            result += globalDecl + "\n";
        }
    }
    return result;
}

static void extractAndStoreExternalGlobals(const std::string& ir) {
    std::lock_guard<std::mutex> lock(sExternalGlobalsMutex);
    std::sregex_iterator it(ir.begin(), ir.end(), sExternalGlobalRegex);
    std::sregex_iterator end;
    for (; it != end; ++it) {
        std::string globalName = (*it)[1].str();
        std::string globalType = (*it)[2].str();
        sExternalGlobals[globalName] = globalType;
    }
}

static void extractAndStoreExternalLibFunctions(const std::string& ir) {
    std::lock_guard<std::mutex> lock(sExternalLibFunctionsMutex);
    std::sregex_iterator it(ir.begin(), ir.end(), sExternalDeclareRegex);
    std::sregex_iterator end;
    for (; it != end; ++it) {
        std::string returnType = (*it)[1].str();
        std::string funcName = (*it)[2].str();
        std::string params = (*it)[3].str();

        // Reconstruct the full declaration (simplified form without calling convention).
        std::string fullDecl = "declare " + returnType + " @" + funcName + "(" + params + ") nounwind\n";
        sExternalLibFunctions[funcName] = fullDecl;
    }
}

// Get external lib functions, filtered to exclude those already declared in the IR.
static std::string getExternalLibFunctionsStringFiltered(const std::string& existingIR) {
    std::lock_guard<std::mutex> lock(sExternalLibFunctionsMutex);
    std::string result;
    constexpr std::string_view kRegexSpecials = R"(.+*?^$[](){}|\\)";
    // Check if each function is already declared.
    for (const auto& kv : sExternalLibFunctions) {
        const std::string& funcName = kv.first;
        // Escape special regex characters in function name.
        std::string escapedName;
        for (char c : funcName) {
            if (kRegexSpecials.find(c) != std::string_view::npos) {
                escapedName += '\\';
            }
            escapedName += c;
        }
        // Check if the function is already declared.
        auto cacheIt = sFuncDeclRegexCache.find(funcName);
        if (cacheIt == sFuncDeclRegexCache.end()) {
            cacheIt = sFuncDeclRegexCache.emplace(
                funcName,
                std::regex("declare\\s+(?:cc\\s+\\d+\\s+)?[^@]*@" + escapedName + "\\s*\\(", std::regex::optimize)
            ).first;
        }
        if (!std::regex_search(existingIR, cacheIt->second)) {
            result += kv.second;
        }
    }
    return result;
}

static void extractAndStoreTypeDefs(const std::string& ir) {
    std::lock_guard<std::mutex> lock(sUserTypeDefsMutex);
    std::sregex_iterator it(ir.begin(), ir.end(), sTypeDefRegex);
    std::sregex_iterator end;
    for (; it != end; ++it) {
        std::string typeName = (*it)[1].str();
        std::string typeDef = (*it)[2].str();
        // Store types not present in the base runtime.
        if (sBuiltinTypes.find(typeName) == sBuiltinTypes.end()) {
            sUserTypeDefs[typeName] = typeDef;
        }
    }
}

// Strip built-in type definitions from incoming IR to avoid duplicates when prepending sInlineString.
// Uses line-by-line processing to avoid multiline regex issues on Windows.
static std::string stripBuiltinTypeDefs(const std::string& ir) {
    if (sBuiltinTypes.empty()) {
        return ir;
    }
    std::string result;
    std::istringstream stream(ir);
    std::string line;
    bool first = true;
    while (std::getline(stream, line)) {
        std::smatch match;
        bool keepLine = true;
        if (std::regex_match(line, match, sTypeDefLineRegex)) {
            std::string typeName = match[1].str();
            if (sBuiltinTypes.find(typeName) != sBuiltinTypes.end()) {
                keepLine = false;
            }
        }
        if (keepLine) {
            if (!first) {
                result += '\n';
            }
            result += line;
            first = false;
        }
    }
    // Preserve trailing newline if original had one.
    if (!ir.empty() && ir.back() == '\n') {
        result += '\n';
    }
    return result;
}

static llvm::Module* jitCompile(const std::string& String)
{
    // Create some module to put our function into it.
    using namespace llvm;

    char modname[256];
    snprintf(modname, sizeof(modname), "xtmmodule_%lld", ++llvm_emitcounter);

    std::string asmcode(String);
    SMDiagnostic pa;

    static std::string sInlineString; // This is a hack for now, but it *WORKS*
    static std::string sInlineBitcode;
    static std::unordered_set<std::string> sInlineSyms;

#ifdef DYLIB
    auto fs = cmrc::xtm::get_filesystem();
#endif

    if (sInlineString.empty()) {
        {
#ifdef DYLIB
            auto data = fs.open("runtime/bitcode.ll");
            sInlineString = std::string(data.begin(), data.end());
#else
            std::ifstream inStream(UNIV::SHARE_DIR + "/runtime/bitcode.ll");
            std::stringstream inString;
            inString << inStream.rdbuf();
            sInlineString = inString.str();
#endif
        }
        // Collect symbol references.
        std::copy(std::sregex_token_iterator(sInlineString.begin(), sInlineString.end(), sGlobalSymRegex, 1),
                std::sregex_token_iterator(), std::inserter(sInlineSyms, sInlineSyms.begin()));

        // Collect global variable definitions.
        std::copy(std::sregex_token_iterator(sInlineString.begin(), sInlineString.end(), sGlobalVarDefRegex, 1),
                std::sregex_token_iterator(), std::inserter(sInlineSyms, sInlineSyms.begin()));

        // Extract built-in type names from base runtime to avoid duplicate definitions.
        // Uses line-by-line processing to avoid multiline regex issues on Windows.
        if (sBuiltinTypes.empty()) {
            std::istringstream stream(sInlineString);
            std::string line;
            while (std::getline(stream, line)) {
                std::smatch match;
                if (std::regex_match(line, match, sTypeDefLineRegex)) {
                    sBuiltinTypes.insert(match[1].str());
                }
            }
        }
        {
#ifdef DYLIB
            auto data = fs.open("runtime/inline.ll");
            std::string tString = std::string(data.begin(), data.end());
#else
            std::ifstream inStream(UNIV::SHARE_DIR + "/runtime/inline.ll");
            std::stringstream inString;
            inString << inStream.rdbuf();
            std::string tString = inString.str();
#endif
            std::copy(std::sregex_token_iterator(tString.begin(), tString.end(), sGlobalSymRegex, 1),
                    std::sregex_token_iterator(), std::inserter(sInlineSyms, sInlineSyms.begin()));
            std::copy(std::sregex_token_iterator(tString.begin(), tString.end(), sGlobalVarDefRegex, 1),
                    std::sregex_token_iterator(), std::inserter(sInlineSyms, sInlineSyms.begin()));
        }
    }

    // Detect if this is a bind-lib declaration.
    // These should not have external lib functions prepended, to avoid duplicates.
    bool isBindLibDeclaration = (asmcode.find("declare") == 0 && asmcode.size() < 500);

    // Build declarations string.
    std::vector<std::string> symbols;
    std::copy(std::sregex_token_iterator(asmcode.begin(), asmcode.end(), sGlobalSymRegex, 1),
            std::sregex_token_iterator(), std::inserter(symbols, symbols.begin()));
    std::sort(symbols.begin(), symbols.end());
    auto end(std::unique(symbols.begin(), symbols.end()));
    std::unordered_set<std::string> ignoreSyms;

    // Ignore symbols defined as functions.
    std::copy(std::sregex_token_iterator(asmcode.begin(), asmcode.end(), sDefineSymRegex, 1),
            std::sregex_token_iterator(), std::inserter(ignoreSyms, ignoreSyms.begin()));

    // Ignore already declared symbols.
    std::copy(std::sregex_token_iterator(asmcode.begin(), asmcode.end(), sDeclareSymRegex, 1),
            std::sregex_token_iterator(), std::inserter(ignoreSyms, ignoreSyms.begin()));

    // Ignore symbols defined/declared as global variables.
    std::copy(std::sregex_token_iterator(asmcode.begin(), asmcode.end(), sGlobalVarDefRegex, 1),
            std::sregex_token_iterator(), std::inserter(ignoreSyms, ignoreSyms.begin()));
    std::string declarations;
    llvm::raw_string_ostream dstream(declarations);
    for (auto iter = symbols.begin(); iter != end; ++iter) {
        const char* sym(iter->c_str());
        // Skip symbols in sInlineSyms or ignoreSyms.
        if (sInlineSyms.find(sym) != sInlineSyms.end() || ignoreSyms.find(sym) != ignoreSyms.end()) {
            continue;
        }
        // Skip symbols already in sExternalGlobals.
        {
            std::lock_guard<std::mutex> lock(sExternalGlobalsMutex);
            if (sExternalGlobals.find(sym) != sExternalGlobals.end()) {
                continue;
            }
        }
        // Skip symbols already in sExternalLibFunctions, unless this is a bind-lib declaration.
        if (!isBindLibDeclaration) {
            std::lock_guard<std::mutex> lock(sExternalLibFunctionsMutex);
            if (sExternalLibFunctions.find(sym) != sExternalLibFunctions.end()) {
                continue;
            }
        }
        auto gv = extemp::EXTLLVM::getGlobalValue(sym);
        if (!gv) {
            continue;
        }
        auto func(llvm::dyn_cast<llvm::Function>(gv));
        if (func) {
            dstream << "declare " << formatLLVMType(func->getReturnType()) << " @" << sym << " (";
            bool first(true);
            for (const auto& arg : func->args()) {
                if (!first) {
                    dstream << ", ";
                } else {
                    first = false;
                }
                dstream << formatLLVMType(arg.getType());
            }
            if (func->isVarArg()) {
                dstream << ", ...";
            }
            dstream << ")\n";
        } else {
            auto globalVar = llvm::dyn_cast<llvm::GlobalVariable>(gv);
            if (globalVar) {
                auto str(formatLLVMType(globalVar->getValueType()));
                dstream << '@' << sym << " = external global " << str << '\n';
            } else {
                // Fallback for other global values.
                dstream << '@' << sym << " = external global ptr\n";
        }
    }
    }

    llvm::Module* modulePtr = nullptr;

    EXTLLVM::getThreadSafeContext().withContextDo([&](LLVMContext* ctx) {
        // Initialize inline bitcode.
        if (sInlineBitcode.empty()) {
            static bool first(true);
            if (!first) {
                auto newModule(parseAssemblyString(sInlineString, pa, *ctx));
                if (newModule) {
                    llvm::raw_string_ostream bitstream(sInlineBitcode);
                    llvm::WriteBitcodeToFile(*newModule, bitstream);
                    // After first compile, replace sInlineString with inline.ll + bitcode.ll declarations.
                    // The inline.ll has the inline functions, and bitcode.ll has runtime declarations.
                    // We need both for subsequent compilations.
                    std::string inlineContent;
#ifdef DYLIB
                    auto data = fs.open("runtime/inline.ll");
                    inlineContent = std::string(data.begin(), data.end());
#else
                    std::ifstream inStream(UNIV::SHARE_DIR + "/runtime/inline.ll");
                    std::stringstream inString;
                    inString << inStream.rdbuf();
                    inlineContent = inString.str();
#endif
                    // After first compile, sInlineString should contain ONLY inline.ll.
                    // The declarations from bitcode.ll are already in sInlineBitcode (the compiled module),
                    // so we don't need to prepend them again - doing so causes "invalid redefinition" errors.
                    // Also strip built-in type definitions from inline.ll to avoid duplicates with init.ll
                    // (which is compiled at startup and already defines %mzone, %clsvar, etc.).
                    sInlineString = stripBuiltinTypeDefs(inlineContent);
                } else {
                    std::cout << pa.getMessage().str() << std::endl;
                    abort();
                }
            } else {
                first = false;
            }
        }

        std::unique_ptr<llvm::Module> newModule;

        // Get user-defined type definitions and external globals.
        std::string userTypeDefs = getUserTypeDefsStringFiltered(asmcode);
        std::string externalGlobals = getExternalGlobalsStringFiltered(asmcode);

        if (!sInlineBitcode.empty()) {
            auto modOrErr(parseBitcodeFile(llvm::MemoryBufferRef(sInlineBitcode, "<string>"), *ctx));
            if (likely(modOrErr)) {
                newModule = std::move(modOrErr.get());
                std::string externalLibFunctions = isBindLibDeclaration ? "" :
                    getExternalLibFunctionsStringFiltered(asmcode);
                asmcode = sInlineString + userTypeDefs + externalGlobals + externalLibFunctions + dstream.str() + asmcode;
                if (parseAssemblyInto(llvm::MemoryBufferRef(asmcode, "<string>"), newModule.get(), nullptr, pa)) {
                    std::cout << "**** DECL ****\n" << dstream.str() << "**** ENDDECL ****\n" << std::endl;
                    newModule.reset();
                }
            }
        } else {
            // First compilation - include sInlineString (with type definitions), user type definitions, and external lib functions.
            // Strip built-in type definitions from asmcode to avoid duplicates with sInlineString.
            std::string strippedAsmcode = stripBuiltinTypeDefs(asmcode);

            // Also strip declarations of symbols that are already in sInlineSyms.
            // Uses line-by-line processing to avoid multiline regex issues on Windows.
            {
                std::string result;
                std::istringstream stream(strippedAsmcode);
                std::string line;
                bool first = true;
                while (std::getline(stream, line)) {
                    std::smatch match;
                    bool keepLine = true;
                    if (std::regex_match(line, match, sDeclareLineRegex)) {
                        std::string symName = match[1].str();
                        if (sInlineSyms.find(symName) != sInlineSyms.end()) {
                            keepLine = false;
                        }
                    }
                    if (keepLine) {
                        if (!first) {
                            result += '\n';
                        }
                        result += line;
                        first = false;
                    }
                }
                // Preserve trailing newline if original had one.
                if (!strippedAsmcode.empty() && strippedAsmcode.back() == '\n') {
                    result += '\n';
                }
                strippedAsmcode = result;
            }

            std::string externalLibFunctions = getExternalLibFunctionsStringFiltered(strippedAsmcode);
            strippedAsmcode = sInlineString + userTypeDefs + externalGlobals + externalLibFunctions + dstream.str() + strippedAsmcode;
            newModule = parseAssemblyString(strippedAsmcode, pa, *ctx);
        }

        if (!newModule) {
            std::string errstr;
            llvm::raw_string_ostream ss(errstr);
            pa.print("LLVM IR", ss);
            printf("%s\n", ss.str().c_str());
            return; // modulePtr remains nullptr
        }

        // Extract and store any new type definitions.
        extractAndStoreTypeDefs(String);

        // Extract and store external global declarations.
        extractAndStoreExternalGlobals(String);

        // Extract and store external library function declarations.
        extractAndStoreExternalLibFunctions(String);

        // Set target triple.
        if (unlikely(!extemp::UNIV::ARCH.empty())) {
            newModule->setTargetTriple(llvm::Triple(extemp::UNIV::ARCH));
        }

        // Optimize
        if (EXTLLVM::OPTIMIZE_COMPILES) {
            llvm::LoopAnalysisManager LAM;
            llvm::FunctionAnalysisManager FAM;
            llvm::CGSCCAnalysisManager CGAM;
            llvm::ModuleAnalysisManager MAM;

            llvm::PassBuilder PB;
            PB.registerModuleAnalyses(MAM);
            PB.registerCGSCCAnalyses(CGAM);
            PB.registerFunctionAnalyses(FAM);
            PB.registerLoopAnalyses(LAM);
            PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

            // Use configurable optimization level.
            llvm::OptimizationLevel optLevel;
            switch (EXTLLVM::OPTIMIZATION_LEVEL) {
                case 0: optLevel = llvm::OptimizationLevel::O0; break;
                case 1: optLevel = llvm::OptimizationLevel::O1; break;
                case 3: optLevel = llvm::OptimizationLevel::O3; break;
                case 2:
                default: optLevel = llvm::OptimizationLevel::O2; break;
            }
            llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(optLevel);
            MPM.run(*newModule, MAM);
        }

        // Verify the module.
        if (extemp::EXTLLVM::VERIFY_COMPILES && verifyModule(*newModule)) {
        std::cout << "\nInvalid LLVM IR\n";
            return;
        }

        modulePtr = newModule.get();

        // Extract symbol names from the module.
        std::vector<std::string> symbolNames;
        for (const auto& func : newModule->getFunctionList()) {
            if (!func.isDeclaration()) {
                symbolNames.push_back(func.getName().str());
            }
        }
        for (const auto& glob : newModule->globals()) {
            if (!glob.isDeclaration()) {
                symbolNames.push_back(glob.getName().str());
            }
        }

        // Clone the module for metadata.
        auto metadataModule = llvm::CloneModule(*newModule);

        // Add module to ORC JIT with symbol tracking.
        auto TSM = llvm::orc::ThreadSafeModule(std::move(newModule), EXTLLVM::getThreadSafeContext());
        auto err = EXTLLVM::addTrackedModule(std::move(TSM), symbolNames);

        // Register cloned module metadata.
        if (err) {
            std::cerr << "Failed to add module to JIT: "
                      << llvm::toString(std::move(err)) << std::endl;
            modulePtr = nullptr;
        } else {
            modulePtr = metadataModule.get();
            EXTLLVM::addModule(metadataModule.get());
            // Transfer ownership to Ms vector.
            metadataModule.release();
        }
    });

    return modulePtr;
}

}

} // end namespace

