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

#ifndef __THORMETA_HPP_
#define __THORMETA_HPP_

#ifdef THORHELPER_EXPORTS
 #define THORHELPER_API DECL_EXPORT
#else
 #define THORHELPER_API DECL_IMPORT
#endif

#include "jrowstream.hpp"
#include "rtlkey.hpp"
#include <vector>
#include "thorstore.hpp"


interface IDistributedFile;

//--------------------------------------------------------------------------------------------------------------------

struct CPartition
{
    offset_t    id;
    offset_t    offset;
};

class CPartitionTable : public CInterface
{
public:
    virtual CPartition queryPartition(unsigned numSplits, unsigned split) = 0;
};

class CNoPartition : public CPartitionTable
{
public:
    virtual CPartition queryPartition(unsigned numSplits, unsigned split) override
    {
        if (split == 0)
            return CPartition{0, 0};
        else
            return CPartition{numRows, size};
    }
private:
    offset_t numRows = 0;
    offset_t size = 0;
};

class CPartitionByOffset : public CPartitionTable
{
public:
    virtual CPartition queryPartition(unsigned numSplits, unsigned split) override;
private:
    offset_t recordStep = 0;
    std::vector<offset_t> offsets; // offsets of record[n * recordStep] is offsets[n];
};

class CPartitionByRecord : public CPartitionTable
{
public:
    virtual CPartition queryPartition(unsigned numSplits, unsigned split) override;
private:
    offset_t offsetStep;
    std::vector<offset_t> recordNums; // the record at offset [n * offsetStep] is recordNums[n];
};

// Could have partition point that
class CLogicalFilePart
{
public:
    CLogicalFilePart() = default;
    CLogicalFilePart(offset_t _numRows, offset_t _fileSize, offset_t _baseRow, offset_t _baseOffset)
    : numRows(_numRows), fileSize(_fileSize), baseRow(_baseRow), baseOffset(_baseOffset)
    {
    }

public://should be private
    offset_t numRows = 0;
    offset_t fileSize = 0;
    offset_t baseRow = 0; // sum of previous numRows
    offset_t baseOffset = 0; // sum of previous file sizes
    Owned<CPartitionTable> partition;
};

class THORHELPER_API CLogicalFile : public CInterface
{
public:
    CLogicalFile(IDistributedFile * file, IOutputMetaData * expectedMeta, IPropertyTree * inputOptions, IPropertyTree * formatOptions, offset_t previousSize, CStorageSystems & storageSystems);
    CLogicalFile(const char * path, bool isLogicalName, CStorageLocation * location, IOutputMetaData * expectedMeta, IPropertyTree * inputOptions, IPropertyTree * formatOptions, offset_t previousSize);
    CLogicalFile(MemoryBuffer & in);

    void getPhysicalFile(unsigned part, unsigned copy) const;
    //IFile * createFile(unsigned part, unsigned copy) const;

    StringBuffer & getURL(StringBuffer & target, unsigned part, unsigned copy) const;
    unsigned getNumParts() const { return numParts; }
    offset_t getPartSize(unsigned part) const;

    inline bool isDistributed() const { return numParts > 1; }  // MORE: Only if originally a logical file...
    inline bool isLogicalFile() const { return name != nullptr; }
    bool isLocal(unsigned part, unsigned copy) const;
    bool onAttachedStorage(unsigned copy) const;

    unsigned queryActualCrc() const { return actualCrc; }
    IOutputMetaData * queryActualMeta() const;
    offset_t queryExpandedFileSize() const { return fileSize; }
    const char * queryFormat();
    IPropertyTree * queryFormatOptions() const { return formatOptions; }
    IPropertyTree * queryInputOptions() const { return inputOptions; }
    const char * queryLogicalFilename() const;
    unsigned queryNumCopies() const; // includes backup locations and alternative locations
    offset_t queryOffsetOfPart(unsigned part) const { return fileBaseOffset + queryPart(part).baseOffset; }
    const CLogicalFilePart & queryPart(unsigned part) const { return parts[part]; }
    const char * queryTracingFilename(unsigned part) const;
    void serialize(MemoryBuffer & out) const;

protected:
    StringBuffer & expandLogicalAsPhysical(StringBuffer & target, unsigned copy) const;
    StringBuffer & expandPath(StringBuffer & target, unsigned part, unsigned copy) const;

private:
    unsigned numParts = 0;
    std::vector<CLogicalFilePart> parts;
    Owned<CPartitionTable> partitions;    //Do partitions live here, or in the file part?  What are the requirements for HDFS?
    //only one of the following should be filled in
    StringAttr name; // logical file name
    StringAttr path; // local path to the resource
    StringAttr format;  // combines type and format - a key is represented with a format of "key"
    Owned<IOutputMetaData> actualMeta;
    unsigned actualCrc = 0;
    bool addPartSuffix = false;
    offset_t fileSize = 0;
    offset_t fileBaseOffset = 0;
    CIArrayOf<CStorageLocation> locations; // An array of locations the file is stored.  I think it simplifies everything for replicas to be expanded out.
    Linked<IPropertyTree> formatOptions; // from CLogicalFileCollection
    Linked<IPropertyTree> inputOptions;        // from helper and logical properties
    StringAttr accessToken; // ?? not sure how this would work.
};

