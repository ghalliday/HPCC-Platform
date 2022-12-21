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

#include "jliball.hpp"

#include "thorfile.hpp"

#include "eclhelper.hpp"
#include "eclrtl.hpp"
#include "eclrtl_imp.hpp"
#include "rtlfield.hpp"
#include "rtlds_imp.hpp"
#include "rtldynfield.hpp"

#include "eclhelper_base.hpp"
#include "thorcommon.ipp"
#include "jhtree.hpp"

#include "keybuild.hpp"
#include "roxiemem.hpp"

void setExpiryTime(IPropertyTree & properties, unsigned expireDays)
{
    properties.setPropInt("@expireDays", expireDays);
}

void setRtlFormat(IPropertyTree & properties, IOutputMetaData * meta)
{
    if (meta && meta->queryTypeInfo())
    {
        MemoryBuffer out;
        if (dumpTypeInfo(out, meta->querySerializedDiskMeta()->queryTypeInfo()))
            properties.setPropBin("_rtlType", out.length(), out.toByteArray());
    }
}


class DiskWorkUnitReadArg : public CThorDiskReadArg
{
public:
    DiskWorkUnitReadArg(const char * _filename, IHThorWorkunitReadArg * _wuRead) : filename(_filename), wuRead(_wuRead)
    {
        recordSize.set(wuRead->queryOutputMeta());
    }
    virtual IOutputMetaData * queryOutputMeta() override
    {
        return wuRead->queryOutputMeta();
    }
    virtual const char * getFileName() override
    {
        return filename;
    }
    virtual IOutputMetaData * queryDiskRecordSize() override
    {
        return (IOutputMetaData *)recordSize;
    }
    virtual IOutputMetaData * queryProjectedDiskRecordSize() override
    {
        return (IOutputMetaData *)recordSize;
    }
    virtual unsigned getDiskFormatCrc() override
    {
        return 0;
    }
    virtual unsigned getProjectedFormatCrc() override
    {
        return 0;
    }
    virtual unsigned getFlags() override
    {
        return 0;
    }
    virtual size32_t transform(ARowBuilder & rowBuilder, const void * src) override
    {
        unsigned size = recordSize.getRecordSize(src);
        memcpy(rowBuilder.ensureCapacity(size, NULL), src, size);
        return size;
    }

protected:
    StringAttr filename;
    Linked<IHThorWorkunitReadArg> wuRead;
    CachedOutputMetaData recordSize;
};




IHThorDiskReadArg * createWorkUnitReadArg(const char * filename, IHThorWorkunitReadArg * wuRead)
{
    return new DiskWorkUnitReadArg(filename, wuRead);
}

//-----------------------------------------------------------------------------

#define MAX_FILE_READ_FAIL_COUNT 3

IKeyIndex *openKeyFile(IDistributedFilePart & keyFile)
{
    unsigned failcount = 0;
    unsigned numCopies = keyFile.numCopies();
    assertex(numCopies);
    Owned<IException> exc;
    for (unsigned copy=0; copy < numCopies && failcount < MAX_FILE_READ_FAIL_COUNT; copy++)
    {
        RemoteFilename rfn;
        try
        {
            OwnedIFile ifile = createIFile(keyFile.getFilename(rfn,copy));
            offset_t thissize = ifile->size();
            if (thissize != (offset_t)-1)
            {
                StringBuffer remotePath;
                rfn.getPath(remotePath);
                unsigned crc = 0;
                keyFile.getCrc(crc);
                return createKeyIndex(remotePath.str(), crc, false);
            }
        }
        catch (IException *E)
        {
            EXCLOG(E, "While opening index file");
            if (exc)
                E->Release();
            else
                exc.setown(E);
            failcount++;
        }
    }
    if (exc)
        throw exc.getClear();
    StringBuffer url;
    RemoteFilename rfn;
    keyFile.getFilename(rfn).getRemotePath(url);
    throw MakeStringException(1001, "Could not open key file at %s%s", url.str(), (numCopies > 1) ? " or any alternate location." : ".");
}


//-----------------------------------------------------------------------------

constexpr unsigned defaultTimeout = 10000;

