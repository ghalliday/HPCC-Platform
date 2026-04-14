#ifndef DFUERR_HPP
#define DFUERR_HPP

#include "jexcept.hpp"
#include "errorlist.h"

/* Errors generated in dali/dfu */

#define DFUERR_SetfilerepeatoptionsClusterSNotFound 7601 // setFileRepeatOptions cluster %s not found
#define DFUERR_EmptyDefaultDirectory 7602 // Empty default directory.
#define DFUERR_DropzoneSNotFound 7603 // DropZone %s not found.
#define DFUERR_InvalidDropzoneDirectoryS 7604 // Invalid DropZone directory %s.
#define DFUERR_EmptyPlaneName 7605 // Empty plane name.
#define DFUERR_UnexpectedEmptyPlaneName 7606 // Unexpected empty plane name.
#define DFUERR_MonitorMonitorFileIncorrectlySpecified 7607 // MONITOR: monitor file incorrectly specified
#define DFUERR_MonitorSIsNotADirectoryIn 7608 // MONITOR: %s is not a directory in DFU WUID %s
#define DFUERR_DestinationCannotBeForeignFile 7609 // Destination cannot be foreign file
#define DFUERR_SourceFileSCouldNotBeFound 7610 // Source file %s could not be found in Dali %s
#define DFUERR_DestinationFileSAlreadyExists 7611 // Destination file %s already exists
#define DFUERR_FileSCouldNotBeCopiedSee 7612 // File %s could not be copied - see %s
#define DFUERR_SuperfileSCouldNotBeCreated 7613 // SuperFile %s could not be created
#define DFUERR_SourceFileNotSpecified 7614 // Source file not specified
#define DFUERR_DestinationNotSpecified 7615 // Destination not specified
#define DFUERR_CannotAddToMultiClusterFileUsing 7616 // Cannot add to multi-cluster file using keypatch
#define DFUERR_DestinationNumpartsoverrideIsProvidedButS 7617 // Destination NumPartsOverride is provided but %s
#define DFUERR_CannotCreateMultiClusterFileInForeign 7618 // Cannot create multi cluster file in foreign file
#define DFUERR_IncompatibleFileForMulticlusterAddS 7619 // Incompatible file for multicluster add - %s
#define DFUERR_DestinationForMergeSDoesNotExist 7620 // Destination for merge %s does not exist
#define DFUERR_IncompatibleFileForMulticlusterMergeS 7621 // Incompatible file for multicluster merge - %s
#define DFUERR_S 7622 // %s
#define DFUERR_DestinationFileSAlreadyExistsAndOverwrite 7623 // Destination file %s already exists and overwrite not specified
#define DFUERR_InternalErrorInAttemptToRemoveFile 7624 // Internal error in attempt to remove file %s
#define DFUERR_DestinationFileSCouldNotBeCreated 7625 // Destination file %s could not be created
#define DFUERR_OldKeyFileSCouldNotBe 7626 // Old key file %s could not be found in source
#define DFUERR_NoClusterSpecifiedForDestination 7627 // No cluster specified for destination
#define DFUERR_DestinationClusterSNotFound 7628 // Destination cluster %s not found
#define DFUERR_NoTargetNameSpecifiedForRemove 7629 // No target name specified for remove
#define DFUERR_TargetSAlreadyExists 7630 // Target %s already exists
#define DFUERR_NoTargetNameSpecifiedForRename 7631 // No target name specified for rename
#define DFUERR_ImportClusterSIsNotRecognizedLocally 7632 // IMPORT cluster %s is not recognized locally
#define DFUERR_DfurunUnsupportedCommandD 7633 // DFURUN: Unsupported command (%d)
#define DFUERR_CoundNotCreateQueue 7634 // Cound not create queue
#define DFUERR_NoSshUserConfigured 7635 // No SSH user configured
#define DFUERR_KeydiffOldAndNewFilesDoNot 7636 // KeyDiff - old and new files do not have the same size
#define DFUERR_KeypatchOldAndPatchFilesDoNot 7637 // KeyPatch - old and patch files do not have the same size
#define DFUERR_SIsADirectory 7638 // %s is a directory
#define DFUERR_CopylogicalfilePartDMissingAnyCopies 7639 // copyLogicalFile: part %d missing any copies
#define DFUERR_ReplicatelogicalfileSPartDMissingAnyCopies 7640 // replicateLogicalFile: %s part %d missing any copies
#define DFUERR_ClonesubfileLogicalNameSInvalid 7641 // cloneSubFile: Logical name %s invalid
#define DFUERR_LogicalNameSInvalid 7642 // Logical name %s invalid
#define DFUERR_CannotFindClusterS 7643 // Cannot find cluster %s
#define DFUERR_CannotCloneSToItself 7644 // Cannot clone %s to itself
#define DFUERR_SourceFileSInDaliSIs 7645 // Source file %s in Dali %s is not a file or superfile
#define DFUERR_AttributesForSourceFileSCouldNot 7646 // Attributes for source file %s could not be found in Dali %s
#define DFUERR_UnrecognisedFileXmlRootTagDetectedS 7647 // Unrecognised file XML root tag detected: '%s'
#define DFUERR_FileSCouldNotBeCopied 7648 // File %s could not be copied
#define DFUERR_ClonefilerelationshipsSrcAndDestinationArraysNotSame 7649 // cloneFileRelationships - src and destination arrays not same size
#define DFUERR_DfuwuAttemptToResubmitPublisherWorkunitWith 7650 // DFUWU: Attempt to resubmit publisher workunit with non publisher wuid %s
#define DFUERR_DfuwuCouldNotConnectToPublisherWorkunit 7651 // DFUWU: Could not connect to publisher workunit %s
#define DFUERR_DfuwuCouldNotOpenProgressSectionOf 7652 // DFUWU: Could not open progress section of publisher workunit %s
#define DFUERR_DfuwuCanOnlyResubmitPublisherWorkunitsThat 7653 // DFUWU: Can only resubmit publisher workunits that are in a failed, unknown, or aborted state %s
#define DFUERR_CdfufilespecClusterSNotFound 7654 // CDFUfileSpec: Cluster %s not found
#define DFUERR_CdfufilespecNoPartsFoundForFile 7655 // CDFUfileSpec: No parts found for file!
#define DFUERR_CdfufilespecGetfiledescriptorCouldNotFindGroupFor 7656 // CDFUfileSpec: getFileDescriptor: Could not find group for cluster %d
#define DFUERR_DfuwuLogicalNameSInvalid2 7657 // DFUWU: Logical name %s invalid(2)
#define DFUERR_DfuwuLogicalNameSInvalid 7658 // DFUWU: Logical name %s invalid
#define DFUERR_DfuwuLogicalGroupSNotFound 7659 // DFUWU: Logical group %s not found
#define DFUERR_DfuwuCannotConstructPartFileName 7660 // DFUWU: cannot construct part file name
#define DFUERR_DfuwuCannotDetermineEndpointForPartFile 7661 // DFUWU: cannot determine endpoint for part file
#define DFUERR_DfuwuCannotDetermineFilePartDirectoryFor 7662 // DFUWU: cannot determine file part directory for %s
#define DFUERR_DfuWorkunitIsProtected 7663 // DFU Workunit is protected
#define DFUERR_DfuWorkunitIsActive 7664 // DFU Workunit is active
#define DFUERR_DfuwuSCouldNotBeFound 7665 // DFUWU %s could not be found
#define DFUERR_DfuNoQueueNameSpecified 7666 // DFU no queue name specified
#define DFUERR_DfuWorkunitSCouldNotBeOpened 7667 // DFU workunit %s could not be opened for update
#define DFUERR_ServerUrlNotSpecified 7668 // Server url not specified
#define DFUERR_ActionIsMissing 7669 // action is missing
#define DFUERR_UnknownDfuplusAction 7670 // Unknown dfuplus action
#define DFUERR_RecordsizeNotSpecifiedForFixed 7671 // recordsize not specified for fixed
#define DFUERR_JsonFormatOnlyAcceptsUtfEncodings 7672 // json format only accepts utf encodings
#define DFUERR_YouCanTUseRowtagOptionWith 7673 // You can't use rowtag option with json format
#define DFUERR_XmlFormatOnlyAcceptsUtfEncodings 7674 // xml format only accepts utf encodings
#define DFUERR_RowtagNotSpecified 7675 // rowtag not specified.
#define DFUERR_YouCanTUseRowpathOptionWith 7676 // You can't use rowpath option with xml format
#define DFUERR_SrcfileNotSpecified 7677 // srcfile not specified
#define DFUERR_NeitherSrcipNorSrcplaneSpecified 7678 // Neither srcip nor srcplane specified
#define DFUERR_SrcipSrcfileSrcplaneAndSrcxmlCanT 7679 // srcip/srcfile/srcplane and srcxml can't be used at the same time
#define DFUERR_DstnameNotSpecified 7680 // dstname not specified
#define DFUERR_DstclusterNotSpecified 7681 // dstcluster not specified
#define DFUERR_FormatSNotSupported 7682 // format %s not supported
#define DFUERR_UnknownErrorSpraying 7683 // unknown error spraying
#define DFUERR_SrcnameNotSpecified 7684 // srcname not specified
#define DFUERR_DstipNotSpecified 7685 // dstip not specified
#define DFUERR_DstipDstfileDstplaneAndDstxmlCanT 7686 // dstip/dstfile/dstplane and dstxml can't be used at the same time
#define DFUERR_SrcdaliNotSpecified 7687 // srcdali not specified
#define DFUERR_NeitherLfnNorFilenameSpecified 7688 // neither lfn nor filename specified
#define DFUERR_FileNameNotSpecified 7689 // file name not specified
#define DFUERR_SrcnamesAndDstnamesMustHaveTheSame 7690 // srcnames and dstnames must have the same number of items (%d vs %d)
#define DFUERR_NoValidNamesFoundInSrcnamesDstnames 7691 // No valid names found in srcnames/dstnames
#define DFUERR_InvalidRenameUsageUseN 7692 // Invalid rename usage. Use:\n
#define DFUERR_SuperfileNameIsNotSpecified 7693 // superfile name is not specified
#define DFUERR_CanTOpenFileSN 7694 // can't open file %s\n
#define DFUERR_CanTWriteToFileSN 7695 // can't write to file %s\n
#define DFUERR_TruncatedWriteToFileSN 7696 // truncated write to file %s\n
#define DFUERR_PleaseSpecifySrcxmlForAddingFromXml 7697 // Please specify srcxml for adding from xml, or srcname for adding from remote dali
#define DFUERR_SrcdaliNotSpecifiedForAddingRemote 7698 // srcdali not specified for adding remote
#define DFUERR_WuidNotSpecified 7699 // wuid not specified
#define DFUERR_DstxmlNotSpecified 7700 // dstxml not specified
#define DFUERR_CanTOpenFileSEraseHistory 7701 // can't open file %s, erase history cancelled.\n
#define DFUERR_CanTWriteToFileSErase 7702 // can't write to file %s, erase history cancelled.\n
#define DFUERR_TruncatedWriteToFileSEraseHistory 7703 // truncated write to file %s, erase history cancelled.\n
#define DFUERR_JobnameNotSpecified 7704 // jobname not specified
#define DFUERR_Aborted 7705 // Aborted
#define DFUERR_ErrStr 7706 // err.str(
#define DFUERR_CouldNotConnectToFiles 7707 // Could not connect to Files
#define DFUERR_FailedToReadSFromFile 7708 // Failed to read %s from file
#define DFUERR_FileExceedsMaximumSizeToleranceUMb 7709 // File exceeds maximum size tolerance (%u MB). File size = %u MB
#define DFUERR_NoXrefDaliTreeAvailable 7710 // No XRef Dali Tree available

#endif
