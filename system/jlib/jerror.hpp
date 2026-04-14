/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems®.

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


#ifndef JERROR_HPP
#define JERROR_HPP

#include "jexcept.hpp"
#include "errorlist.h"

#if (JLIB_ERROR_START != 6000 || JLIB_ERROR_END != 6499)
#error "JLIB_ERROR_START has changed"
#endif

/* Errors generated in jlib */

#define JLIBERR_BadlyFormedDateTime             6000
#define JLIBERR_BadUtf8InArguments              6001
#define JLIBERR_InternalError                   6002
#define JLIBERR_CppCompileError                 6003
#define JLIBERR_UnexpectedValue                 6004
#define JLIBERR_K8sServiceError                 6005

#define JLIBERR_PluginKindNotSpecified         6010
#define JLIBERR_UnableToLoadLibrary            6011
#define JLIBERR_FactoryFunctionNotFound        6012
#define JLIBERR_FeatureConfigNotFound          6013
#define JLIBERR_FactoryReturnedNull            6014

#define JLIBERR_ArrayOverflow                  6015
#define JLIBERR_ArrayAllocationFailed          6016
#define JLIBERR_ArrayTooFewItems               6017

#define JLIBERR_BufferErrorReadingFile         6018
#define JLIBERR_BufferUnableToCreateFile       6019
#define JLIBERR_BufferDiskFull                 6020
#define JLIBERR_BufferErrorWritingFile         6021

#define JLIBERR_SocketInvalidNetworkAddress    6025
#define JLIBERR_SocketSelectError              6026
#define JLIBERR_SocketEpollError               6027
#define JLIBERR_SocketEpollInvalidState        6028
#define JLIBERR_SocketInvalidEndpointName      6029
#define JLIBERR_SocketWaitMultipleMalloc       6030
#define JLIBERR_SocketWaitMultipleError        6031
#define JLIBERR_BufferedSocketFromNull         6032

#define JLIBERR_FileCopyTargetCreateFailed          6033
#define JLIBERR_FileCopySourceNotFound              6034
#define JLIBERR_FileRenameSourceNotFound            6035
#define JLIBERR_FileRenameSourceInvalid             6036
#define JLIBERR_FileRenameSourceReadOnly            6037
#define JLIBERR_FileRenameTargetCreateFailed        6038
#define JLIBERR_FileRenameTargetExists              6039
#define JLIBERR_ExtractBlobElementsFileNotFound     6040
#define JLIBERR_FileLockInvalidParameter            6041
#define JLIBERR_MemoryMapRemoteFileError            6042
#define JLIBERR_MemoryBufferReadBeyondEnd           6043
#define JLIBERR_MemoryBufferWriteBeyondEndUnimplemented  6044
#define JLIBERR_MemoryBufferSetSizeBeyondEndUnimplemented 6045
#define JLIBERR_SequentialFileOutOfSequence         6046
#define JLIBERR_FileStreamSeekNotSeekable           6047
#define JLIBERR_CreateIFileCannotResolve            6048
#define JLIBERR_CreateIFileCannotAttach             6049
#define JLIBERR_TouchFileFailedToCreate             6050
#define JLIBERR_CreateHardLinkFailed                6051
#define JLIBERR_CreateLogAliasFailed                6052
#define JLIBERR_BadlyFormattedFileEntry             6053
#define JLIBERR_ComponentFileIPMismatch             6054
#define JLIBERR_MakeAbsolutePathResolveFailed       6055
#define JLIBERR_FailedToOpenPartFileAnyLocation     6056
#define JLIBERR_InputStreamReadPastEnd              6057
#define JLIBERR_StreamFileTooBigToMap               6058
#define JLIBERR_MappedStreamReadPastEnd             6059
#define JLIBERR_MappedStreamSkipPastEnd             6060
#define JLIBERR_BufferStreamReadPastEnd             6061
#define JLIBERR_BufferStreamSkipPastEnd             6062
#define JLIBERR_MemoryMappedFileOutsideMap          6063
#define JLIBERR_MemoryMappedFileReinitTooBig        6064
#define JLIBERR_CachedFileIONotSupported            6065
#define JLIBERR_CachedFileIOOpenFailed              6066
#define JLIBERR_InvalidIFileInputSortedDirIterator  6067
#define JLIBERR_InvalidDirNameInputSortedDirIterator 6068
#define JLIBERR_FileEventWatcherAddInvalidEmpty     6069
#define JLIBERR_ReentrantCallCBlockedFileIORead     6070

//---- Text for all errors (make it easy to internationalise) ---------------------------

#define JLIBERR_BadlyFormedDateTime_Text        "Badly formatted date/time '%s'"
#define JLIBERR_BadUtf8InArguments_Text         "The utf separators/terminators aren't valid utf-8"

#endif