static bool checkIndexMetaInformation(IDistributedFilePart * part, bool force)
{
    if (part->queryAttributes().hasProp("@offsetBranches"))
        return true;

    if (!force)
        return false;

    Owned<IKeyIndex> index = openKeyFile(*part);
    offset_t branchOffset = index->queryFirstBranchOffset();

    part->lockProperties(defaultTimeout);
    part->queryAttributes().setPropInt64("@offsetBranches", branchOffset);
    part->unlockProperties();
    return true;
}

bool checkIndexMetaInformation(IDistributedFile * file, bool force)
{
    IDistributedSuperFile * super = file->querySuperFile();
    if (super)
    {
        Owned<IDistributedFileIterator> subfiles = super->getSubFileIterator(true);
        bool first = false;
        ForEach(*subfiles)
        {
            IDistributedFile & cur = subfiles->query();
            if (!checkIndexMetaInformation(&cur, force))
                return false;
        }
        return true;
    }

    try
    {
        IPropertyTree & attrs = file->queryAttributes();
        if (!attrs.hasProp("@nodeSize") || !attrs.hasProp("@keyedSize"))
        {
            if (!force)
                return false;

            //Read values from the header of the first index part and save in the meta data
            IDistributedFilePart & part = file->queryPart(0);
            Owned<IKeyIndex> index = openKeyFile(part);
            size_t keySize = index->keySize();
            size_t nodeSize = index->getNodeSize();

            //FUTURE: When file information is returned from an esp service, will this be updated?
            file->lockProperties(defaultTimeout);
            file->queryAttributes().setPropInt("@nodeSize", nodeSize);
            file->queryAttributes().setPropInt("@keyedSize", keySize);
            file->unlockProperties();
        }

        if (!attrs.hasProp("@numLeafNodes"))
        {
            Owned<IDistributedFilePartIterator> parts = file->getIterator();
            ForEach(*parts)
            {
                IDistributedFilePart & cur = parts->query();
                if (!checkIndexMetaInformation(&cur, force))
                    return false;
            }
        }

        return true;
    }
    catch (IException * e)
    {
        e->Release();
        return false;
    }
}

bool calculateDerivedIndexInformation(DerivedIndexInformation & result, IDistributedFile * file, bool force)
{
    if (!checkIndexMetaInformation(file, force))
        return false;

    IDistributedSuperFile * super = file->querySuperFile();
    if (super)
    {
        Owned<IDistributedFileIterator> subfiles = super->getSubFileIterator(true);
        bool first = true;
        ForEach(*subfiles)
        {
            IDistributedFile & cur = subfiles->query();
            if (!first)
            {
                DerivedIndexInformation nextInfo;
                nextInfo.gather(&cur);
                result.merge(nextInfo);
            }
            else
            {
                result.gather(&cur);
                first = false;
            }
        }
    }
    else
        result.gather(file);

    return true;
}


//-----------------------------------------------------------------------------

void buildUserMetadata(Owned<IPropertyTree> & metadata, IHThorIndexWriteArg & helper)
{
    size32_t nameLen;
    char * nameBuff;
    size32_t valueLen;
    char * valueBuff;
    unsigned idx = 0;
    while (helper.getIndexMeta(nameLen, nameBuff, valueLen, valueBuff, idx++))
    {
        StringBuffer name(nameLen, nameBuff);
        StringBuffer value(valueLen, valueBuff);
        rtlFree(nameBuff);
        rtlFree(valueBuff);
        if(*name == '_' && !checkReservedMetadataName(name))
        {
            roxiemem::OwnedRoxieString fname(helper.getFileName());
            throw MakeStringException(0, "Invalid name %s in user metadata for index %s (names beginning with underscore are reserved)", name.str(), fname.get());
        }
        if(!validateXMLTag(name.str()))
        {
            roxiemem::OwnedRoxieString fname(helper.getFileName());
            throw MakeStringException(0, "Invalid name %s in user metadata for index %s (not legal XML element name)", name.str(), fname.get());
        }
        if(!metadata)
            metadata.setown(createPTree("metadata", ipt_fast));
        metadata->setProp(name.str(), value.str());
    }
}
