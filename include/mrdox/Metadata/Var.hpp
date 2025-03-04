//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_METADATA_VARIABLE_HPP
#define MRDOX_METADATA_VARIABLE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Bits.hpp>
#include <mrdox/Metadata/FieldType.hpp>
#include <mrdox/Metadata/Symbol.hpp>
#include <mrdox/Metadata/Type.hpp>
#include <clang/Basic/Specifiers.h>

namespace clang {
namespace mrdox {

enum class VarFlags0 : std::uint32_t
{
    storageClass        = 0b00000000'00000000'00000000'00000111
};

/** A variable.

    This includes variables at namespace
    scope, and static variables at class scope.
*/
struct VarInfo
    : SymbolInfo
    , TypeInfo // holds the type of this variable
{
    Bits<VarFlags0> specs;

    //--------------------------------------------

    static constexpr InfoType type_id = InfoType::IT_variable;

    explicit
    VarInfo(
        SymbolID ID = SymbolID(),
        llvm::StringRef Name = llvm::StringRef())
        : SymbolInfo(InfoType::IT_variable, ID, Name)
    {
    }
};

} // mrdox
} // clang

#endif
