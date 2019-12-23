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


CLogicalFile::CLogicalFile(IDistributedFile * file, IOutputMetaData * expectedMeta, IPropertyTree * _inputOptions, IPropertyTree * _formatOptions, offset_t previousSize, CStorageSystems & storageSystems) : inputOptions(_inputOptions)
{
    numParts = file->numParts();
    fileBaseOffset = previousSize;

    offset_t baseOffset = 0;
    offset_t baseRow = 0;
    for (unsigned part=0; part < numParts; part++)
    {
        IDistributedFilePart & cur = file->queryPart(part);
        offset_t partSize = cur.getFileSize(true, false);
        offset_t numRows = 0; // MORE: This should be stored in the meta and extracted at this point.
        parts.emplace_back(numRows, partSize, baseRow, baseOffset);
        baseOffset += partSize;
        baseRow += numRows;
    }
    name.set(file->queryLogicalName());
    fileSize = baseOffset;
    addPartSuffix = true;

    IPropertyTree & fileProperties = file->queryAttributes();
    const char * kind = fileProperties.queryProp("@kind");
    if (kind && (stricmp(kind, "key") == 0))
        format.set("key");
    else
        format.set(fileProperties.queryProp("@format"));

    actualMeta.setown(getDaliLayoutInfo(fileProperties));
    actualCrc = fileProperties.getPropInt("@formatCrc");

    if (!actualMeta)
    {
        //MORE: Old files do not have the serialized file format, some new files cannot create them
        //we should possibly have away of distinguishing between the two
        actualMeta.set(expectedMeta);
        actualCrc = 0;
    }

    unsigned numClusters = file->numClusters();
    for (unsigned cluster=0; cluster < numClusters; cluster++)
    {
        StringBuffer clusterName;
        file->getClusterGroupName(cluster, clusterName);
        unsigned numCopies = file->numCopies(0); // This should depend on the storage subsystem, not the part number
        for (unsigned copy=0; copy < numCopies; copy++)
            locations.append(*LINK(storageSystems.resolveLocation(clusterName, copy)));
    }

    accessToken.clear();

    bool blockcompressed = false;
    bool compressed = file->isCompressed(&blockcompressed); //try new decompression, fall back to old unless marked as block
    if (compressed || blockcompressed)
    {
        inputOptions.setown(createPTreeFromIPT(_inputOptions));
        inputOptions->setPropBool("compressed", compressed);
        inputOptions->setPropBool("blockCompressed", blockcompressed);
    }
    size32_t dfsSize = fileProperties.getPropInt("@recordSize");
    if (dfsSize != 0)
    {
        ensureCloned(inputOptions, _inputOptions);
        inputOptions->setPropInt("dfsRecordSize", dfsSize);
    }


    formatOptions.setown(createPTreeFromIPT(_formatOptions));
    queryInheritProp(*formatOptions, "quote", fileProperties, "@csvQuote");
    queryInheritSeparatorProp(*formatOptions, "separator", fileProperties, "@csvSeparate");
    queryInheritProp(*formatOptions, "terminator", fileProperties, "@csvTerminate");
    queryInheritProp(*formatOptions, "escape", fileProperties, "@csvEscape");
    if (areMatchingPTrees(formatOptions, _formatOptions))
        formatOptions.set(_formatOptions);
}

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

CLogicalFile::CLogicalFile(MemoryBuffer & in)
{
    UNIMPLEMENTED;
}

offset_t CLogicalFile::getPartSize(unsigned part) const
{
    if (parts.size() > part)
        return parts[part].fileSize;
    return (offset_t)-1;
}

bool CLogicalFile::isLocal(unsigned part, unsigned copy) const
{
    return locations.item(copy).isLocal(part);
}

bool CLogicalFile::onAttachedStorage(unsigned copy) const
{
    return locations.item(copy).onAttachedStorage();
}

