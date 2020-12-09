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

#if 0
//SplitPoints are not currently supported the following classes sketch out what they would look like
struct CSplitPoint
{
    offset_t    id;
    offset_t    offset;
};

class CSplitPointTable : public CInterface
{
public:
    virtual CSplitPoint querySplitPoint(unsigned numSplits, unsigned split) = 0;
};

class CNoSplitPoint : public CSplitPointTable
{
public:
    virtual CSplitPoint querySplitPoint(unsigned numSplits, unsigned split) override
    {
        if (split == 0)
            return CSplitPoint{0, 0};
        else
            return CSplitPoint{numRows, size};
    }
private:
    offset_t numRows = 0;
    offset_t size = 0;
};

class CSplitPointByOffset : public CSplitPointTable
{
public:
    virtual CSplitPoint querySplitPoint(unsigned numSplits, unsigned split) override;
private:
    offset_t recordStep = 0;
    std::vector<offset_t> offsets; // offsets of record[n * recordStep] is offsets[n];
};

class CSplitPointByRecord : public CSplitPointTable
{
public:
    virtual CSplitPoint querySplitPoint(unsigned numSplits, unsigned split) override;
private:
    offset_t offsetStep;
    std::vector<offset_t> recordNums; // the record at offset [n * offsetStep] is recordNums[n];
};
#endif


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
//  Owned<CSplitPointTable> splits;   // This is where split points would be saved and accessed
};

class THORHELPER_API CLogicalFile : public CInterface
{
public:
    CLogicalFile(const CStorageSystems & storage, const IPropertyTree * xml, offset_t previousSize, IOutputMetaData * _expectedMeta);
    CLogicalFile(MemoryBuffer & in);

    void getPhysicalFile(unsigned part, unsigned copy) const;
    //IFile * createFile(unsigned part, unsigned copy) const;

    StringBuffer & getURL(StringBuffer & target, unsigned part, unsigned copy) const;
    offset_t getFileSize() const { return fileSize; }
    unsigned getNumCopies() const;
    unsigned getNumParts() const { return numParts; }
    offset_t getPartSize(unsigned part) const;
    bool isDistributed() const { return numParts > 1; }  // MORE: Only if originally a logical file...
    bool isGrouped() const { return xml->getPropBool("@grouped"); }
    bool isLogicalFile() const { return name != nullptr; }
    bool isLocal(unsigned part, unsigned copy) const;
    bool onAttachedStorage(unsigned copy) const;
    const CStoragePlane * queryPlane(unsigned idx) const { return planes.item(idx); }

    unsigned queryActualCrc() const { return actualCrc; }
    IOutputMetaData * queryActualMeta() const;
    const char * queryFormat();
    const IPropertyTree * queryFormatOptions() const;
    const IPropertyTree * queryInputOptions() const;
    const char * queryLogicalFilename() const;
    offset_t queryOffsetOfPart(unsigned part) const;
    const CLogicalFilePart & queryPart(unsigned part) const { return parts[part]; }
    const char * queryTracingFilename(unsigned part) const;
    void serialize(MemoryBuffer & out) const;

    const char * queryPhysicalPath() const { UNIMPLEMENTED; }    // MORE!!!
    bool includePartSuffix() const { return true; }

protected:
    StringBuffer & expandLogicalAsPhysical(StringBuffer & target, unsigned copy) const;
    StringBuffer & expandPath(StringBuffer & target, unsigned part, unsigned copy) const;

private:
    const IPropertyTree * xml = nullptr;
    offset_t fileBaseOffset = 0;                // calculated from size of previous files
    IOutputMetaData * expectedMeta;             // same as CLogicalFileCollection::expectedMeta
    //All of the following are derived from the xml
    const char * name = nullptr;
    unsigned numParts = 0;
    offset_t fileSize = 0;
    std::vector<CLogicalFilePart> parts;
    ConstPointerArrayOf<CStoragePlane> planes; // An array of locations the file is stored.  I think it simplifies everything for replicas to be expanded out.
    mutable Owned<IOutputMetaData> actualMeta;
    mutable unsigned actualCrc = 0;
};

class THORHELPER_API CLogicalFileSlice
{
    friend class CLogicalFileCollection;
public:
    CLogicalFileSlice(CLogicalFile * _file, unsigned _part, offset_t _startOffset = 0, offset_t _length = unknownFileSize);

    StringBuffer & getURL(StringBuffer & url, unsigned copy) const { return file->getURL(url, part, copy); }

    unsigned getNumCopies() const { return file->getNumCopies(); }
    bool isEmpty() const { return !file || length == 0; }
    bool isLogicalFile() const { return file->isLogicalFile(); }
    bool isRemoteReadCandidate(unsigned copy) const;
    bool isWholeFile() const;
    bool isLocal(unsigned copy) const { return file->isLocal(part, copy); }
    bool onAttachedStorage(unsigned copy) const { return file->onAttachedStorage(copy); }

    CLogicalFile * queryFile() const { return file; }
    const char * queryFormat() { return file->queryFormat(); }
    const IPropertyTree * queryFormatOptions() const { return file->queryFormatOptions(); }
    const IPropertyTree * queryInputOptions() const { return file->queryInputOptions(); }
    offset_t queryLength() const { return length; }
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
    CLogicalFileCollection() = default;
    CLogicalFileCollection(MemoryBuffer & in);

    void init(const char * _wuid,  bool _isTemporary,  bool _isCodeSigned, IUserDescriptor * _user, IOutputMetaData * _expectedMeta); // called once
    void calcPartition(std::vector<CLogicalFileSlice> & slices, unsigned numChannels, unsigned channel, bool preserveDistribution);
    void serialize(MemoryBuffer & out) const;
    void setEclFilename(const char * filename, IPropertyTree * inputOptions, IPropertyTree * formatOptions);

protected:
    void appendFile(CLogicalFile & file);
    void processFile(IDistributedFile * file, IOutputMetaData * expectedMeta, IPropertyTree * inputOptions, IPropertyTree * formatOptions);
    void processFilename(CDfsLogicalFileName & logicalFilename, IUserDescriptor *user, bool isTemporary, IOutputMetaData * expectedMeta, IPropertyTree * inputOptions, IPropertyTree * formatOptions);
    void processMissing(const char * filename, IPropertyTree * inputOptions);
    void processPhysicalFilename(const char * path, IOutputMetaData * expectedMeta, IPropertyTree * inputOptions, IPropertyTree * formatOptions);
    void processProtocolFilename(const char * name, const char * colon, const char * slash, IOutputMetaData * expectedMeta, IPropertyTree * inputOptions, IPropertyTree * formatOptions);
    void reset();

private:
    //Options that are constant for the lifetime of the class
    StringAttr wuid;
    IUserDescriptor * user = nullptr;
    IOutputMetaData * expectedMeta = nullptr;
    bool isTemporary = false;
    bool isCodeSigned = false;
    //The following may be reset e.g. if used within a child query
    StringAttr filename;
    Owned<IPropertyTree> inputOptions;    // defined by the helper functions
    Owned<IPropertyTree> formatOptions;   // defined by the format properties in ecl
    //derived information
    Owned<IPropertyTree> resolved;
    CStorageSystems storageSystems;
    CIArrayOf<CLogicalFile> files;
    offset_t totalSize = 0;
};

#endif
