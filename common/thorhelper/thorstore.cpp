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

#include "thorstore.hpp"



StringBuffer & expandPlanePath(StringBuffer & target, const char * path, unsigned device)
{
    for (;;)
    {
        const char * hash = strchr(path, '#');
        if (!hash)
            break;

        target.append(hash-path, path);
        unsigned width = 1;
        while (hash[1] == '#')
        {
            hash++;
            width++;
        }

        target.appendf("%0*u", width, device);
        path = hash+1;
    }
    target.append(path);
    return target;
}

CStorageHostGroup::CStorageHostGroup(const IPropertyTree * _xml)
: xml(_xml)
{
}

const char * CStorageHostGroup::queryName() const
{
    return xml->queryProp("@name");
}

const char * CStorageHostGroup::queryHost(unsigned idx) const
{
    VStringBuffer xpath("part[%u]", idx);
    return xml->queryProp(xpath);
}

bool CStorageHostGroup::isLocal(unsigned device) const
{
    const char * hostname = queryHost(device);
    if (hostname)
    {
        //MORE: Likely to be inefficient - should search differently?
        IpAddress ip(hostname);
        return ip.isLocal();
    }
    return false;
}

/*
 * A StoragePlane represents a set of storage devices that are used together to store a distributed file.  Each
 * storage plane has a fixed number of logical devices.  If there are > 1 then the storage plane index can be include
 * within the file path - which allows data to be striped across multiple devices.
 *
 * For S3 etc. it specifies the region, bucket and root path.
 * For local NFS mounted NAS it specifies the mount point.
 * For other NAS solutions it could indicate a list of ips, and root paths.
 * For locally attached storage it specifies the root path, and a set of hostnames.  Also whether dafilesrv is allowed/required.
 *
 * For NAS/cloud situation, each cluster should define the default location the data is stored (although it can be overridden in the OUTPUT)..
 *
 * For an attached storage scheme, each cluster defined in the environment implicitly defines an locally attached storage plane.  It would
 * be useful for any cluster to be able to specify a separate plane for the spill files e.g., ramdisk/faster disks.
 * It could alternatively rely on an implicit plane (for the current node),
 *
 * There should be a special local plane for spill files and other temporary files.  How do these fit in?  Should
 * there be a special ip group ('!') that refers to the cluster for the current component?
 *
 * What problems are there resolving ips - e.g. in the globally shared storage plane?  Is it done in the plane or in the logical file?
 */


CStoragePlane::CStoragePlane(const IPropertyTree * _xml, const CStorageHostGroup * _host)
 : xml(_xml), hostGroup(_host)
{
    name = xml->queryProp("@name");
    numDevices = xml->getPropInt("@numDevices", 1);
    size = xml->getPropInt("@size", numDevices);
    offset = xml->getPropInt("@size", 0);
    startDelta = xml->getPropInt("@offset", 0);
    startDrive = 0; // Not sure if we really want to support multiple drives..
}

bool CStoragePlane::containsHost(const char * host) const
{
    Owned<IPropertyTreeIterator> iter = xml->getElements("hosts");
    ForEach(*iter)
    {
        if (streq(iter->query().queryProp(""), host))
            return true;
    }
    return false;
}

bool CStoragePlane::containsPath(const char * path)
{
    return startsWith(path, queryPath());
}

bool CStoragePlane::matchesHost(const char * host)
{
    return (xml->getCount("hosts") == 1) && containsHost(host);
}

const char * CStoragePlane::queryPath() const
{
    const char * path = xml->queryProp("@prefix");
    return path ? path : "/";
}

StringBuffer & CStoragePlane::getURL(StringBuffer & target, unsigned part) const
{
    unsigned device = getDevice(part);
    unsigned drive = getDrive(part);

    if (hostGroup)
    {
        target.append("//");
        target.append(hostGroup->queryHost(device));
    }

    //MORE: Should this allow drive to modify and use $D<n>$ syntax instead?  I think that functionality  can be lost.
    expandPlanePath(target, queryPath(), device);
    return target;
}


unsigned CStoragePlane::getWidth() const
{
    return numDevices;  // Could subdivide even if not done by ip
}

#if 0
//What is the cost of accessing part "part" from host "accessIp"
unsigned CStoragePlane::getCost(unsigned part, const const char * accessIp) const
{
    if (!hostGroup)
        return xml->getPropInt("@accessCost", remoteCost);
    unsigned device = getDevice(part);
    if (hosts->isLocal(device, accessIp))
        return directCost;
    if (hostGroup->isSameNetwork(device, accessIp))
        return localDCost;
    return remoteCost;
}
#endif

unsigned CStoragePlane::getDevice(unsigned part) const
{
    unsigned nodeDelta = (part + startDelta) % size;
    unsigned device = (nodeDelta + offset);
    assertex(device < getWidth());
    return device;
}

unsigned CStoragePlane::getDrive(unsigned part) const
{
    unsigned driveDelta = (part + startDelta) / size;
    unsigned drive = (startDrive + driveDelta) % getNumDrives();
    return drive;
}

bool CStoragePlane::isLocal(unsigned part) const
{
    if (hostGroup)
        return hostGroup->isLocal(getDevice(part));
    return false;
}

bool CStoragePlane::isAttachedStorage() const
{
    return (hostGroup != nullptr);
}

//---------------------------------------------------------------------------------------------------------------------

const CStorageHostGroup * CStorageSystems::queryHostGroup(const char * search) const
{
    if (!search)
        return nullptr;

    ForEachItemIn(i, hostGroups)
    {
        CStorageHostGroup & cur = hostGroups.item(i);
        if (strsame(search, cur.queryName()))
            return &cur;
    }
    return nullptr;
}

const CStoragePlane * CStorageSystems::queryPlane(const char * search) const
{
    if (!search)
        return nullptr;

    ForEachItemIn(i, planes)
    {
        CStoragePlane & cur = planes.item(i);
        if (strsame(search, cur.queryName()))
            return &cur;
    }
    return nullptr;
}

void CStorageSystems::setFromMeta(IPropertyTree * xml)
{
    IPropertyTree * storage = xml->queryPropTree("storage");

    //MORE: Is it worth checking if the hostGroups and storageplanes are the same as last time?
    //if areMatchingPTrees(storage, savedStorage) return;
    hostGroups.kill();
    planes.kill();

    if (!storage)
        return;

    Owned<IPropertyTreeIterator> hostIter = storage->getElements("hostGroups");
    ForEach(*hostIter)
        hostGroups.append(*new CStorageHostGroup(&hostIter->query()));

    Owned<IPropertyTreeIterator> planeIter = storage->getElements("planes");
    ForEach(*planeIter)
    {
        IPropertyTree * cur = &planeIter->query();
        const CStorageHostGroup * hosts = queryHostGroup(cur->queryProp("@hosts"));
        planes.append(*new CStoragePlane(cur, hosts));
    }
}
