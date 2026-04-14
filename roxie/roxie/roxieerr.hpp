#ifndef ROXIEERR_HPP
#define ROXIEERR_HPP

#include "jexcept.hpp"
#include "errorlist.h"

/* Errors generated in roxie */

#define ROXIEERR_FileContainedALineOfLengthGreater 1470 // File contained a line of length greater than %d bytes.
#define ROXIEERR_PersistNotSupportedWhenRunningPredeployedQueries 1471 // PERSIST not supported when running predeployed queries
#define ROXIEERR_ErrmsgStr 1472 // errmsg.str(
#define ROXIEERR_CriticalNotSupportedWhenRunningPredeployedQueries 1473 // CRITICAL not supported when running predeployed queries
#define ROXIEERR_CannotObtainCriticalSectionLock 1474 // Cannot obtain Critical section lock
#define ROXIEERR_InvalidPersistNameUsedS 1475 // Invalid persist name used : '%s'
#define ROXIEERR_MissingOrInvalidWorkunitNameSIn 1476 // Missing or invalid workunit name %s in getExternalResult()
#define ROXIEERR_FailedToRetrieveHashValueSFrom 1477 // Failed to retrieve hash value %s from workunit %s
#define ROXIEERR_UnknownClusterS 1478 // Unknown cluster '%s'
#define ROXIEERR_GetgroupnameAmbiguousGroupsSS 1479 // getGroupName(): ambiguous groups %s, %s
#define ROXIEERR_ErrorCannotSwitchClusterIfTargetingRoxie 1480 // Error - cannot switch cluster if targeting roxie
#define ROXIEERR_ErrorDaliNotConnected 1481 // Error - dali not connected
#define ROXIEERR_LogicalNameSInvalid 1482 // Logical name %s invalid
#define ROXIEERR_RoxieDoesNotSupportFetchOrKeyed 1483 // Roxie does not support FETCH or KEYED JOIN to superkey with multiple parts
#define ROXIEERR_TranslatableFileLayoutMismatchReadingFileS 1484 // Translatable file layout mismatch reading file %s but translation disabled when expected fields are missing from source.
#define ROXIEERR_UnknownClusterSWhileWritingFileS 1485 // Unknown cluster %s while writing file %s
#define ROXIEERR_ClusterSOccupiesNodeAlreadySpecifiedWhile 1486 // Cluster %s occupies node already specified while writing file %s
#define ROXIEERR_InvalidMemindexSpecificationMissingFieldName 1487 // Invalid MemIndex specification - missing field name
#define ROXIEERR_InvalidMemindexSpecificationFieldNameSNot 1488 // Invalid MemIndex specification - field name %s not found (fields are %s)
#define ROXIEERR_InmemorydirectreaderGetRequestedUBytesOnlyU 1489 // InMemoryDirectReader::get: requested %u bytes, only %u available
#define ROXIEERR_BuffereddirectreaderGetRequestedUBytesAtEof 1490 // BufferedDirectReader::get: requested %u bytes at eof
#define ROXIEERR_BuffereddirectreaderSkipTriedToSkipUBytes 1491 // BufferedDirectReader::skip: tried to skip %u bytes at eof
#define ROXIEERR_FailedToLoadProtocolLibraryS 1492 // Failed to load protocol library %s
#define ROXIEERR_FailedToLoadProtocolLibrarySLoadhpccprotocolplugin 1493 // Failed to load protocol library %s loadHpccProtocolPlugin function
#define ROXIEERR_ProtocolLibrarySLoadhpccprotocolpluginFunctionFailed 1494 // Protocol library %s loadHpccProtocolPlugin function failed
#define ROXIEERR_ErrorInvalidSQuerynameNotFoundReceived 1495 // ERROR: Invalid %s queryName not found - received from %s:%d - %s
#define ROXIEERR_NoAgentsAvailableForChannelD 1496 // No agents available for channel %d
#define ROXIEERR_Interrupted 1497 // Interrupted
#define ROXIEERR_MemoryLimitExceeded 1498 // memory limit exceeded
#define ROXIEERR_AttemptingToReadFlatFileAsAn 1499 // Attempting to read flat file as an index: %s
#define ROXIEERR_AttemptingToReadIndexAsAFlat 1500 // Attempting to read index as a flat file: %s
#define ROXIEERR_CannotCombineExtendAndClusterFlagsOn 1501 // Cannot combine EXTEND and CLUSTER flags on disk write of file %s
#define ROXIEERR_CannotWriteIndexFileSFileAlready 1502 // Cannot write index file %s, file already exists (missing OVERWRITE attribute?)
#define ROXIEERR_IndexMaximumRecordLengthDExceeds32k 1503 // Index maximum record length (%d) exceeds 32k internal limit
#define ROXIEERR_LibraryCannotWriteToResultSThat 1504 // Library cannot write to result %s that does not exist in calling query
#define ROXIEERR_LibraryCannotWriteToResultSResult 1505 // Library cannot write to result %s: result type in query does not match
#define ROXIEERR_DaliResultOutputsAreRestrictedToA 1506 // Dali result outputs are restricted to a maximum of %d MB, the current limit is %d MB. A huge dali result usually indicates the ECL needs altering.
#define ROXIEERR_CannotWriteSFileAlreadyExistsMissing 1507 // Cannot write %s, file already exists (missing OVERWRITE attribute?)
#define ROXIEERR_FatalErrorReceivedUBytesOfData 1508 // Fatal error: received %u bytes of data, not a multiple of -ow %u
#define ROXIEERR_MemoryPoolExhausted 1509 // memory pool exhausted
#define ROXIEERR_TwoIpPortsMappedToTheSame 1510 // Two ip/ports mapped to the same port - improve the hash (or change maxPorts)!

#endif
