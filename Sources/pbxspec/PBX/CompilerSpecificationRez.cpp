// Copyright 2013-present Facebook. All Rights Reserved.

#include <pbxspec/PBX/CompilerSpecificationRez.h>

using pbxspec::PBX::CompilerSpecificationRez;

CompilerSpecificationRez::CompilerSpecificationRez(bool isDefault) :
    Tool(isDefault, ISA::PBXCompilerSpecificationRez)
{
}