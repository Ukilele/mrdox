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

#ifndef MRDOX_METADATA_TYPEDEF_HPP
#define MRDOX_METADATA_TYPEDEF_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Symbol.hpp>
#include <mrdox/Metadata/Type.hpp>

namespace clang {
namespace mrdox {

// Info for typedef and using statements.
struct TypedefInfo : SymbolInfo
{
    TypeInfo Underlying;

    // Indicates if this is a new C++ "using"-style typedef:
    //   using MyVector = std::vector<int>
    // False means it's a C-style typedef:
    //   typedef std::vector<int> MyVector;
    bool IsUsing = false;

    //--------------------------------------------

    static constexpr InfoType type_id = InfoType::IT_typedef;

    TypedefInfo(
        SymbolID id_ = SymbolID())
        : SymbolInfo(InfoType::IT_typedef, id_)
    {
    }
};

} // mrdox
} // clang

#endif
