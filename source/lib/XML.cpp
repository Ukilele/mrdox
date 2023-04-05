//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Generators.h"
#include "CorpusVisitor.hpp"
#include "Representation.h"
#include "base64.hpp"
#include <mrdox/Config.hpp>
#include <clang/Tooling/Execution.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/YAMLParser.h>

//------------------------------------------------

namespace clang {
namespace mrdox {

namespace {

//------------------------------------------------

struct escape
{
    explicit
    escape(
        llvm::StringRef const& s) noexcept
        : s_(s)
    {
    }

    friend
    llvm::raw_ostream&
    operator<<(
        llvm::raw_ostream& os,
        escape const& t);

private:
    llvm::StringRef s_;
};

llvm::raw_ostream&
operator<<(
    llvm::raw_ostream& os,
    escape const& t)
{
    std::size_t pos = 0;
    auto const size = t.s_.size();
    while(pos < size)
    {
    unescaped:
        auto const found = t.s_.find_first_of("<>&'\"", pos);
        if(found == llvm::StringRef::npos)
        {
            os.write(t.s_.data() + pos, t.s_.size() - pos);
            break;
        }
        os.write(t.s_.data() + pos, found - pos);
        pos = found;
        while(pos < size)
        {
            auto const c = t.s_[pos];
            switch(c)
            {
            case '<':
                os.write("&lt;", 4);
                break;
            case '>':
                os.write("&gt;", 4);
                break;
            case '&':
                os.write("&amp;", 5);
                break;
            case '\'':
                os.write("&apos;", 6);
                break;
            case '\"':
                os.write("&quot;", 6);
                break;
            default:
                goto unescaped;
            }
            ++pos;
        }
    }
    return os;
}

//------------------------------------------------

namespace fs = llvm::sys::fs;
namespace path = llvm::sys::path;

class XMLGenerator
    : public clang::mrdox::Generator
{
    using Attrs =
        std::initializer_list<
            std::pair<
                llvm::StringRef,
                llvm::StringRef>>;

    //--------------------------------------------
    
    void writeNamespaces(std::vector<Reference> const& v);
    void writeRecords(std::vector<Reference> const& v);
    void write(FunctionList const& fnList);
    void write(FunctionOverloads const& fns);
    void write(std::vector<EnumInfo> const& v);
    void write(std::vector<TypedefInfo> const& v);

    //--------------------------------------------

    void write(NamespaceInfo const& I);
    void write(RecordInfo const& I);
    void write(FunctionInfo const& I);
    void write(EnumInfo const& I);
    void write(TypedefInfo const& I);

    //--------------------------------------------

    void write(llvm::ArrayRef<FieldTypeInfo> const& v);

    //--------------------------------------------

    void write(FieldTypeInfo const& I);
    void write(Reference const& ref);

    //--------------------------------------------

    void writeInfo(Info const& I);
    void writeSymbolInfo(SymbolInfo const& I);
    void writeList(llvm::SmallVector<Reference, 4> const& v);
    void writeLoc(llvm::ArrayRef<Location> const& loc);
    void writeLoc(std::optional<Location> const& loc);

    //--------------------------------------------

    void openTag(llvm::StringRef);
    void openTag(llvm::StringRef, Attrs);
    void closeTag(llvm::StringRef);
    void writeTag(llvm::StringRef);
    void writeTag(llvm::StringRef, Attrs);
    void writeTagLine(llvm::StringRef tag, llvm::StringRef value);
    void writeTagLine(llvm::StringRef tag, llvm::StringRef value, Attrs);

    //--------------------------------------------

    std::string toString(SymbolID const& id);
    llvm::StringRef toString(AccessSpecifier access);

    //--------------------------------------------

    NamespaceInfo const*
    findGlobalNamespace();

    //--------------------------------------------

    static llvm::StringRef toString(InfoType) noexcept;

    //--------------------------------------------

    Config const& cfg_;
    std::string level_;
    llvm::raw_ostream* os_ = nullptr;
    InfoMap const* infos_ = nullptr;

public:
    static char const* Format;

    XMLGenerator()
        : cfg_([]() -> Config&
        {
            static Config c;
            return c;
        }())
    {
    }

    explicit
    XMLGenerator(
        Config const& cfg) noexcept
        : cfg_(cfg)
    {
    }

    bool
    render(
        std::string& xml,
        Corpus const& corpus,
        Config const& cfg);

    llvm::Error
    generateDocs(
        llvm::StringRef RootDir,
        Corpus const& corpus,
        Config const& cfg) override;

    llvm::Error
    generateDocForInfo(
        clang::mrdox::Info* I,
        llvm::raw_ostream& os,
        clang::mrdox::Config const& cfg) override;

};

//------------------------------------------------

void
XMLGenerator::
writeNamespaces(
    std::vector<Reference> const& v)
{
    for(auto const& ref : v)
    {
        assert(ref.RefType == InfoType::IT_namespace);
        auto it = infos_->find(
            llvm::toHex(llvm::toStringRef(ref.USR)));
        assert(it != infos_->end());
        write(*static_cast<NamespaceInfo const*>(
            it->second.get()));
    }
}

void
XMLGenerator::
writeRecords(
    std::vector<Reference> const& v)
{
    for(auto const& ref : v)
    {
        assert(ref.RefType == InfoType::IT_record);
        auto it = infos_->find(
            llvm::toHex(llvm::toStringRef(ref.USR)));
        assert(it != infos_->end());
        write(
            *static_cast<RecordInfo const*>(
                it->second.get()));
    }
}

void
XMLGenerator::
write(
    FunctionList const& fnList)
{
    for(auto const& fns : fnList)
        write(fns);
}

void
XMLGenerator::
write(
    FunctionOverloads const& fns)
{
    for(auto const& fn : fns)
        write(fn);
}

void
XMLGenerator::
write(
    std::vector<EnumInfo> const& v)
{
    for(auto const& I : v)
        write(I);
}

void
XMLGenerator::
write(
    std::vector<TypedefInfo> const& v)
{
    for(auto const& I : v)
        writeInfo(I);
}

//------------------------------------------------

void
XMLGenerator::
write(
    NamespaceInfo const& I)
{
    openTag("scope", {
        { "name", I.Name },
        { "tag", toString(I.IT) },
        { "usr", toBase64(I.USR) }
        });
    writeInfo(I);
    writeNamespaces(I.Children.Namespaces);
    writeRecords(I.Children.Records);
    write(I.Children.functions);
    write(I.Children.Enums);
    write(I.Children.Typedefs);
    closeTag("scope");
}

void
XMLGenerator::
write(
    RecordInfo const& I)
{
    openTag( "scope", {
        { "name", I.Name },
        { "tag", getTagType(I.TagType) },
        { "usr", toBase64(I.USR) }
        });
    writeSymbolInfo(I);
    //writeTagLine("fullname", I.FullName);
    //writeTagLine("typedef", std::to_string(I.IsTypeDef));
    //write(I.Members);
    writeRecords(I.Children.Records);
    write(I.Children.functions);
    write(I.Children.Enums);
    write(I.Children.Typedefs);
    closeTag("scope");
}

void
XMLGenerator::
write(
    FunctionInfo const& I)
{
    openTag("function", {
        { "name", I.Name },
        { "access", toString(I.Access) },
        { "usr", toBase64(I.USR) }
        });
    writeSymbolInfo(I);
    writeTagLine("return",
        //I.ReturnType.Type.Name, {
        I.ReturnType.Type.QualName, {
            { "usr", toString(I.ReturnType.Type.USR) }
        });
    write(I.Params);
#if 0
    //writeRef(I.ReturnType.Type);
    if(I.Template)
    {
        for(TemplateParamInfo const& tp : I.Template->Params)
            writeTag(
                "tp", {
                { "n", tp.Contents }
                });
    }
    writeLoc(I.Loc);
#endif
    closeTag("function");
}

void
XMLGenerator::
write(
    EnumInfo const& I)
{
    openTag("enum", {
        { "name", I.Name },
        { "usr", toBase64(I.USR) },
        });
    writeInfo(I);
    for(auto const& v : I.Members)
    {
        writeTag("element", {
            { "name", v.Name },
            { "value", v.Value },
            });
    }
    writeLoc(I.Loc);
    closeTag("enum");
}

void
XMLGenerator::
write(
    TypedefInfo const& I)
{
    openTag("typedef", {
        { "name", I.Name },
        { "usr", toBase64(I.USR) }
        //{ "usr", toBase64(I.Underlying.Type.USR) }
        });
    writeSymbolInfo(I);
    writeTagLine("qualname", I.Underlying.Type.QualName);
    writeTagLine("qualusr", toBase64(I.Underlying.Type.USR));
    writeLoc(I.DefLoc);
    closeTag("typedef");
}

//------------------------------------------------

void
XMLGenerator::
write(
    llvm::ArrayRef<FieldTypeInfo> const& v)
{
    for(auto const& I : v)
        write(I);
}

//------------------------------------------------

void
XMLGenerator::
write(FieldTypeInfo const& I)
{
    writeTag("param", {
        { "name", I.Name },
        { "default", I.DefaultValue },
        { "type", I.Type.Name },
        { "qualname", I.Type.QualName },
        { "reftype", toString(I.Type.RefType) },
        { "usr", toString(I.Type.USR) }
        });
}

void
XMLGenerator::
write(
    Reference const& I)
{
    //openTag("ref", {
        //{ "usr", toString(ref.USR) }
        //});
    //writeTagLine("relpath", ref.getRelativeFilePath());
    writeTagLine("basename",
        I.getFileBaseName());
    writeTagLine("name", I.Name);
    writeTagLine("qual", I.QualName);
    writeTagLine("tag", std::to_string(static_cast<int>(I.RefType)));
    writeTagLine("path", I.Path);
    //closeTag("ref");
}

//------------------------------------------------

void
XMLGenerator::
writeInfo(
    Info const& I)
{
}

void
XMLGenerator::
writeSymbolInfo(
    SymbolInfo const& I)
{
    writeInfo(I);
    // I.DefLoc
    // I.Loc[]
}

void
XMLGenerator::
writeList(
    llvm::SmallVector<Reference, 4> const& v)
{
    for(auto const& ns : v)
        writeTagLine("ns", ns.QualName);
}

void
XMLGenerator::
writeLoc(
    llvm::ArrayRef<Location> const& loc)
{
    return;
    if(loc.size() > 0)
        *os_ << level_ <<
            "<file>" << escape(loc[0].Filename) <<
            "</file><line>" << std::to_string(loc[0].LineNumber) <<
            "</line>\n";
}

void
XMLGenerator::
writeLoc(
    std::optional<Location> const& opt)
{
    return;
    if(! opt)
        return;
    Location const& loc(*opt);
    writeLoc(llvm::ArrayRef<Location>(&loc, &loc+1));
}

//------------------------------------------------

void
XMLGenerator::
openTag(
    llvm::StringRef tag)
{
    *os_ << level_ << '<' << tag << ">\n";
    level_.push_back(' ');
}

void
XMLGenerator::
openTag(
    llvm::StringRef tag,
    Attrs init)
{
    *os_ << level_ << '<' << tag;
    for(auto const& attr : init)
        *os_ <<
            ' ' << attr.first << '=' <<
            "\"" << escape(attr.second) << "\"";
    *os_ << ">\n";
    level_.push_back(' ');
}

void
XMLGenerator::
closeTag(
    llvm::StringRef tag)
{
    level_.pop_back();
    *os_ << level_ << "</" << tag << ">\n";
}

void
XMLGenerator::
writeTag(
    llvm::StringRef tag)
{
    *os_ << level_ << "<" << tag << "/>\n";
}

void
XMLGenerator::
writeTag(
    llvm::StringRef tag,
    Attrs init)
{
    *os_ << level_ << "<" << tag;
    for(auto const& attr : init)
        *os_ <<
            ' ' << attr.first << '=' <<
            "\"" << escape(attr.second) << "\"";
    *os_ << "/>\n";
}

void
XMLGenerator::
writeTagLine(
    llvm::StringRef tag,
    llvm::StringRef value)
{
    *os_ << level_ <<
        "<" << tag << ">" <<
        escape(value) <<
        "</" << tag << ">" <<
        "\n";
}

void
XMLGenerator::
writeTagLine(
    llvm::StringRef tag,
    llvm::StringRef value,
    Attrs init)
{
    *os_ << level_ <<
        "<" << tag;
    for(auto const& attr : init)
        *os_ <<
            ' ' << attr.first << '=' <<
            "\"" << escape(attr.second) << "\"";
    *os_ << ">" << escape(value) << "</" << tag << ">" << "\n";
}

//------------------------------------------------

std::string
XMLGenerator::
toString(
    SymbolID const& id)
{
    return toBase64(id);
}

llvm::StringRef
XMLGenerator::
toString(
    AccessSpecifier access)
{
    switch(access)
    {
    case AccessSpecifier::AS_public:
        return "public";
    case AccessSpecifier::AS_protected:
        return "protected";
    case AccessSpecifier::AS_private:
        return "private";
    case AccessSpecifier::AS_none:
    default:
        return "none";
    }
}

//------------------------------------------------

NamespaceInfo const*
XMLGenerator::
findGlobalNamespace()
{
    for(auto const& g : *infos_)
    {
        auto const& inf = *g.getValue().get();
        if( inf.Name == "" &&
            inf.IT == InfoType::IT_namespace)
            return static_cast<
                NamespaceInfo const*>(&inf);
    }

    return nullptr;
}

llvm::StringRef
XMLGenerator::
toString(
    InfoType it) noexcept
{
    switch(it)
    {
    case InfoType::IT_default:    return "default";
    case InfoType::IT_namespace:  return "namespace";
    case InfoType::IT_record:     return "record";
    case InfoType::IT_function:   return "function";
    case InfoType::IT_enum:       return "enum";
    case InfoType::IT_typedef:    return "typedef";
    default:
        llvm_unreachable("unknown InfoType");
    }
}

//------------------------------------------------

//------------------------------------------------

bool
XMLGenerator::
render(
    std::string& xml,
    Corpus const& corpus,
    Config const& cfg)
{
    xml.clear();
    infos_ = &corpus.USRToInfo;
    auto const ns = findGlobalNamespace();
    assert(ns != nullptr);
    llvm::raw_string_ostream os(xml);
    os_ = &os;
    level_ = {};
#if 0
    *os_ <<
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" <<
        "<!DOCTYPE mrdox SYSTEM \"mrdox.dtd\">\n" <<
        "<mrdox>\n";
#endif
    write(*ns);
#if 0
    *os_ <<
        "</mrdox>\n";
#endif
    return true;
}

llvm::Error
XMLGenerator::
generateDocs(
    llvm::StringRef RootDir,
    Corpus const& corpus,
    Config const& cfg)
{
    llvm::SmallString<256> filename(cfg.OutDirectory);
    if(! fs::is_directory(filename))
        return llvm::createStringError(
            llvm::inconvertibleErrorCode(),
            "OutDirectory is not a directory");
    path::append(filename, "index.xml");
    if( fs::exists(filename) &&
        ! fs::is_regular_file(filename))
        return llvm::createStringError(
            llvm::inconvertibleErrorCode(),
            "Output file is not regular");
    infos_ = &corpus.USRToInfo;
    auto const ns = findGlobalNamespace();
    if(! ns)
        return llvm::createStringError(
            llvm::inconvertibleErrorCode(),
            "not found: (global namespace)");
    std::error_code ec;
    llvm::raw_fd_ostream os(filename, ec);
    if(ec)
        return llvm::createStringError(
            ec,
            "output file could not be opened");
    level_ = {};
    os_ = &os;
#if 0
    *os_ <<
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" <<
        "<!DOCTYPE mrdox SYSTEM \"mrdox.dtd\">\n" <<
        "<mrdox>\n";
#endif
    write(*ns);
#if 0
    *os_ <<
        "</mrdox>\n";
#endif
    // VFALCO the error handling needs to be
    //        propagated through all write functions
    if(os.error())
        return llvm::createStringError(
            ec,
            "output stream failure");
    return llvm::Error::success();
}

llvm::Error
XMLGenerator::
generateDocForInfo(
    clang::mrdox::Info* I,
    llvm::raw_ostream& os,
    clang::mrdox::Config const& cfg)
{
    return llvm::Error::success();
}

//------------------------------------------------

char const*
XMLGenerator::
Format = "xml";

} // (anon)

//------------------------------------------------

void
renderToXMLString(
    std::string& xml,
    Corpus const& corpus,
    Config const& cfg)
{
    XMLGenerator G(cfg);
    G.render(xml, corpus, cfg);
    //return llvm::Error::success();
}

//------------------------------------------------

static
GeneratorRegistry::
Add<XMLGenerator>
xmlGenerator(
    XMLGenerator::Format,
    "Generator for XML output.");

// This anchor is used to force the linker
// to link in the generated object file and
// thus register the generator.
volatile int XMLGeneratorAnchorSource = 0;

using namespace llvm;
} // mrdox
} // clang

void
force_xml_generator_linkage()
{
    // VFALCO This whole business of disappearing
    //        TUs needs to be refactored.
    clang::mrdox::XMLGeneratorAnchorSource++;
}
