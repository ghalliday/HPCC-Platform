/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2020 HPCC Systems®.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */

#ifndef DAMETA_HPP
#define DAMETA_HPP

#ifdef DALI_EXPORTS
#define da_decl DECL_EXPORT
#else
#define da_decl DECL_IMPORT
#endif

#include "jptree.hpp"
#include "dasess.hpp"

enum ResolveOptions : unsigned {
    ROnone              = 0x00000000,
    ROincludeLocation   = 0x00000001,
    ROsign              = 0x00000002,
    ROpartinfo          = 0x00000004,
    ROtimestamps        = 0x00000008,
    ROsecrets           = 0x00000010,       // Needs careful thought to ensure they can't leak to the outside world.
    ROall               = ~0U
};

constexpr ResolveOptions operator |(ResolveOptions l, ResolveOptions r) { return (ResolveOptions)((unsigned)l | (unsigned)r); }
constexpr ResolveOptions operator &(ResolveOptions l, ResolveOptions r) { return (ResolveOptions)((unsigned)l & (unsigned)r); }

extern da_decl IPropertyTree * resolveLogicalFilename(const char * filename, IUserDescriptor * user, ResolveOptions options);

#endif
