import re

with open('system/jlib/jfile.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

replacements = [
    (r'MakeStringException\(-1, "copyFile: target path ''%s'' could not be created"', r'MakeStringException(JLIBERR_FileCopyTargetCreateFailed, "copyFile: target path \'%s\' could not be created"'),
    (r'MakeStringException\(-1, "copySection: source ''%s'' not found"', r'MakeStringException(JLIBERR_FileCopySourceNotFound, "copySection: source \'%s\' not found"'),
    (r'makeStringException\(-1, "CDiscretionaryFileLock - invalid parameter"\)', r'makeStringException(JLIBERR_FileLockInvalidParameter, "CDiscretionaryFileLock - invalid parameter")'),
    (r'MakeStringException\(-1,"Remote file cannot be memory mapped"\)', r'MakeStringException(JLIBERR_MemoryMapRemoteFileError, "Remote file cannot be memory mapped")'),
    (r'MakeStringException\(-1, "CMemoryBufferIO: read beyond end of buffer', r'MakeStringException(JLIBERR_MemoryBufferReadBeyondEnd, "CMemoryBufferIO: read beyond end of buffer'),
    (r'MakeStringException\(-1, "CMemoryBufferIO: UNIMPLEMENTED, writing beyond buffer', r'MakeStringException(JLIBERR_MemoryBufferWriteBeyondEndUnimplemented, "CMemoryBufferIO: UNIMPLEMENTED, writing beyond buffer'),
    (r'MakeStringException\(-1, "CMemoryBufferIO: UNIMPLEMENTED, setting size', r'MakeStringException(JLIBERR_MemoryBufferSetSizeBeyondEndUnimplemented, "CMemoryBufferIO: UNIMPLEMENTED, setting size'),
    (r'MakeStringException\(-1, "CSequentialFileIO %s out of sequence', r'MakeStringException(JLIBERR_SequentialFileOutOfSequence, "CSequentialFileIO %s out of sequence'),
    (r'makeStringExceptionV\(0, "Seek on non-seekable CFileIOStream', r'makeStringExceptionV(JLIBERR_FileStreamSeekNotSeekable, "Seek on non-seekable CFileIOStream'),
    (r'MakeStringException\(-1, "renameFile: source ''%s'' not found', r'MakeStringException(JLIBERR_FileRenameSourceNotFound, "renameFile: source \'%s\' not found'),
    (r'MakeStringException\(-1, "renameFile: source ''%s'' is not a valid file', r'MakeStringException(JLIBERR_FileRenameSourceInvalid, "renameFile: source \'%s\' is not a valid file'),
    (r'MakeStringException\(-1, "renameFile: source ''%s'' is readonly', r'MakeStringException(JLIBERR_FileRenameSourceReadOnly, "renameFile: source \'%s\' is readonly'),
    (r'MakeStringException\(-1, "renameFile: target path ''%s'' could not be created', r'MakeStringException(JLIBERR_FileRenameTargetCreateFailed, "renameFile: target path \'%s\' could not be created'),
    (r'MakeStringException\(-1, "renameFile: target file already exists: ''%s'' will not overwrite', r'MakeStringException(JLIBERR_FileRenameTargetExists, "renameFile: target file already exists: \'%s\' will not overwrite'),
    (r'MakeStringException\(-1, "copyFile: source ''%s'' not found"', r'MakeStringException(JLIBERR_FileCopySourceNotFound, "copyFile: source \'%s\' not found"'),
    (r'MakeStringException\(-1, "CreateIFile cannot resolve %s', r'MakeStringException(JLIBERR_CreateIFileCannotResolve, "CreateIFile cannot resolve %s'),
    (r'MakeStringException\(-1, "createIFile: cannot attach to %s', r'MakeStringException(JLIBERR_CreateIFileCannotAttach, "createIFile: cannot attach to %s'),
    (r'makeStringExceptionV\(0, "touchFile: failed to create file %s', r'makeStringExceptionV(JLIBERR_TouchFileFailedToCreate, "touchFile: failed to create file %s'),
    (r'MakeStringException\(-1, "createHardLink:: %s', r'MakeStringException(JLIBERR_CreateHardLinkFailed, "createHardLink:: %s'),
    (r'MakeStringException\(-1, "Failed to create log alias %s for %s: error code = %d', r'MakeStringException(JLIBERR_CreateLogAliasFailed, "Failed to create log alias %s for %s: error code = %d'),
    (r'MakeStringException\(-1, "Badly formatted file entry %s', r'MakeStringException(JLIBERR_BadlyFormattedFileEntry, "Badly formatted file entry %s'),
    (r'MakeStringException\(-1, "Component file IP does not match: %s', r'MakeStringException(JLIBERR_ComponentFileIPMismatch, "Component file IP does not match: %s'),
    (r'makeStringExceptionV\(-1, "makeAbsolutePath: could not resolve absolute path for %s', r'makeStringExceptionV(JLIBERR_MakeAbsolutePathResolveFailed, "makeAbsolutePath: could not resolve absolute path for %s'),
    (r'MakeStringException\(-1, "extractBlobElements: file ''%s'' not found', r'MakeStringException(JLIBERR_ExtractBlobElementsFileNotFound, "extractBlobElements: file \'%s\' not found'),
    (r'MakeStringException\(0, "%s: Failed to open part file at any of the following locations:', r'MakeStringException(JLIBERR_FailedToOpenPartFileAnyLocation, "%s: Failed to open part file at any of the following locations:'),
    (r'makeStringExceptionV\(-1, "InputStream::get read past end of stream', r'makeStringExceptionV(JLIBERR_InputStreamReadPastEnd, "InputStream::get read past end of stream'),
    (r'MakeStringException\(-1,"CMemoryMappedSerialStream file too big to be mapped', r'MakeStringException(JLIBERR_StreamFileTooBigToMap, "CMemoryMappedSerialStream file too big to be mapped'),
    (r'MakeStringException\(-1,"CMemoryMappedSerialStream::get read past end of stream', r'MakeStringException(JLIBERR_MappedStreamReadPastEnd, "CMemoryMappedSerialStream::get read past end of stream'),
    (r'MakeStringException\(-1,"CMemoryMappedSerialStream::skip read past end of stream', r'MakeStringException(JLIBERR_MappedStreamSkipPastEnd, "CMemoryMappedSerialStream::skip read past end of stream'),
    (r'MakeStringException\(-1,"CMemoryBufferSerialStream::get read past end of stream', r'MakeStringException(JLIBERR_BufferStreamReadPastEnd, "CMemoryBufferSerialStream::get read past end of stream'),
    (r'MakeStringException\(-1,"CMemoryBufferSerialStream::skip read past end of stream', r'MakeStringException(JLIBERR_BufferStreamSkipPastEnd, "CMemoryBufferSerialStream::skip read past end of stream'),
    (r'MakeStringException\(-1,"CMemoryMappedFile::nextPtr - outside map', r'MakeStringException(JLIBERR_MemoryMappedFileOutsideMap, "CMemoryMappedFile::nextPtr - outside map'),
    (r'MakeStringException\(-1,"CMemoryMappedFile::reinit file too big', r'MakeStringException(JLIBERR_MemoryMappedFileReinitTooBig, "CMemoryMappedFile::reinit file too big'),
    (r'MakeStringException\(-1, "CCachedFileIO\(%s\) %s not supported', r'MakeStringException(JLIBERR_CachedFileIONotSupported, "CCachedFileIO(%s) %s not supported'),
    (r'MakeStringException\(-1, "CCachedFileIO::open', r'MakeStringException(JLIBERR_CachedFileIOOpenFailed, "CCachedFileIO::open'),
    (r'MakeStringException\(-1, "Invalid IFile input in getSortedDirectoryIterator', r'MakeStringException(JLIBERR_InvalidIFileInputSortedDirIterator, "Invalid IFile input in getSortedDirectoryIterator'),
    (r'MakeStringException\(-1, "Invalid dirName input in getSortedDirectoryIterator', r'MakeStringException(JLIBERR_InvalidDirNameInputSortedDirIterator, "Invalid dirName input in getSortedDirectoryIterator'),
    (r'makeStringException\(-1, "CFileEventWatcher::add\(\) - invalid empty filename', r'makeStringException(JLIBERR_FileEventWatcherAddInvalidEmpty, "CFileEventWatcher::add() - invalid empty filename'),
    (r'makeStringExceptionV\(99, "Reentrant call to CBlockedFileIO::read', r'makeStringExceptionV(JLIBERR_ReentrantCallCBlockedFileIORead, "Reentrant call to CBlockedFileIO::read')
]

for s, r in replacements:
    content = re.sub(s, r, content)

with open('system/jlib/jfile.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
