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


CStorageHostGroup::CStorageHostGroup(const IPropertyTree * _xml)
: xml(_xml)
{
}

bool CStorageHostGroup::isLocal(unsigned device) const
{
    VStringBuffer xpath("part[%u]", device);
    const char * hostname = xml->queryProp(xpath);
    if (xpath)
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
 : xml(_xml), host(_host)
{
    name = xml->queryProp("@name");
    numDevices = xml->getPropInt("@numDevices", 1);
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
    return startsWith(path, rootPaths.item(0));
}

bool CStoragePlane::matchesHost(const char * host)
{
    return (xml->getCount("hosts") == 1) && containsHost(host);
}

StringBuffer & CStoragePlane::getURL(StringBuffer & target, unsigned device, unsigned drive) const
{
    // protocol:<extra>//[ip/]path
    if (!strsame(protocol, "thor"))
        target.append(protocol).append(":").append(protocolExtra);

#if 0
    //include some form of the ip if it is required (not required for s3)
    if (includeIpInPath)
    {
        if (!resolvedNodes.empty())
        {
            unsigned node = (resolvedNodes.size() != 1) ? device : 0;
            target.append("//");
            resolvedNodes[node].getIpText(target);
        }
        else if (logicalNodes.ordinality())
        {
            unsigned node = (logicalNodes.ordinality() != 1) ? device : 0;
            target.append("//");
            target.append(logicalNodes.item(node));
        }
    }

    target.append(rootPaths.item(drive));
    if (includeNameInPath)
        target.append(queryScopeSeparator()).append(name);
    if (includeDeviceInPath)
        target.append(queryScopeSeparator()).append(device);
#endif
    return target;
}


unsigned CStoragePlane::getWidth() const
{
    return numDevices;  // Could subdivide even if not done by ip
}

unsigned CStoragePlane::getCost(unsigned device, const IpAddress & accessIp, PhysicalGroup & peerIPs) const
{
    return 0;
    #if 0
    if (resolvedNodes.size())
    {
        unsigned compareIndex = (resolvedNodes.size() == 1) ? 0 : device;
        if (resolvedNodes[compareIndex].ipequals(accessIp))
            return directCost;

        //Slight concern that this is O(#ips)
        for (auto & cur : peerIPs)
            if (resolvedNodes[compareIndex].ipequals(cur))
                return localCost;
    }
    if (logicalNodes.ordinality())
        throwUnexpectedX("Should have resolved ips before calculating best location");
    return remoteCost;
    #endif
}

bool CStoragePlane::isLocal(unsigned device) const
{
    if (host)
        return host->isLocal(device);
    return false;
}

bool CStoragePlane::onAttachedStorage() const
{
    return !protocol || strsame(protocol, "thor");
}

//---------------------------------------------------------------------------------------------------------------------

CStorageLocation::CStorageLocation(const char * _name, CStoragePlane * _plane, unsigned _offset, unsigned _size, unsigned _copy, unsigned _startDelta, const char * _subDir)
: name(_name), plane(_plane), offset(_offset), size(_size), copy(_copy), startDelta(_startDelta), subDir(_subDir)
{
}

CStorageLocation::CStorageLocation(CStorageSystems & systems, IPropertyTree * xml)
{
    load(systems, xml);
}

unsigned CStorageLocation::getDevice(unsigned part) const
{
    unsigned nodeDelta = (part + startDelta) % size;
    unsigned device = (nodeDelta + offset);
    assertex(device < plane->getWidth());
    return device;
}

unsigned CStorageLocation::getDrive(unsigned part) const
{
    unsigned driveDelta = (part + startDelta) / size;
    unsigned drive = (startDrive + driveDelta) % plane->getNumDrives();
    return drive;
}

StringBuffer & CStorageLocation::getURL(StringBuffer & target, unsigned part) const
{
    plane->getURL(target, getDevice(part), getDrive(part));
    if (subDir)
        target.append(PATHSEPCHAR).append(subDir);
    return target;
}


bool CStorageLocation::isLocal(unsigned part) const
{
    return plane->isLocal(getDrive(part));
}

bool CStorageLocation::onAttachedStorage() const
{
    return plane->onAttachedStorage();
}


void CStorageLocation::load(CStorageSystems & systems, IPropertyTree * xml)
{
    name.set(xml->queryProp("@name"));
    StringAttr planeName(xml->queryProp("@plane"));// need to resolve this in the list of planes
    plane.set(systems.queryPlane(planeName));
    assertex(plane);
    offset = xml->getPropInt("@offset", 0);
    size = xml->getPropInt("@size", plane->getWidth());
    copy = xml->getPropInt("@copy", 0);
    startDelta = xml->getPropInt("@delta", copy);
    startDrive = xml->getPropInt("@drive", 0);
    subDir.set(xml->queryProp("@subDir"));
}

void CStorageLocation::save(IPropertyTree * xml)
{
    xml->setProp("@name", name);
    xml->setProp("@plane", plane->queryName());
    xml->setPropInt("@offset", offset);
    xml->setPropInt("@size", size);
    xml->setPropInt("@copy", copy);
    if (startDelta)
        xml->setPropInt("@delta", startDelta);
    if (startDrive)
        xml->setPropInt("@drive", startDrive);
    xml->setProp("@subDir", subDir);
}

//---------------------------------------------------------------------------------------------------------------------

CStorageHostGroup * CStorageSystems::queryHostGroup(const char * search)
{
    if (!search)
        return nullptr;

    ForEachItemIn(i, hostsGroups)
    {
        CStorageHostGroup & cur = hostGroups.item(i);
        if (strsame(search, cur.queryName()))
            return &cur;
    }
    return nullptr;
}

CStoragePlane * CStorageSystems::queryPlane(const char * search)
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
    Owned<IPropertyTreeIterator> hostIter = xml->getElements("hostGroups");
    ForEach(*hostIter)
        hostGroups.append(*new CStorageHostGroup(hostIter->query()));

    Owned<IPropertyTreeIterator> planeIter = xml->getElements("storage/planes");
    ForEach(*planeIter)
    {
        IPropertyTree * cur = planeIter->query();
        CStorageHostGroup * hosts = queryHostGroup(cur->queryProp("@hosts"));
        planes.append(*new CStoragePlane(cur, hosts));
    }
}