class THORHELPER_API CLogicalFileSlice
{
    friend class CLogicalFileCollection;
public:
    CLogicalFileSlice(CLogicalFile * _file, unsigned _part, offset_t _startOffset = 0, offset_t _length = unknownFileSize);

    StringBuffer & getURL(StringBuffer & url, unsigned copy) const { return file->getURL(url, part, copy); }

    bool isEmpty() const { return !file || length == 0; }
    bool isLogicalFile() const { return file->isLogicalFile(); }
    bool isRemoteReadCandidate(unsigned copy) const;
    bool isWholeFile() const;
    bool isLocal(unsigned copy) const { return file->isLocal(part, copy); }
    bool onAttachedStorage(unsigned copy) const { return file->onAttachedStorage(copy); }

    CLogicalFile * queryFile() const { return file; }
    const char * queryFormat() { return file->queryFormat(); }
    IPropertyTree * queryFormatOptions() const { return file->queryFormatOptions(); }
    IPropertyTree * queryInputOptions() const { return file->queryInputOptions(); }
    offset_t queryLength() const { return length; }
    unsigned queryNumCopies() const { return file->queryNumCopies(); }
    unsigned queryPartNumber() const { return part; }
    offset_t queryOffsetOfPart() const { return file->queryOffsetOfPart(part); }
    offset_t queryStartOffset() const { return startOffset; }
    const char * queryLogicalFilename() const { return file->queryLogicalFilename(); }
    const char * queryTracingFilename() const { return file->queryTracingFilename(part); }

    void setAccessed() {}  // MORE:

private:
    CLogicalFileSlice() = default;

private:
    CLogicalFile * file = nullptr;
    unsigned part = 0;
    offset_t startOffset = 0;
    offset_t length = unknownFileSize;
    //unsigned location = 0; //MORE: Not sure when this is resolved - early or late?
    //MORE: What about HDFS records that are split over multiple files??
};

//The following class is always used to access a collection of files - even if it is only a single physical file.
class CDfsLogicalFileName;
class THORHELPER_API CLogicalFileCollection
{
public:
    CLogicalFileCollection(CStorageSystems & _storageSystems, const char * _wuid) : storageSystems(_storageSystems), wuid(_wuid) { }
    CLogicalFileCollection(MemoryBuffer & in);

    void calcPartition(std::vector<CLogicalFileSlice> & slices, unsigned numChannels, unsigned channel, bool preserveDistribution);
    void serialize(MemoryBuffer & out) const;
    void setEclFilename(const char * filename, bool isTemporary,  bool isCodeSigned, IUserDescriptor *user, IOutputMetaData * expectedMeta, IPropertyTree * inputOptions, IPropertyTree * formatOptions);

protected:
    void appendFile(CLogicalFile & file);
    void processFile(IDistributedFile * file, IOutputMetaData * expectedMeta, IPropertyTree * inputOptions, IPropertyTree * formatOptions);
    void processFilename(CDfsLogicalFileName & logicalFilename, IUserDescriptor *user, bool isTemporary, IOutputMetaData * expectedMeta, IPropertyTree * inputOptions, IPropertyTree * formatOptions);
    void processMissing(const char * filename, IPropertyTree * inputOptions);
    void processPhysicalFilename(const char * path, IOutputMetaData * expectedMeta, IPropertyTree * inputOptions, IPropertyTree * formatOptions);
    void processProtocolFilename(const char * name, const char * colon, const char * slash, IOutputMetaData * expectedMeta, IPropertyTree * inputOptions, IPropertyTree * formatOptions);
    void reset();

private:
    CStorageSystems & storageSystems;
    CIArrayOf<CLogicalFile> files;
    Owned<IPropertyTree> eclOptions;      // explicitly defined in the ecl helpers.
    Owned<IPropertyTree> formatOptions;   // defined by the format properties in ecl
    StringAttr wuid;
    offset_t totalSize = 0;
};

#endif
