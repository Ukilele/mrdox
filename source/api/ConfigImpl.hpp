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

#ifndef MRDOX_API_CONFIGIMPL_HPP
#define MRDOX_API_CONFIGIMPL_HPP

#include "Support/YamlFwd.hpp"
#include <mrdox/Config.hpp>
#include <llvm/Support/ThreadPool.h>
#include <memory>

namespace clang {
namespace mrdox {

class ConfigImpl
    : public Config
    , public std::enable_shared_from_this<ConfigImpl>

{
public:
    //--------------------------------------------
    //
    // YAML
    //
    //--------------------------------------------

    struct FileFilter
    {
        std::vector<std::string> include;
    };

    int concurrency_ = 0;
    std::vector<std::string> additionalDefines_;
    std::string sourceRoot_;
    bool includePrivate_ = false;

    FileFilter input_;

    //--------------------------------------------

private:
    llvm::ThreadPool mutable threadPool_;
    std::string configYamlStr_;
    std::string extraYamlStr_;
    llvm::SmallString<0> workingDir_;
    llvm::SmallString<0> outputPath_;
    std::vector<std::string> inputFileIncludes_;

    friend class Config;
    friend class Options;
    friend class WorkGroup;

    template<class T>
    friend struct llvm::yaml::MappingTraits;

    llvm::Error construct(
        llvm::StringRef workingDir,
        llvm::StringRef configYaml,
        llvm::StringRef extraYaml);

    llvm::SmallString<0>
    normalizedPath(
        llvm::StringRef pathName);

public:
    ConfigImpl();

    /** Return true if the thread pool should be used for work.
    */
    bool useThreadPool() const noexcept
    {
        return concurrency_ != 1;
    }

    /** Return the full path to the working directory.

        The returned path will always be POSIX
        style and have a trailing separator.
    */
    llvm::StringRef
    workingDir() const noexcept
    {
        return workingDir_;
    }

    /** Return the full path to the source root directory.

        The returned path will always be POSIX
        style and have a trailing separator.
    */
    std::string_view
    sourceRoot() const noexcept
    {
        return sourceRoot_;
    }

    //--------------------------------------------
    //
    // Private Interface
    //
    //--------------------------------------------

    /** Returns true if the translation unit should be visited.

        @param filePath The posix-style full path
        to the file being processed.
    */
    bool
    shouldVisitTU(
        llvm::StringRef filePath) const noexcept;

    /** Returns true if the file should be visited.

        If the file is visited, then prefix is
        set to the portion of the file path which
        should be be removed for matching files.

        @param filePath The posix-style full path
        to the file being processed.

        @param prefix The prefix which should be
        removed from subsequent matches.
    */
    bool
    shouldVisitFile(
        llvm::StringRef filePath,
        llvm::SmallVectorImpl<char>& prefix) const noexcept;

    /** A diagnostic handler for reading YAML files.
    */
    static void yamlDiagnostic(llvm::SMDiagnostic const&, void*);

    /** Return a configuration created from YAML.

        @param workingDir The directory which
        should be considered the working directory
        for calculating filenames from relative
        paths.

        @param configYaml A string containing valid
        YAML which will be parsed and applied to create
        the configuration.

        @param extraYaml An optional string containing
        additional valid YAML which will be parsed and
        applied to the existing configuration.
    */
    friend auto
    createConfigFromYAML(
        llvm::StringRef workingDir,
        llvm::StringRef configYaml,
        llvm::StringRef extraYaml) ->
            llvm::ErrorOr<std::shared_ptr<
                ConfigImpl const>>;
};

//------------------------------------------------

/** Return a configuration by loading one or both YAML strings.

    This function attempts to parse the given
    YAML string and apply the results to create
    a configuration. The working directory of
    the config object will be set to the specified
    full path. If the specified path is empty,
    then the current working directory of the
    process will be used instead.

    If the `extraYaml` string is not empty, then
    after the YAML file is applied the string
    will be parsed as YAML and the results will
    be applied to the configuration. And keys
    and values in the extra YAML which are the
    same as elements from the file will replace
    existing settings.

    @return The configuration upon success,
        otherwise an error.

    @param workingDir The directory which
    should be considered the working directory
    for calculating filenames from relative
    paths.

    @param configYaml A string containing valid
    YAML which will be parsed and applied to create
    the configuration.

    @param extraYaml An optional string containing
    additional valid YAML which will be parsed and
    applied to the existing configuration.
*/
auto
createConfigFromYAML(
    llvm::StringRef workingDir,
    llvm::StringRef configYaml,
    llvm::StringRef extraYaml) ->
        llvm::ErrorOr<std::shared_ptr<
            ConfigImpl const>>;


/** Create a configuration by loading a YAML file.

    This function attemtps to load the given
    YAML file and apply the results to create
    a configuration. The working directory of
    the config object will be set to the
    directory containing the file.

    If the `extraYaml` string is not empty, then
    after the YAML file is applied the string
    will be parsed as YAML and the results will
    be applied to the configuration. And keys
    and values in the extra YAML which are the
    same as elements from the file will replace
    existing settings.

    @return A valid object upon success.

    @param fileName A full or relative path to
    the configuration file, which may have any
    extension including no extension.

    @param extraYaml An optional string containing
    additional valid YAML which will be parsed and
    applied to the existing configuration.

    @param ec [out] Set to the error, if any occurred.
*/
MRDOX_DECL
std::shared_ptr<ConfigImpl const>
loadConfigFile(
    std::string_view fileName,
    std::string_view extraYaml,
    std::error_code& ec);

/** Return a configuration by loading a YAML file.

    This function attemtps to load the given
    YAML file and apply the settings to create
    a configuration. The working directory of
    the config object will be set to the
    directory containing the file.

    @return A valid object upon success.

    @param fileName A full or relative path to
    the configuration file, which may have any
    extension including no extension.

    @param ec [out] Set to the error, if any occurred.
*/
inline
std::shared_ptr<ConfigImpl const>
loadConfigFile(
    std::string_view fileName,
    std::error_code& ec)
{
    return loadConfigFile(fileName, "", ec);
}

/** Create a configuration by loading a YAML string.

    This function attempts to parse the given
    YAML string and apply the results to create
    a configuration. The working directory of
    the config object will be set to the specified
    full path. If the specified path is empty,
    then the current working directory of the
    process will be used instead.

    @return A valid object upon success.

    @param workingDir The directory which
    should be considered the working directory
    for calculating filenames from relative
    paths.

    @param configYaml A string containing valid
    YAML which will be parsed and applied to create
    the configuration.

    @param ec [out] Set to the error, if any occurred.
*/
MRDOX_DECL
std::shared_ptr<ConfigImpl const>
loadConfigString(
    std::string_view workingDir,
    std::string_view configYaml,
    std::error_code& ec);

} // mrdox
} // clang

#endif
