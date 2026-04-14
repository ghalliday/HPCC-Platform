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

#define JLIBERR_CompressClzwcompressorTargetBufferTooSmall 6100
#define JLIBERR_CompressCorruptRleFormat                   6101
#define JLIBERR_CompressCorruptCompressedData1             6102
#define JLIBERR_CompressCorruptCompressedData2             6103
#define JLIBERR_CompressCorruptCompressedData3             6104
#define JLIBERR_CompressCorruptCompressedData4             6105
#define JLIBERR_CompressCrdiffcompressorRowDoesnTFitInBuffer 6106
#define JLIBERR_CompressCrdiffcompressorUsedWithVariableSizedRow 6107
#define JLIBERR_CompressCrdiffcompressorUsedWithVariableSizedRow_1 6108
#define JLIBERR_CompressCrdiffcompressorTargetBufferTooSmall 6109
#define JLIBERR_CompressCrdiffexpanderInvalidBufferFormat  6110
#define JLIBERR_CompressCrdiffexpanderInvalidBufferFormat_1 6111
#define JLIBERR_CompressCrandrdiffcompressorUsedWithVariableSizedRow 6112
#define JLIBERR_CompressCrandrdiffcompressorUsedWithVariableSizedRow_1 6113
#define JLIBERR_CompressCrandrdiffcompressorTargetBufferTooSmall 6114
#define JLIBERR_CompressCrandrdiffcompressorUsedWithVariableSizedRowU 6115
#define JLIBERR_CompressFileHasCompressionTypeUWhichIs     6116
#define JLIBERR_CompressUnexpectedZeroLengthCompressionBlock 6117
#define JLIBERR_CompressReadPastEndOfIoBuffer              6118
#define JLIBERR_CompressReadPastEndOfIoBuffer_1            6119
#define JLIBERR_CompressUnexpectedZeroFillInCompressedFileAt 6120
#define JLIBERR_CompressUnsupportedCompressionMethodU      6121
#define JLIBERR_CompressSequentialWritesOnlyOnCompressedFile 6122
#define JLIBERR_CompressCompressedFileFormatErrorDEncrypted 6123
#define JLIBERR_CompressCompressedFileFormatErrorDEncrypted_1 6124
#define JLIBERR_CompressAppendingToARowCompressedFileIs    6125
#define JLIBERR_CompressAppendingToFileThatIsNotCompressed 6126
#define JLIBERR_CompressUnsupportedCompressionMethodU_1    6127
#define JLIBERR_CompressCaescompressorTargetBufferTooSmall 6128
#define JLIBERR_CompressSetdefaultcompressorSCompressorNotRegistered 6129
#define JLIBERR_CompressLzmaencCreateFailed                6130
#define JLIBERR_CompressLzmaencSetpropsFailed              6131
#define JLIBERR_CompressLzmaencMemencodeFailedD            6132
#define JLIBERR_CompressLzmadecodeFailedD                  6133
#define JLIBERR_CompressFastlzdecompresstobufferCorruptData1DD 6134
#define JLIBERR_CompressFastlzdecompresstobufferCorruptData3DD 6135
#define JLIBERR_CompressFastlzdecompresstobufferCorruptData2DD 6136
#define JLIBERR_CompressFastlzdecompresstobufferCorruptData4DD 6137
#define JLIBERR_CompressFastlzexpanderCorruptData1DD       6138
#define JLIBERR_CompressFastlzexpanderCorruptData2DD       6139
#define JLIBERR_CompressFastlzdecompresstobufferCorruptData1DD_1 6140
#define JLIBERR_CompressFastlzdecompresstobufferCorruptData3DD_1 6141
#define JLIBERR_CompressFastlzdecompresstobufferCorruptData2DD_1 6142
#define JLIBERR_CompressFastlzdecompresstobufferCorruptData4DD_1 6143
#define JLIBERR_CompressCfastlzstreamFileCorrupt1          6144
#define JLIBERR_CompressCfastlzstreamFileCorrupt2          6145
#define JLIBERR_CompressZstdCompressionErrorS              6146
#define JLIBERR_CompressZstdDecompressionErrorS            6147
#define JLIBERR_CompressZstdDecompressionErrorS_1          6148
#define JLIBERR_CompressFailedToCreateZstdCompressionStream 6149
#define JLIBERR_CompressFailedToInitializeZstdCompressionStreamS 6150
#define JLIBERR_CompressFailedToInitializeZstdCompressionStreamS_1 6151
#define JLIBERR_CompressZstdCompressionErrorSCompressLimitUU 6152
#define JLIBERR_CompressFailedToCreateZstdDecompressionStream 6153
#define JLIBERR_CompressFailedToResetZstdDecompressionStreamS 6154
#define JLIBERR_CompressZstdStreamDecompressionErrorS      6155
#define JLIBERR_CompressBlockexpanderCorruptData1UU        6156
#define JLIBERR_CompressBlockexpanderCorruptData2UU        6157
#define JLIBERR_CompressBlockexpanderCorruptData3UU        6158
#define JLIBERR_CompressStreamCompressorsDoesNotSupportMemorybufferOutput 6159
#define JLIBERR_CompressTotalSizeTooLargeUncompressedUU    6160
#define JLIBERR_CompressLz4compressorFlushcommittedInputSizeUExceedsMaximum 6161
#define JLIBERR_ParseCouldNotLocateFilenameS               6170
#define JLIBERR_ParseSavexmlCouldNotFindSToOpen            6171
#define JLIBERR_ParseInvalidExtractXmlTextUsageXpath       6172
#define JLIBERR_ParseNotAllowedInParentNodeOf              6173
#define JLIBERR_ParseNotAllowedInParentNodeOf_1            6174
#define JLIBERR_ParseInvalidUrlParameterFormatS            6175
#define JLIBERR_ParseInvalidRestQueryInputSpecifierS       6176
#define JLIBERR_ParseConfigurationFileSNotFound            6177
#define JLIBERR_ParseErrorLoadingConfigurationFileSInvalidYaml 6178
#define JLIBERR_ParseUnrecognisedFileExtensionS            6179
#define JLIBERR_ParseErrorLoadingConfigurationFileS        6180
#define JLIBERR_ParseSectionSIsMissingFromFileS            6181
#define JLIBERR_ParseCannotOverrideScalarConfigurationElementSWith 6182
#define JLIBERR_ParseInvalidOptionNameS                    6183
#define JLIBERR_ParseConfigurationHasAlreadyBeenInitialised 6184
#define JLIBERR_ParseNameOfConfigurationFileOmittedUseConfigFilename 6185
#define JLIBERR_ParseConfigurationForComponentSHasAlreadyBeen 6186
#define JLIBERR_ParseConfigurationForComponentSHasAlreadyBeen_1 6187
#define JLIBERR_ParseDefaultConfigurationDoesNotContainTheTag 6188
#define JLIBERR_ParseFiledToInitializeLibyamlParser        6189
#define JLIBERR_ParseLibyamlParserErrorS                   6190
#define JLIBERR_ParseLibyamlParserS                        6191
#define JLIBERR_ParseLibyamlParserExpectedSequenceName     6192
#define JLIBERR_ParseYamlCurrentlyOnlySupportOneContentSection 6193
#define JLIBERR_ParseYamlCurrentlyOnlySupportOneContentSection_1 6194
#define JLIBERR_ParseYamlCurrentlyOnlySupportOneDocumentPer 6195
#define JLIBERR_ParseYamlemitterFailedToInitialize         6196
#define JLIBERR_ParseYamlemitterSFailed                    6197
#define JLIBERR_ParseSavexmlCouldNotFindSToOpen_1          6198
#define JLIBERR_ParseDuplicateEntry                        6199
#define JLIBERR_ParseDuplicateEntry_1                      6200
#define JLIBERR_SystemNiceLevelShouldBeBetween20And        6210
#define JLIBERR_SystemNiceCanOnlyBeSetBeforeThe            6211
#define JLIBERR_SystemUnknownExceptionInThreadS            6212
#define JLIBERR_SystemThreadStartSThreadAlreadyStarted     6213
#define JLIBERR_SystemThreadStartSFailed                   6214
#define JLIBERR_SystemUnknownExceptionInThreadS_1          6215
#define JLIBERR_SystemUnknownExceptionInMainThread         6216
#define JLIBERR_SystemUnknownExceptionInThreadFromPoolS    6217
#define JLIBERR_SystemNoThreadsAvailableInPoolS            6218
#define JLIBERR_SystemUnauthorizedPipeProgramS             6219
#define JLIBERR_SystemFailedToRunDoperf                    6220
#define JLIBERR_SystemCanNotLockD                          6221
#define JLIBERR_SystemInvalidEventTypeU                    6222
#define JLIBERR_SystemRecordingsourceCanOnlyBeTheFirstRecorded 6223
#define JLIBERR_SystemChannelidValueLluExceedsMaximumAllowedValue 6224
#define JLIBERR_SystemReplicaidValueLluExceedsMaximumAllowedValue 6225
#define JLIBERR_SystemInvalidAttributeTypeU                6226
#define JLIBERR_SystemNoDataTypeForAttributeS              6227
#define JLIBERR_SystemUnknownDataTypeDForAttributeS        6228
#define JLIBERR_SystemFileSNotFound                        6229
#define JLIBERR_SystemFileSNotOpenedForReading             6230
#define JLIBERR_SystemFileSIsNotAnEventFile                6231
#define JLIBERR_SystemUnsupportedFileVersionURequiredU     6232
#define JLIBERR_SystemUnexpectedEof                        6233
#define JLIBERR_SystemEofBeforeEndOfUByteString            6234
#define JLIBERR_SystemEofBeforeEndOfNullTerminatedString   6235
#define JLIBERR_SystemEofBeforeEndOfUByteString_1          6236
#define JLIBERR_SystemErrorWhileWritingByte0xXN            6237
#define JLIBERR_SystemErrorWhileWritingDBytesN             6238
#define JLIBERR_SystemTruncatedDWhileWritingDBytesN        6239
#define JLIBERR_SystemBlockedInputStreamRecordTooLargeTo   6240
#define JLIBERR_SystemEndOfInputStreamForReadOf            6241
#define JLIBERR_SystemBlockedInputStreamRecordTooLargeTo_1 6242
#define JLIBERR_SystemEndOfInputStreamForReadOf_1          6243
#define JLIBERR_SystemFailedToWriteUBytesAtOffset          6244
#define JLIBERR_SystemFailedToReadTheExpectedNumberOf      6245
#define JLIBERR_SystemFailedToReadTheExpectedNumberOf_1    6246
#define JLIBERR_SystemFailedToInitializeIoUringQueueErrorCode 6247
#define JLIBERR_SystemInvalidSecretCategoryS               6248
#define JLIBERR_SystemInvalidSecretNameS                   6249
#define JLIBERR_SystemInvalidSecretKeyNameS                6250
#define JLIBERR_SystemInvalidEmptyUrl                      6251
#define JLIBERR_SystemInvalidUrlProtocolNotRecognizedS     6252
#define JLIBERR_SystemVaultSSAuthErrorS                    6253
#define JLIBERR_SystemInvalidSecretNameS_1                 6254
#define JLIBERR_SystemSecretSSNotFound                     6255
#define JLIBERR_SystemSecretSSMissingKeyS                  6256
#define JLIBERR_SystemUdpKeyNotInitialized                 6257
#define JLIBERR_SystemUdpKeyNotFoundCertManagerIntegrationConfigurationRequired 6258
#define JLIBERR_SystemSecretSSNotFound_1                   6259
#define JLIBERR_SystemMonthShouldBetween1And12             6260
#define JLIBERR_SystemBadCronSpecS                         6261
//---- Text for all errors (make it easy to internationalise) ---------------------------

#define JLIBERR_BadlyFormedDateTime_Text        "Badly formatted date/time '%s'"
#define JLIBERR_BadUtf8InArguments_Text         "The utf separators/terminators aren't valid utf-8"

#endif



