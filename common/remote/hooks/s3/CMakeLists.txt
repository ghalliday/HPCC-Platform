################################################################################
#    HPCC SYSTEMS software Copyright (C) 2019 HPCC Systems®.
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


# Component: s3file
#####################################################
# Description:
# ------------
#    Cmake Input File for amazon s3 direct reading hook
#####################################################

project( s3file )

set (    SRCS
         s3file.cpp
         s3file.hpp
    )

include_directories (
         ${HPCC_SOURCE_DIR}/system/include
         ${HPCC_SOURCE_DIR}/system/jlib
         ${HPCC_SOURCE_DIR}/plugins/sqs/aws-sdk-cpp/aws-cpp-sdk-core/include     # Should move to system/aws_sm
         ${HPCC_SOURCE_DIR}/plugins/sqs/aws-sdk-cpp/aws-cpp-sdk-s3/include     # Should move to system/aws_sm
    )

ADD_DEFINITIONS( -D_USRDLL -DS3FILE_EXPORTS )

HPCC_ADD_LIBRARY( s3file SHARED ${SRCS}  )
install ( TARGETS s3file DESTINATION filehooks )
target_link_options(s3file  PUBLIC  "LINKER:-z,defs" )

#message(FATAL_ERROR "${CMAKE_BINARY_DIR}/plugins/sqs/aws-sdk-cpp/aws-cpp-sdk-core/libaws-cpp-sdk-core.so")
target_link_libraries ( s3file
    jlib
        ${CMAKE_BINARY_DIR}/oss/plugins/sqs/aws-sdk-cpp/aws-cpp-sdk-core/libaws-cpp-sdk-core.so
        ${CMAKE_BINARY_DIR}/oss/plugins/sqs/aws-sdk-cpp/aws-cpp-sdk-s3/libaws-cpp-sdk-s3.so
    )