#if  0
void CStorageSystems::addDefaultLocations(CStoragePlane * plane, const char * subDir)
{
    unsigned width = plane->interleave ? plane->interleave : plane->getWidth();

    CStorageLocation * primary = new CStorageLocation(plane->queryName(), plane, 0, width, 0, 0, subDir);
    locations.append(*primary);
    addCopyLocations(primary);
}

void CStorageSystems::addCopyLocations(CStorageLocation * primary)
{
    CStoragePlane * plane = primary->plane;
    unsigned copies = plane->getNumDefaultCopies();
    unsigned interleave = plane->getInterleave();
    unsigned offset = primary->offset;
    unsigned delta = interleave ? 0 : 1;
    for (unsigned copy=1; copy < copies; copy++)
    {
        if (interleave)
            offset = (offset + interleave) % plane->getWidth();
        StringBuffer name;
        name.append(primary->name).append(".").append(copy);
        locations.append(* new CStorageLocation(name, plane, primary->offset, primary->size, copy, copy*delta, primary->subDir));
    }
}

CStorageLocation * CStorageSystems::queryLocation(const char * name) const
{
    ForEachItemIn(i, locations)
    {
        if (locations.item(i).matches(name))
            return &locations.item(i);
    }
    return nullptr;
}

CStorageLocation * CStorageSystems::resolveLocation(const char * name, unsigned copy)
{
    ForEachItemIn(i, locations)
    {
        if (locations.item(i).matches(name))
            return &locations.item(i);
    }

    //Allow system[:size[@offset]] to create a subset
    const char * colon = strchr(name, ':');
    CStoragePlane * plane;
    if (colon)
    {
        StringBuffer systemName(colon-name, name);
        plane = queryPlane(systemName);
    }
    else
        plane = queryPlane(name);

    unsigned offset = 0;
    unsigned width = plane->getWidth();
    unsigned size = width;
    if (colon)
    {
        char * end = nullptr;
        size = strtoul(colon+1, &end, 10);
        assertex(size <= width);

        if (end && *end == '@')
        {
            offset = strtoul(end+1, nullptr, 10);
            assertex(offset + size <= width);
        }
    }

    unsigned startDelta = copy;
    //MORE: Include information from ClusterPartDiskMapSpec::calcPartLocation so can easily map (part,copy) to a node.
    locations.append(* new CStorageLocation(name, plane, offset, size, copy, startDelta, nullptr));
    return &locations.tos();
}

CStorageLocation * CStorageSystems::queryHostLocation(const char * host, const char * path) const
{
    //Should possibly loop through the planes, and then find a location that matches the name of the plane
    ForEachItemIn(i, locations)
    {
        CStorageLocation & cur = locations.item(i);
        CStoragePlane * plane = cur.queryPlane();
        if (plane->matchesHost(host))
        {
            //Ensure that the path is a subset of the path supported by the plane - as a security check
            if (plane->containsPath(path))
                return &cur;
        }
    }
    return nullptr;
}

CStorageLocation * CStorageSystems::queryLocalSpillLocation() const
{
    return queryLocation("_local_spill_");
}

CStorageLocation * CStorageSystems::queryLocalLocation(const char * path) const
{
    return queryHostLocation(".", path);
}

CStorageLocation * CStorageSystems::queryProtocolLocation(const char * protocol, const char * protocolExtra) const
{
    ForEachItemIn(i, planes)
    {
        CStoragePlane & cur = planes.item(i);
        if (strsame(cur.protocol, protocol) && strsame(cur.protocolExtra, protocolExtra))
            return queryLocation(cur.queryName());
    }
    return nullptr;
}
#endif