void CLogicalFile::serialize(MemoryBuffer & out) const
{
    UNIMPLEMENTED;
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
        target.append(locations.item(copy).queryScopeSeparator());
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
        assertex(path);
        target.append(path);
    }

    //Add part number suffix
    if ((numParts > 1) || addPartSuffix)
    {
        target.append("._").append(part+1).append("_of_").append(numParts);
    }

    return target;
}

StringBuffer & CLogicalFile::getURL(StringBuffer & target, unsigned part, unsigned copy) const
{
    locations.item(copy).getURL(target, part);
    target.append(locations.item(copy).queryScopeSeparator());
    return expandPath(target, part, copy);
}


IOutputMetaData * CLogicalFile::queryActualMeta() const
{
    return actualMeta;
}

const char * CLogicalFile::queryFormat()
{
    if (format)
        return format;
    return defaultFileFormat;
}

unsigned CLogicalFile::queryNumCopies() const
{
    if (path)
        return 1;
    return locations.ordinality();
}


const char * CLogicalFile::queryLogicalFilename() const
{
    return name ? name.str() : "";
}

const char * CLogicalFile::queryTracingFilename(unsigned part) const
{
    if (name)
        return name;
    //MORE: Include the part number and assign to a string buffer...
    return path;
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
    totalSize += file.queryExpandedFileSize();
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


void CLogicalFileCollection::processFile(IDistributedFile * file, IOutputMetaData * expectedMeta, IPropertyTree * inputOptions, IPropertyTree * formatOptions)
{
    IDistributedSuperFile * super = file->querySuperFile();
    if (super)
    {
        unsigned max = super->numSubFiles(false);
        for (unsigned i=0; i < max; i++)
        {
            Owned<IDistributedFile> child = super->getSubFile(i, false);
            processFile(child, expectedMeta, inputOptions, formatOptions);
        }
        return;
    }

    //MORE:
    //Option for resolving filenames in dali, or resolving them as local filenames
    //Check for existence of files and complain if missing.  Complain about empty keys?
    //Pass isCodeSigned and other options down to this class/function
    //Log read access to the file
    bool isCodeSigned = false;
    bool isGrouped = false; // initialize both of these when class instance is created;
    //bool wasGrouped = file->isGrouped(); // MORE: This should be implemented
    bool wasGrouped = false;
    if (wasGrouped != isGrouped)
    {
        StringBuffer msg;
        msg.append("DFS and code generated group info. differs: DFS(").append(wasGrouped ? "grouped" : "ungrouped").append("), CodeGen(").append(isGrouped ? "ungrouped" : "grouped").append("), using DFS info");
        //throw agent.addWuExceptionEx(msg.str(), WRN_MismatchGroupInfo, SeverityError, MSGAUD_user, "hthor");
        throwUnexpected();
    }

    appendFile(* new CLogicalFile(file, expectedMeta, inputOptions, formatOptions, totalSize, storageSystems));
}

void CLogicalFileCollection::processFilename(CDfsLogicalFileName & logicalFilename, IUserDescriptor *user, bool isTemporary, IOutputMetaData * expectedMeta, IPropertyTree * inputOptions, IPropertyTree * formatOptions)
{
    if (logicalFilename.isMulti())
    {
        assertex(!isTemporary);
        if (!logicalFilename.isExpanded())
            logicalFilename.expand(user); //expand wild-cards

        unsigned max = logicalFilename.multiOrdinality();
        for (unsigned child=0; child < max; child++)
            processFilename(const_cast<CDfsLogicalFileName &>(logicalFilename.multiItem(child)), user, false, expectedMeta, inputOptions, formatOptions);
        return;
    }

    const char * raw = logicalFilename.get(false);
    if (logicalFilename.isExternal())
    {
    }
    else
    {
        //Following could be implemented as logicalFilename.isPhysical()
        const char * slash = strchr(raw, '/');
        if (slash)
        {
            //If the filename contains a slash then treat it as a physical filename
            processPhysicalFilename(raw, expectedMeta, inputOptions, formatOptions);
        }
        else
        {
            if (isTemporary)
            {
                UNIMPLEMENTED;
                CStorageLocation * location = storageSystems.queryLocalSpillLocation();
                appendFile(* new CLogicalFile(raw, true, location, expectedMeta, inputOptions, formatOptions, totalSize));
            }
            else
            {
                Owned<IDistributedFile> f = queryDistributedFileDirectory().lookup(logicalFilename, user, true, false, false, nullptr, defaultNonPrivilegedUser);
                if (f)
                    processFile(f, expectedMeta, inputOptions, formatOptions);
                else
                    processMissing(raw, inputOptions);
            }
        }
    }
}

void CLogicalFileCollection::processMissing(const char * filename, IPropertyTree * inputOptions)
{
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

void CLogicalFileCollection::processPhysicalFilename(const char * path, IOutputMetaData * expectedMeta, IPropertyTree * inputOptions, IPropertyTree * formatOptions)
{
    const char * slash = strchr(path, '/');
    assertex(slash);

    //If the filename contains a ':' before the slash then assume format is protocol:/path
    //Could possibly support windows style c:, but may be cleaner to require soft links
    const char * colon = strchr(path, ':');
    if (colon && colon < slash)
    {
        processProtocolFilename(path, colon, slash, expectedMeta, inputOptions, formatOptions);
        return;
    }

    /*
     * The filename is either in the form
     *
     * //ip/path or /path or relative-path
     */
    StringBuffer absolutePath;
    if (path[0] != '/')
    {
        makeAbsolutePath(path, absolutePath, false);
        path = absolutePath;
    }

    //MORE: Security risk..... what kind of checking should be done?
    CStorageLocation * location = nullptr;
    if (path[1] == '/')
    {
        const char * start = path+2;
        const char * slash = strchr(start, '/');
        assertex(slash);

        StringBuffer ip(slash-start, start);
        location = storageSystems.queryHostLocation(ip, slash);
        path = slash;
    }
    else
    {
        location = storageSystems.queryHostLocation(".", path);
    }

    if (location)
    {
        //MORE: Update totalSize member
        appendFile(* new CLogicalFile(path, false, location, expectedMeta, inputOptions, formatOptions, totalSize));
    }
    else
    {
        throw makeStringExceptionV(99, "No permission to access file %s", path);
    }

}

void CLogicalFileCollection::processProtocolFilename(const char * name, const char * colon, const char * slash, IOutputMetaData * expectedMeta, IPropertyTree * inputOptions, IPropertyTree * formatOptions)
{
    StringBuffer protocol(colon-name, name);
    StringBuffer protocolExtra(slash-(colon+1), colon+1);

    CStorageLocation * location = storageSystems.queryProtocolLocation(protocol, protocolExtra);
    appendFile(* new CLogicalFile(slash, false, location, expectedMeta, inputOptions, formatOptions, totalSize));
}

void CLogicalFileCollection::reset()
{
    totalSize = 0;
    files.kill();
}

void CLogicalFileCollection::setEclFilename(const char * filename, bool isTemporary, bool isCodeSigned, IUserDescriptor * user, IOutputMetaData * expectedMeta, IPropertyTree * inputOptions, IPropertyTree * formatOptions)
{
    reset();

    //Walk the contents of the filename.
    //   Add entries for logical files.
    //   Expand super files (both explicit and implicit)
    //   Add entries for (wild-carded) external or hooked data files.
    //This should allow more flexibility e.g. physical/external files within super files.
    if (strchr(filename, PATHSEPCHAR))
    {
        processPhysicalFilename(filename, expectedMeta, inputOptions, formatOptions);
    }
    else
    {
        CDfsLogicalFileName logicalFilename;
        logicalFilename.set(filename);
        processFilename(logicalFilename, user, isTemporary, expectedMeta, inputOptions, formatOptions);
    }

    //MORE: Check that the grouping of the resolved files match the grouping specified in the helper.
    //Take into account isTemporary as well.  Where does the code for scope mangling live, and how does it connect with creating output filenames?
    //Should at least be some common global functions.  Once you have deployed queries the same spill might be written by multiple workunits.
}
