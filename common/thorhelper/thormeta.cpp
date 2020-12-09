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

#include "jliball.hpp"
#include "jsocket.hpp"
#include "thorfile.hpp"

#include "eclhelper.hpp"
#include "eclrtl.hpp"
#include "eclrtl_imp.hpp"

#include "dautils.hpp"
#include "dadfs.hpp"
#include "dameta.hpp"

#include "thormeta.hpp"
#include "rtlcommon.hpp"
#include "thorcommon.hpp"

//Should be common with agentctx.hpp
#define WRN_SkipMissingOptFile              5401

//Default format - if not specified in ecl and not known to dali etc.
static constexpr const char * defaultFileFormat = "flat";

static void ensureCloned(Shared<IPropertyTree> option, IPropertyTree * original)
{
    if (option == original)
        option.setown(createPTreeFromIPT(original));
}

IPropertyTree * resolveLogicalFilename(const char * filename, IUserDescriptor * user, ResolveOptions options)
{
    //This may go via esp instead at some point....
    return resolveLogicalFilenameFromDali(filename, user, options);
}

//---------------------------------------------------------------------------------------------------------------------

//Cloned for now -
static void queryInheritProp(IPropertyTree & target, const char * targetName, IPropertyTree & source, const char * sourceName)
{
    if (source.hasProp(sourceName) && !target.hasProp(targetName))
        target.setProp(targetName, source.queryProp(sourceName));
}

static void queryInheritSeparatorProp(IPropertyTree & target, const char * targetName, IPropertyTree & source, const char * sourceName)
{
    //Legacy - commas are quoted if they occur in a separator list, so need to remove the leading backslashes
    if (source.hasProp(sourceName) && !target.hasProp(targetName))
    {
        StringBuffer unquoted;
        const char * text = source.queryProp(sourceName);
        while (*text)
        {
            if ((text[0] == '\\') && (text[1] == ','))
                text++;
            unquoted.append(*text++);
        }
        target.setProp(targetName, unquoted);
    }
}


CLogicalFile::CLogicalFile(const CStorageSystems & storage, const IPropertyTree * _xml, offset_t previousSize, IOutputMetaData * _expectedMeta)
: xml(_xml), expectedMeta(_expectedMeta)
{
    name = xml->queryProp("@name");
    numParts = xml->getPropInt("@numParts");
    fileBaseOffset = previousSize;
    fileSize = xml->getPropInt64("@size");

    Owned<IPropertyTreeIterator> partIter = xml->getElements("parts");
    bool hasXml = partIter->first();
    offset_t baseOffset = 0;
    offset_t baseRow = 0;
    for (unsigned part=0; part < numParts; part++)
    {
        offset_t partSize = hasXml ? partIter->query().getPropInt("@size") : 0;
        offset_t numRows = hasXml ? partIter->query().getPropInt("@numRows") : 0;
        parts.emplace_back(numRows, partSize, baseRow, baseOffset);
        baseOffset += partSize;
        baseRow += numRows;
        if (hasXml)
            hasXml = partIter->next();
    }
    fileSize = baseOffset;

    Owned<IPropertyTreeIterator> planeIter = xml->getElements("planes");
    ForEach(*planeIter)
    {
        const char * planeName = planeIter->query().queryProp("");
        planes.append(storage.queryPlane(planeName));
    }

    actualCrc = xml->getPropInt("@metaCrc");
#if 0
    size32_t dfsSize = fileProperties.getPropInt("@recordSize");
    if (dfsSize != 0)
    {
        ensureCloned(inputOptions, _inputOptions);
        inputOptions->setPropInt("dfsRecordSize", dfsSize);
    }
#endif
}

#if 0
//Create an entry for a physical file (or explicitly using a protocol)
CLogicalFile::CLogicalFile(const char * _path, bool isLogicalName, CStorageLocation * location, IOutputMetaData * expectedMeta, IPropertyTree * _inputOptions, IPropertyTree * _formatOptions, offset_t previousSize)
: inputOptions(_inputOptions), formatOptions(_formatOptions)
{
    fileBaseOffset = previousSize;
    numParts = 1;
    parts.emplace_back();
    addPartSuffix = false;
    if (isLogicalName)
        name.set(_path);
    else
        path.set(_path);
    locations.append(*LINK(location));
    actualMeta.set(expectedMeta);
    //MORE: For accurate filepositions need to know the filesize, but that may be expensive for non local files
    //may need a flag to indicate whether the virtual fileposition is actually used.
}
#endif

