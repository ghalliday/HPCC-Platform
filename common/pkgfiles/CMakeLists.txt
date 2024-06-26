################################################################################
#    HPCC SYSTEMS software Copyright (C) 2022 HPCC Systems®.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
################################################################################


# Component: pkgfiles
#####################################################
# Description:
# ------------
#    Cmake Input File for pkgfiles
#####################################################

project( pkgfiles )

set (    SRCS
         package.cpp
         referencedfilelist.cpp
    )

include_directories (
        ${HPCC_SOURCE_DIR}/system/include
        ${HPCC_SOURCE_DIR}/system/jlib
        ${HPCC_SOURCE_DIR}/system/security/shared
        ${HPCC_SOURCE_DIR}/system/mp
        ${HPCC_SOURCE_DIR}/dali/base
        ${HPCC_SOURCE_DIR}/dali/dfu
        ${HPCC_SOURCE_DIR}/rtl/include
        ${HPCC_SOURCE_DIR}/rtl/eclrtl
        ${HPCC_SOURCE_DIR}/common/workunit
        ${HPCC_SOURCE_DIR}/esp/clients/ws_dfsclient
    )

if (NOT CONTAINERIZED)
    include_directories ( ${HPCC_SOURCE_DIR}/common/environment )
endif()

HPCC_ADD_LIBRARY( pkgfiles SHARED ${SRCS} )

set_target_properties(pkgfiles PROPERTIES
    COMPILE_FLAGS -D_USRDLL
    DEFINE_SYMBOL PKGFILES_EXPORTS )
if(NOT PLUGIN)
    install(
        TARGETS pkgfiles
        RUNTIME DESTINATION ${EXEC_DIR}
        LIBRARY DESTINATION ${LIB_DIR})
    target_link_libraries(
        pkgfiles
        dfuwu
        ws_dfsclient
        )
    if (NOT CONTAINERIZED)
        target_link_libraries(pkgfiles environment)
    endif()
endif()
