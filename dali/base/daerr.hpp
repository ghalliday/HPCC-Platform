#ifndef DAERR_HPP
#define DAERR_HPP

#include "jexcept.hpp"
#include "errorlist.h"

/* Errors generated in dali/base */

#define DALIERR_CouldNotConnectToS 5801 // Could not connect to %s
#define DALIERR_DaliMismatchedCovenServers 5802 // Dali mismatched Coven servers
#define DALIERR_UidBaseIncompatibilityAmongstCovenServers 5803 // UID base incompatibility amongst Coven servers
#define DALIERR_SdsEditionIncompatibilityAmongstCovenServers 5804 // SDS edition incompatibility amongst Coven Servers
#define DALIERR_NoAccessToDaliThisNormallyMeans 5805 // No access to Dali - this normally means a plugin call is being called from a thorslave
#define DALIERR_GetuniqueidsNotConnectedToDali 5806 // getUniqueIds: Not connected to dali
#define DALIERR_CannotSAMultiFileNameS 5807 // cannot %s a multi file name (%s)
#define DALIERR_CannotSAnExternalFileNameS 5808 // cannot %s an external file name (%s)
#define DALIERR_CannotSAForeignFileNameS 5809 // cannot %s a foreign file name (%s)
#define DALIERR_TransactionAlreadyStarted 5810 // Transaction already started
#define DALIERR_SerializefileattroptionsReadfieldsIncludeallMustBeBeLeading 5811 // SerializeFileAttrOptions::readFields 'includeAll' must be be leading specifier
#define DALIERR_SuperfileiterInvalidSuperFileOnQueryAt 5812 // superFileIter: invalid super-file on query at %s
#define DALIERR_SSSCompressionSettingSIs 5813 // %s: %s's compression setting (%s) is different than %s's (%s)
#define DALIERR_SSSBlockedSettingSIs 5814 // %s: %s's blocked setting (%s) is different than %s's (%s)
#define DALIERR_SSSRecordLayoutSIs 5815 // %s: %s's record layout (%s) is different than %s's (%s)
#define DALIERR_SSSRecordSizeDIs 5816 // %s: %s's record size (%d) is different than %s's (%d)
#define DALIERR_SSSFormatSIsDifferent 5817 // %s: %s's format (%s) is different than %s's (%s)
#define DALIERR_SSSLocalSettingSIs 5818 // %s: %s's local setting (%s) is different than %s's (%s)
#define DALIERR_SSSReplicationOffsetDIs 5819 // %s: %s's replication offset (%d) is different than %s's (%d)
#define DALIERR_DetachS 5820 // detach: %s
#define DALIERR_CannotRemoveAForeignFileS 5821 // cannot remove a foreign file (%s)
#define DALIERR_CfileclusterownerRemoveclusterCannotRemoveSoleClusterS 5822 // CFileClusterOwner::removeCluster cannot remove sole cluster %s
#define DALIERR_SomePhysicalPartsDoNotExistsFor 5823 // Some physical parts do not exists, for logical file : %s
#define DALIERR_OperationNotAllowedOnForeignFile 5824 // Operation not allowed on foreign file
#define DALIERR_AddsubfileSuperfileSCannotBeFound 5825 // addSubFile: SuperFile %s cannot be found
#define DALIERR_CaddsubfileactionSubFileSNotFound 5826 // cAddSubFileAction: sub file %s not found
#define DALIERR_AddsubfileFileSCannotBeFoundTo 5827 // addSubFile: File %s cannot be found to add
#define DALIERR_Addsubfile2FileSCannotBeFound 5828 // addSubFile(2): File %s cannot be found to add
#define DALIERR_RemovesubfileSuperfileSCannotBeFound 5829 // removeSubFile: SuperFile %s cannot be found
#define DALIERR_RemoveownedsubfilesSuperfileSCannotBeFound 5830 // removeOwnedSubFiles: SuperFile %s cannot be found
#define DALIERR_SwapsuperfileSuperfileSCannotBeFound 5831 // swapSuperFile: SuperFile %s cannot be found
#define DALIERR_CdistributedsuperfileSS 5832 // CDistributedSuperFile::%s %s
#define DALIERR_SStr 5833 // s.str(
#define DALIERR_C2CorruptSubfileFilePartD 5834 // C(2): Corrupt subfile file part %d cannot be found
#define DALIERR_Cdistributedsuperfile3CorruptSubfileFilePartD 5835 // CDistributedSuperFile(3): Corrupt subfile file part %d cannot be found
#define DALIERR_Cdistributedsuperfile2CorruptSubfileFilePartD 5836 // CDistributedSuperFile(2): Corrupt subfile file part %d cannot be found
#define DALIERR_RenamephysicalpartfilesNotSupportedForSuperfiles 5837 // renamePhysicalPartFiles not supported for SuperFiles
#define DALIERR_AddsubfileCannotAddFileSToItself 5838 // addSubFile: Cannot add file %s to itself
#define DALIERR_AddsubfileFileSIsAlreadyASubfile 5839 // addSubFile: File %s is already a subfile of %s
#define DALIERR_LogicalSubfileSDoesnTExists 5840 // Logical subfile '%s' doesn't exists!
#define DALIERR_TheValueOfNumsubfilesDIsNot 5841 // The value of @numsubfiles (%d) is not equal to the number of SubFile items (%d)!
#define DALIERR_AddsubfileInsertPositionDOutOfRange 5842 // addSubFile: Insert position %d out of range for file %s in superfile %s
#define DALIERR_Err 5843 // err
#define DALIERR_InvalidGroupRangeS 5844 // Invalid group range %s
#define DALIERR_InvalidFilenameLookupS 5845 // Invalid filename lookup: %s
#define DALIERR_RenameCannotRenameExternalFiles 5846 // rename: cannot rename external files
#define DALIERR_RenameCannotRenameForeignFiles 5847 // rename: cannot rename foreign files
#define DALIERR_CreatesuperfileSuperfileSAlreadyExists 5848 // createSuperFile: SuperFile %s already exists
#define DALIERR_CinitgroupsAddclustergroupCalledInReadOnlyMode 5849 // CInitGroups::addClusterGroup called in read-only mode
#define DALIERR_CinitgroupsClearunprotectedgroupsCalledInReadOnlyMode 5850 // CInitGroups::clearUnprotectedGroups called in read-only mode
#define DALIERR_CannotResolveDaliIpInForeignFile 5851 // cannot resolve dali ip in foreign file name (%s)
#define DALIERR_UnknownGetfiletreeSerializationVersionD 5852 // Unknown GetFileTree serialization version %d
#define DALIERR_SInvalidSuperfileNameS 5853 // %s: Invalid superfile name '%s'
#define DALIERR_SCannotCreateSuperfileS 5854 // %s: Cannot create superfile '%s'
#define DALIERR_PromotesuperfilesSuperfilesSAndSShareSame 5855 // promoteSuperFiles: superfiles %s and %s share same subfile %s
#define DALIERR_PromotesuperfilesInvalidLogicalNameToAddS 5856 // promoteSuperFiles: invalid logical name to add: %s
#define DALIERR_CdistributedfiledirectoryGetfilesuperownersInvalidFileNameS 5857 // CDistributedFileDirectory::getFileSuperOwners: Invalid file name '%s'
#define DALIERR_SInvalidLogicalFileNameS 5858 // %s: invalid logical file name '%s'
#define DALIERR_WildcardNotAllowedInAddfilerelation 5859 // Wildcard not allowed in addFileRelation
#define DALIERR_AddfilerelationshipInvalidPrimaryNameS 5860 // addFileRelationship invalid primary name '%s'
#define DALIERR_AddfilerelationshipPrimarySNotAllowed 5861 // addFileRelationship primary %s not allowed
#define DALIERR_AddfilerelationshipPrimarySDoesNotExist 5862 // addFileRelationship primary %s does not exist
#define DALIERR_AddfilerelationshipInvalidSecondaryNameS 5863 // addFileRelationship invalid secondary name '%s'
#define DALIERR_AddfilerelationshipSecondarySNotAllowed 5864 // addFileRelationship secondary %s not allowed
#define DALIERR_AddfilerelationshipSecondarySDoesNotExist 5865 // addFileRelationship secondary %s does not exist
#define DALIERR_AddfilerelationshipCardinalitySInvalid 5866 // addFileRelationship cardinality %s invalid
#define DALIERR_RemovefilerelationshipsPrimaryAndSecondaryCannotBothBe 5867 // removeFileRelationships primary and secondary cannot both be wild
#define DALIERR_LookupfilerelationshipsCannotResolveForeignDaliS 5868 // lookupFileRelationships::Cannot resolve foreign dali %s
#define DALIERR_RemoveallfilerelationshipsFilenameCannotBeWild 5869 // removeAllFileRelationships filename cannot be wild
#define DALIERR_WildcardFilenameNotAllowedInLookupallfilerelationships 5870 // Wildcard filename not allowed in lookupAllFileRelationships
#define DALIERR_RemoteFilenameCannotResolveSinglePartFrom 5871 // Remote Filename: Cannot resolve single part from wild/multi filename
#define DALIERR_IfiledescriptorSetupAlreadyDone 5872 // IFileDescriptor - setup already done
#define DALIERR_FiledescriptorSerializationVersionMismatchDD 5873 // FileDescriptor serialization version mismatch %d/%d
#define DALIERR_MissingNodeInExternalFileNameS 5874 // missing node in external file name (%s)
#define DALIERR_CannotResolveNodeSInExternalFile 5875 // cannot resolve node %s in external file name (%s)
#define DALIERR_SetbasedirectorySRequiresAnAbsolutePath 5876 // setBaseDirectory(%s) requires an absolute path
#define DALIERR_CreatefiledescriptorfromroxieSMissingDirectory 5877 // createFileDescriptorFromRoxie: %s missing directory
#define DALIERR_CreatefiledescriptorfromroxieSMissingPartMask 5878 // createFileDescriptorFromRoxie: %s missing part mask
#define DALIERR_CreatefiledescriptorfromroxieSMissingPart1 5879 // createFileDescriptorFromRoxie: %s missing part 1
#define DALIERR_CreatefiledescriptorfromroxieSMissingPart1LocPath 5880 // createFileDescriptorFromRoxie: %s missing part 1 loc path
#define DALIERR_CreatefiledescriptorfromroxieSMissingPart1Loc 5881 // createFileDescriptorFromRoxie: %s missing part 1 Loc
#define DALIERR_CreatefiledescriptorfromroxieSMissingPartD 5882 // createFileDescriptorFromRoxie: %s missing part %d
#define DALIERR_CreatefiledescriptorfromroxieSMissingPartDLocPath 5883 // createFileDescriptorFromRoxie: %s missing part %d loc path
#define DALIERR_CreatefiledescriptorfromroxieSCannotDetermineReplicationOffset 5884 // createFileDescriptorFromRoxie: %s cannot determine replication offset
#define DALIERR_CreatefiledescriptorfromroxiexmlClusterSNotFound 5885 // createFileDescriptorFromRoxieXML: Cluster %s not found
#define DALIERR_CreatefiledescriptorfromroxiexmlClusterSDoesNotMatchXml 5886 // createFileDescriptorFromRoxieXML: Cluster %s does not match XML
#define DALIERR_DaliNamedQueuesTryingToStartNested 5887 // Dali Named Queues: trying to start nested transaction frames
#define DALIERR_ClientTooOldToCommunicateWithThis 5888 // Client too old to communicate with this dali
#define DALIERR_MultipleStoreXFilesOnlyOneCorresponding 5889 // Multiple store.X files - only one corresponding to latest dalisds<X>.xml should exist
#define DALIERR_UnknownExceptionLoadingSStoreFileS 5890 // Unknown exception - loading %s store file : %s
#define DALIERR_SDoesNotExist 5891 // '%s' does not exist
#define DALIERR_FailedToOpenS 5892 // Failed to open '%s'
#define DALIERR_ExternalEnvironmentFileSHasSAs 5893 // External environment file '%s', has '%s' as root, expecting a 'Environment' xml node.
#define DALIERR_FailedToInitializePluginStoreS 5894 // Failed to initialize plugin store '%s'
#define DALIERR_SaveAlreadyInProgress 5895 // Save already in progress
#define DALIERR_MissingPositionAttributeInChildReferenceSection 5896 // Missing position attribute in child reference, section end offset=%
#define DALIERR_FailedToLocateDeltaChangeInS 5897 // Failed to locate delta change in %s, section end offset=%
#define DALIERR_FailedToLocateHeaderXpathS 5898 // Failed to locate header xpath = %s
#define DALIERR_BadlyConstructedDeltaFormatMissingDeltaT 5899 // Badly constructed delta format (missing Delta/T) in header path=%s, section end offset=%
#define DALIERR_SubscriptionNotificationAborted 5900 // Subscription notification aborted
#define DALIERR_SubscriptionNotificationToSTimedOut 5901 // Subscription notification to %s timed out
#define DALIERR_DropZoneNameRequired 5902 // Drop zone name required
#define DALIERR_BlankDfsXmlBranchType 5903 // Blank DFS xml branch type
#define DALIERR_MustCallCdfslogicalfilenameExpandBeforeCallingCdfslogicalfilename 5904 // Must call CDfsLogicalFileName::expand() before calling CDfsLogicalFileName::isForeign(), wildcards are specified
#define DALIERR_ScopeContainsTrailingSpacesInFileName 5905 // Scope contains trailing spaces in file name '%s'
#define DALIERR_ScopeContainsLeadingSpacesInFileName 5906 // Scope contains leading spaces in file name '%s'
#define DALIERR_ScopeIsBlankInFileNameS 5907 // Scope is blank in file name '%s'
#define DALIERR_NoDirectorySpecifiedInExternalFileName 5908 // No directory specified in external file name (%s)
#define DALIERR_PathCannotContainSeparatorsUseToSeparate 5909 // Path cannot contain separators, use '::' to separate directories: (%s)
#define DALIERR_ExternalFilenameCannotContainRelativePathS 5910 // External filename cannot contain relative path '..' (%s)
#define DALIERR_PathCannotContainSingleUseCTo 5911 // Path cannot contain single ':', use 'c$' to indicate 'c:' (%s)
#define DALIERR_UnexpectedCharacterCInLogicalNameS 5912 // Unexpected character '%c' in logical name '%s' detected
#define DALIERR_WildcardsNotAllowedInFilenameS 5913 // Wildcards not allowed in filename (%s)
#define DALIERR_LogicalFilenameCannotEndWithScope 5914 // Logical filename cannot end with scope \
#define DALIERR_FileNotExternalS 5915 // File not external (%s)
#define DALIERR_AttemptedToCreateEmptyDaliPath 5916 // Attempted to create empty DALI path
#define DALIERR_CouldNotCreateDaliSBranch 5917 // Could not create DALI %s branch
#define DALIERR_CdalimutexEnterCannotCreateSBranch 5918 // CDaliMutex::enter Cannot create %s branch
#define DALIERR_CdalimutexEnterCannotCreateLocksBranch 5919 // CDaliMutex::enter Cannot create /Locks branch
#define DALIERR_CdalimutexLeaveWithoutCorrespondingEnter 5920 // CDaliMutex leave without corresponding enter
#define DALIERR_UnableToUpdateIsysinfologgermsg 5921 // Unable to update ISysInfoLoggerMsg
#define DALIERR_IsysinfologgermsgfilterCannotFilterByBothHiddenonlyAnd 5922 // ISysInfoLoggerMsgFilter: cannot filter by both hiddenOnly and visibleOnly
#define DALIERR_IsysinfologgermsgfilterMonthAndYearMustBeProvided 5923 // ISysInfoLoggerMsgFilter: month and year must be provided when filtering by day
#define DALIERR_IsysinfologgermsgfilterYearMustBeProvidedWhenFiltering 5924 // ISysInfoLoggerMsgFilter: year must be provided when filtering by month