CLogicalFile::CLogicalFile(MemoryBuffer & in)
{
    UNIMPLEMENTED;
}


const IPropertyTree * CLogicalFile::queryFormatOptions() const
{
    return xml->queryPropTree("formatOptions");
}
const IPropertyTree * CLogicalFile::queryInputOptions() const
{
    return xml->queryPropTree("inputOptions");
}

offset_t CLogicalFile::queryOffsetOfPart(unsigned part) const
{
    return fileBaseOffset + queryPart(part).baseOffset;
}

offset_t CLogicalFile::getPartSize(unsigned part) const
{
    if (parts.size() > part)
        return parts[part].fileSize;
    return (offset_t)-1;
}

bool CLogicalFile::isLocal(unsigned part, unsigned copy) const
{
    return queryPlane(copy)->isLocal(part);
}

bool CLogicalFile::onAttachedStorage(unsigned copy) const
{
    return queryPlane(copy)->isAttachedStorage();
}


/*
IFile * CLogicalFile::createFile(unsigned part, unsigned copy) const
{
    if (path)
    {
        assertex(part == 0);
        assertex(queryNumCopies() == 1);
        return createIFile(path);
    }

    StringBuffer url;
    getURL(url, part, copy);
    RemoteFilename filename;
    filename.setRemotePath(url, nullptr);
    return createIFile(filename);
}
*/

//expand name as path, e.g. copy and translate :: into /
StringBuffer & CLogicalFile::expandLogicalAsPhysical(StringBuffer & target, unsigned copy) const
{
    const char * cur = name;
    for (;;)
    {
        const char * colon = strstr(cur, "::");
        if (!colon)
            break;

        //MORE: Process special characters?
        target.append(colon - cur, cur);
        target.append(planes.item(copy)->queryScopeSeparator());
        cur = colon + 2;
    }

    return target.append(cur);
}

StringBuffer & CLogicalFile::expandPath(StringBuffer & target, unsigned part, unsigned copy) const
{
    if (name)
    {
        expandLogicalAsPhysical(target, copy);
    }
    else
    {
        assertex(queryPhysicalPath());
        target.append(queryPhysicalPath());
    }

    //Add part number suffix
    if (includePartSuffix())
    {
        target.append("._").append(part+1).append("_of_").append(numParts);
    }

    return target;
}

StringBuffer & CLogicalFile::getURL(StringBuffer & target, unsigned part, unsigned copy) const
{
    planes.item(copy)->getURL(target, part);
    target.append(planes.item(copy)->queryScopeSeparator());
    return expandPath(target, part, copy);
}


IOutputMetaData * CLogicalFile::queryActualMeta() const
{
    if (!actualMeta)
    {
        actualMeta.setown(getDaliLayoutInfo(*xml));
        if (!actualMeta)
        {
            //MORE: Old files do not have the serialized file format, some new files cannot create them
            //we should possibly have away of distinguishing between the two
            actualMeta.set(expectedMeta);
            actualCrc = 0;
        }
    }
    return actualMeta;
}

const char * CLogicalFile::queryFormat()
{
    return xml->queryProp("@format");
}

unsigned CLogicalFile::getNumCopies() const
{
    return planes.ordinality();
}


const char * CLogicalFile::queryTracingFilename(unsigned part) const
{
    if (name)
        return name;
    //MORE: Include the part number and assign to a string buffer...
    return queryPhysicalPath();
}

const char * CLogicalFile::queryLogicalFilename() const
{
    return name ? name : "";
}


//---------------------------------------------------------------------------------------------------------------------

CLogicalFileSlice::CLogicalFileSlice(CLogicalFile * _file, unsigned _part, offset_t _startOffset, offset_t _length)
: file(_file), part(_part), startOffset(_startOffset), length(_length)
{
}

bool CLogicalFileSlice::isWholeFile() const
{
    if ((startOffset == 0) && file)
    {
        if ((length == unknownFileSize) || (length == file->getPartSize(part)))
            return true;
    }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------

void CLogicalFileCollection::appendFile(CLogicalFile & file)
{
    files.append(file);
    totalSize += file.getFileSize();
}


//Calculate which parts of which files will be included in this channel
//If preserveDistribution is specified then any logical files with more than 1 part are assumed to be distributed, so they shouldn't be split.
//Spread external single part files and files with no distribution across the different channels that are reading the data
//Need to be careful about superfile ordering - part

void CLogicalFileCollection::calcPartition(std::vector<CLogicalFileSlice> & slices, unsigned numChannels, unsigned channel, bool preserveDistribution)
{
    unsigned numFiles = files.ordinality();
    for (unsigned from = 0; from < numFiles; from++)
    {
        CLogicalFile & cur = files.item(from);
        unsigned numParts = cur.getNumParts();
        unsigned to = from+1;
        if ((!preserveDistribution || !cur.isDistributed()) && (numParts < numChannels))
        {
            for (; to < numFiles; to++)
            {
                CLogicalFile & next = files.item(to);
                unsigned nextNum = next.getNumParts();
                if ((!preserveDistribution || !cur.isDistributed()) && (numParts + nextNum< numChannels))
                {
                    numParts += nextNum;
                }
                else
                    break;
            }
        }

        //Two cases - a single file which may have more parts than channels or multiple files which have fewer parts than channels.
        if ((from + 1 == to) && (numParts >= numChannels))
        {
            //A single file which has more parts than channels or multiple files which have fewer parts than channels.
            for (unsigned part=channel; part < numParts; part++)
                slices.emplace_back(&cur, part, 0, unknownFileSize);
        }
        else
        {
            //One or more files which have fewer parts than channels.
            //Split the number of parts of the file by the number of channels.  If distribution doesn't matter then we can try and use partitions
            //MORE: Revisit with partitioning
            unsigned remaining = channel;
            for (unsigned src=from; src < to; src++)
            {
                CLogicalFile & source = files.item(src);
                unsigned sourceParts = source.getNumParts();
                if (remaining < sourceParts)
                {
                    slices.emplace_back(&cur, remaining, 0, unknownFileSize);
                    break;
                }
                remaining -= sourceParts;
            }
        }
    }
}


void CLogicalFileCollection::reset()
{
    totalSize = 0;
    files.kill();
}

void CLogicalFileCollection::init(const char * _wuid,  bool _isTemporary,  bool _isCodeSigned, IUserDescriptor * _user, IOutputMetaData * _expectedMeta)
{
    wuid.set(_wuid);
    isTemporary = _isTemporary;
    isCodeSigned = _isCodeSigned;
    user = _user;
    expectedMeta = _expectedMeta;
}

void CLogicalFileCollection::setEclFilename(const char * _filename, IPropertyTree * _inputOptions, IPropertyTree * _formatOptions)
{
    //Check if the same parameters have been passed, and if so avoid rebuilding the information
    if (strieq(filename, _filename) && areMatchingPTrees(inputOptions, _inputOptions) && areMatchingPTrees(formatOptions, _formatOptions))
        return;

    reset();
    filename.set(_filename);
    inputOptions.set(_inputOptions);
    formatOptions.set(_formatOptions);

    //Partinfo only need if a count operation, or if virtual(fileposition)
    ResolveOptions options = ROincludeLocation|ROpartinfo;
    resolved.setown(resolveLogicalFilename(filename, user, options));

    storageSystems.setFromMeta(resolved);
    Owned<IPropertyTreeIterator> fileIter = resolved->getElements("file");
    ForEach(*fileIter)
    {
        IPropertyTree & cur = fileIter->query();
        if (cur.getPropBool("@missing"))
        {
            const char * filename = cur.queryProp("@name");
            if (!inputOptions->getPropBool("optional", false))
            {
                StringBuffer errorMsg("");
                throw makeStringException(0, errorMsg.append(": Logical file name '").append(filename).append("' could not be resolved").str());
            }
            else
            {
                StringBuffer buff;
                buff.appendf("Input file '%s' was missing but declared optional", filename);
                //agent.addWuExceptionEx(buff.str(), WRN_SkipMissingOptFile, SeverityInformation, MSGAUD_user, "hthor");
            }
        }
        else
        {
            CLogicalFile * file = new CLogicalFile(storageSystems, &cur, totalSize, expectedMeta);
            appendFile(*file);

            bool expectedGrouped = false; // should pass in to init()
            bool isGrouped = false && file->isGrouped();
            if (isGrouped != expectedGrouped)
            {
                StringBuffer msg;
                msg.append("DFS and code generated group info. differs: DFS(").append(isGrouped ? "grouped" : "ungrouped").append("), CodeGen(").append(expectedGrouped ? "ungrouped" : "grouped").append("), using DFS info");
                //throw agent.addWuExceptionEx(msg.str(), WRN_MismatchGroupInfo, SeverityError, MSGAUD_user, "hthor");
                throwUnexpected();
            }
        }
    }
}