#define DALIERR_Pszerrmsg 5925 // pszErrMsg
#define DALIERR_UnrecognizedAuthMethodSpecifiedAuthS 5926 // Unrecognized auth method specified, (auth: %s)
#define DALIERR_UnableToLocateLdapDefaultsFileS 5927 // Unable to locate LDAP defaults file '%s'
#define DALIERR_CouldNotDeduceTheEndOfThe 5928 // Could not deduce the end of the record - pathological 
#define DALIERR_UnrecognisedCharacterEscapeSequenceC 5929 // unrecognised character escape sequence '\\%c'
#define DALIERR_LaunchftslavecmdFailedToFindDafilesrvServiceNamed 5930 // launchFtSlaveCmd: failed to find dafilesrv service named: '%s'
#define DALIERR_LaunchftslavecmdConfiguredServiceSIsOfApplication 5931 // launchFtSlaveCmd: configured service '%s' is of application type '%s' ('spray' type required)
#define DALIERR_LaunchftslavecmdNoSprayDafilesrvServicesFound 5932 // launchFtSlaveCmd: no 'spray' dafilesrv services found
#define DALIERR_PushingToMultipleTargetsWithNoSplit 5933 // Pushing to multiple targets with no split is not supported to this target plane (%s)
#define DALIERR_FailedToOpenFileS 5934 // Failed to open file %s
#define DALIERR_RequestedOutputFieldSNotFound 5935 // Requested output field '%s' not found
#define DALIERR_CannotFilterOnPayloadFieldS 5936 // Cannot filter on payload field '%s'
#define DALIERR_ScaWorkunitServicesGetNotImplementedFor 5937 // SCA_WORKUNIT_SERVICES_GET not implemented for online workunits!
#define DALIERR_CouldNotConnectToSashaServerOn 5938 // Could not connect to Sasha server on %s
#define DALIERR_SashaServerSProtocolError 5939 // Sasha server[%s]: Protocol error
#define DALIERR_ServiceUndefined 5940 // 'service' undefined
#define DALIERR_UnrecognisedServiceS 5941 // Unrecognised 'service': %s
#define DALIERR_CouldNotCreateFileS 5942 // Could not create file %s
#define DALIERR_CouldNotOpenFileS 5943 // Could not open file %s
#define DALIERR_XrefallocatorAllocRequestedSizeTooLargeReq 5944 // XRefAllocator::alloc : Requested size too large: req: %d, used: %zu, max: %zu
#define DALIERR_CfiledescCreateNumpartsTooLargeDMax 5945 // cFileDesc::create : numParts too large: %d (max 4096)
#define DALIERR_LogpfxIsmisplacedInvalidDirectoryNameInFile 5946 // LOGPFX "isMisplaced: Invalid directory name in file path: %s"
#define DALIERR_LogpfxFailedToCreateDirectoryS 5947 // LOGPFX "Failed to create directory: %s"
#define DALIERR_CapabilitiesBinaryIsTooLarge 5948 // Capabilities binary is too large
#define DALIERR_CapabilitiesBinaryWrongFormat 5949 // Capabilities binary wrong format
#define DALIERR_UnknownRoleS 5950 // Unknown role: %s
#define DALIERR_InvalidFormatRole 5951 // Invalid format (role)
#define DALIERR_InvalidFormatAddress 5952 // Invalid format address)
#define DALIERR_ExpectingXpathTailNodeInS 5953 // Expecting xpath tail node in: %s
#define DALIERR_InvalidXpathEmpty 5954 // Invalid xpath (empty)
#define DALIERR_InvalidXpathNoTrailingXpathNodeProvided 5955 // Invalid xpath (no trailing xpath node provided)
#define DALIERR_ExternalFileNotSupported 5956 // External file not supported
#define DALIERR_ForeignFileNotSupported 5957 // Foreign file not supported
#define DALIERR_CouldNotFindFileS 5958 // Could not find file: '%s'
#define DALIERR_CannotBeUsedOnASuperfile 5959 // Cannot be used on a superfile
#define DALIERR_InvalidPartNumberMustBeInThe 5960 // Invalid part number, must be in the range 1 - %u
#define DALIERR_InvalidModeS 5961 // Invalid mode: %s
#define DALIERR_FailedToConnectToEnvironmentSoftware 5962 // Failed to connect to Environment/Software
#define DALIERR_FailedToConnectToWorkunits 5963 // Failed to connect to /WorkUnits
#define DALIERR_ExpectingUidSGenerateddllDButFound 5964 // Expecting uid=%s @ GeneratedDll[%d], but found uid=%s
#define DALIERR_SourceAndTargetClusterGroupsCannotBe 5965 // source and target cluster groups cannot be the same! cluster = %s
#define DALIERR_CouldNotFindSourceClusterGroupS 5966 // Could not find source cluster group: %s
#define DALIERR_CouldNotFindTargetClusterGroupS 5967 // Could not find target cluster group: %s
#define DALIERR_UnsupportedTargetClusterIsWiderThanSource 5968 // Unsupported - target cluster is wider than source (target size=%u, source size=%u
#define DALIERR_UnsupportedTargetClusterMustBeAFactor 5969 // Unsupported - target cluster must be a factor of source cluster size (target size=%u, source size=%u
#define DALIERR_FailedToCreateBackupFile 5970 // Failed to create backup file
#define DALIERR_SIncorrectNumberOfParameters 5971 // %s: incorrect number of parameters
#define DALIERR_DfspermRequiresUsernameToBeSetUser 5972 // dfsperm requires username to be set (user=)
#define DALIERR_UnknownTypeSMustBeFileOr 5973 // unknown type [ %s ], must be 'file' or 'path'
#define DALIERR_RemotetestForNonRemoteFilesNeedsDali 5974 // remotetest for non-remote files needs Dali.
#define DALIERR_FailedToConnectToPathS 5975 // Failed to connect to path %s
#define DALIERR_TestOutOfRangeThereAreOnly 5976 // Test out of range, there are only %d tests
#define DALIERR_NoSourceLogicalFilenameSupplied 5977 // no source logical filename supplied
#define DALIERR_FileSHasNoRecordDefinition 5978 // File '%s' has no record definition
#define DALIERR_LocalAndSdsOutputsMismatch 5979 // Local and SDS outputs mismatch

#endif
