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


CStoragePlane::CStoragePlane(const IPropertyTree * in)
{
    load(in);
}

void CStoragePlane::serialize(MemoryBuffer & out) const
{
    //MORE: Serialize to an IPropertyTree
    UNIMPLEMENTED;
}

bool CStoragePlane::containsHost(const char * host)
{
    ForEachItemIn(i, logicalNodes)
    {
        if (streq(logicalNodes.item(i), host))
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
    return (logicalNodes.ordinality() == 1) && streq(logicalNodes.item(0), host);
}

StringBuffer & CStoragePlane::getURL(StringBuffer & target, unsigned device, unsigned drive) const
{
    // protocol:<extra>//[ip/]path
    if (!strsame(protocol, "thor"))
        target.append(protocol).append(":").append(protocolExtra);

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
    return target;
}


unsigned CStoragePlane::getWidth() const
{
    if (logicalNodes)
        return logicalNodes.ordinality();
    if (!resolvedNodes.empty())
        return resolvedNodes.size();
    return numDevices;  // Could subdivide even if not done by ip
}

unsigned CStoragePlane::getCost(unsigned device, const IpAddress & accessIp, PhysicalGroup & peerIPs) const
{
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
}

bool CStoragePlane::isLocal(unsigned device) const
{
    if (resolvedNodes.size() > device)
        return resolvedNodes[device].isLocal();
    if (logicalNodes.ordinality() > device)
    {
        IpAddress ip(logicalNodes.item(device));
        return ip.isLocal();
    }
    return false;
}

bool CStoragePlane::onAttachedStorage() const
{
    return !protocol || strsame(protocol, "thor");
}

void CStoragePlane::load(const IPropertyTree * xml)
{
    name.set(xml->queryProp("@name"));
    protocol.set(xml->queryProp("@protocol"));
    protocolExtra.set(xml->queryProp("@protocolExtra"));
    //logicalNodes;    // textual representation stored in dali.  Either nodes for storage, or nodes that it is mounted on.

    const char * host = xml->queryProp("@host");
    if (host)
        logicalNodes.append(host);

    Owned<IPropertyTreeIterator> hostIter = xml->getElements("host");
    ForEach(*hostIter)
    {
        const char * host = hostIter->query().queryProp("");
        assertex(host && *host);
        logicalNodes.append(host);
    }

    // Is the ip the only interesting item, or does this more structure?
    Owned<IPropertyTreeIterator> ipIter = xml->getElements("ip");
    ForEach(*ipIter)
    {
        IpAddress ip(ipIter->query().queryProp(""));
        resolvedNodes.push_back(ip);
    }

    options.set(xml->queryPropTree("options"));
    directCost = xml->getPropInt("@directCost");
    localCost = xml->getPropInt("@localCost");
    remoteCost = xml->getPropInt("@remoteCost");
    rootPaths.append(xml->queryProp("@path"));      // more could allow multiple mounts to multiple local disk drives
    scopeSeparator.set(xml->queryProp("@separator"));
    numDevices = logicalNodes.ordinality();
    if (numDevices == 0)
        numDevices = xml->getPropInt("@numDevices", 1);
    defaultCopies = xml->getPropInt("@numReplicas", 1);
    interleave = xml->getPropInt("@interleave", 0);     // set to size/2 for most roxie clusters
    canReadRemote = xml->getPropBool("@canReadRemote", false);
    includeIpInPath = xml->getPropBool("@includeIpInPath", strsame(protocol, "thor"));
    includeNameInPath = xml->getPropBool("@includeNameInPath", false);
    includeDeviceInPath = xml->getPropBool("@includeDeviceInPath", (numDevices != 1));
}

void CStoragePlane::save(IPropertyTree * xml)
{
    xml->setProp("@name", name);
    xml->setProp("@protocol", protocol);
    xml->setProp("@protocolExtra", protocolExtra);

    ForEachItemIn(i, logicalNodes)
        xml->addProp("host", logicalNodes.item(i));

    for (auto & node : resolvedNodes)
    {
        StringBuffer ipText;
        node.getIpText(ipText);
        xml->addProp("ip", ipText);
    }

    options.set(xml->queryPropTree("options"));
    if (directCost)
        xml->setPropInt("@directCost", directCost);
    if (localCost)
        xml->setPropInt("@localCost", localCost);
    if (remoteCost)
        xml->setPropInt("@remoteCost", remoteCost);

    //MORE: Do we allow more than one?
    xml->setProp("@path", rootPaths.item(0));

    xml->setProp("@separator", scopeSeparator);
    if (numDevices != logicalNodes.ordinality())
        xml->setPropInt("@numDevices", numDevices);

    if (defaultCopies != 1)
        xml->setPropInt("@numReplicas", defaultCopies);
    if (interleave != 0)
        xml->setPropInt("@interleave", interleave);
    xml->setPropBool("@canReadRemote", canReadRemote);
    xml->setPropBool("@includeIpInPath", includeIpInPath);
    xml->setPropBool("@includeNameInPath", includeNameInPath);
    xml->setPropBool("@includeDeviceInPath", includeDeviceInPath);
}

constexpr const char * demoStoragePlanes =
  "<Config>"
    "<StoragePlanes>"
        "<StoragePlane name='mys3' protocol='s3' protocolExtra='@eu-west-2' remoteCost='100' path='/myBucket'/>"
        "<StoragePlane name='myazure' protocol='azure' remoteCost='100' path='/myBlob' includeNameInPath='false' includeDeviceInPagth='false' separator='__scope__'/>"
        "<StoragePlane name='mynas' protocol='nas' directCost='5' localCost='10' remoteCost='100' path='/dev/mnt/mynas' numDevices='4'>"
        "  <Mount name='mythorgroup1' canReadRemote='true'/>"
        "  <Mount name='mythorgroup2'/>"
        "  <Mount name='myhthor'/>"
        // Should they be mounted on all nodes??  If not, how does it work with dynamic ips.
        "</StoragePlane>"
        "<StoragePlane name='win' protocol='thor' host='.' directCost='5' localCost='10' remoteCost='30' canReadRemote='true' path='c:\\HPCCSystems\\data' numReplicas='2' includeNameInPath='true' separator='\\'/>"

        "<StoragePlane name='_local_spill_' protocol='thor' host='.' directCost='5' canReadRemote='false' path='/var/lib/HPCCSystems/data' includeNameInPath='false' separator='__scope__'/>"
        "<StoragePlane name='_local_' protocol='thor' host='.' directCost='5' canReadRemote='false' path='/'/>"    // For the moment allow access to any files.
    "</StoragePlanes>"
    "<StorageLocations>"
      "<StorageLocation name='mythor20_0' plane='mythor' offset='0' size='20'/>"
      //"<StorageLocation name='mythor20_0.1' plane='mythor' offset='0' size='20'/>" // Automatically create locations for the replicas of subsets
      "<StorageLocation name='mythor20_1' plane='mythor' offset='20' size='20'/>"
      "<StorageLocation name='mythor:20+20' plane='mythor' offset='20' size='20'/>"   //Should this be the name instead i.e. offset and size included in the name???
      "<StorageLocation name='mythor20_2' plane='mythor' offset='40' size='20'/>"
      "<StorageLocation name='mythor20_3' plane='mythor' offset='60' size='20'/>"
      "<StorageLocation name='mythor20_4' plane='mythor' offset='80' size='20'/>"

      //"<StorageLocation name='myroxie' plane='myroxie' offset='0' size='50'/>"
      //"<StorageLocation name='myroxie.1' plane='myroxie' offset='50' size='50'/ copy='1' delta='0'/>"   //See code to calculate this automatically from "interleave" - should have a better name
    "</StorageLocations>"
  "</Config>";

//How do nas planes work - need to specify which machines they are accessible through - it is different from the multiple planes since the same file can be accessed through any of the nodes
//Do they need to be mounted on all nodes?  That makes most sense (managed by the container manager from the configuration).  Is that a potential security flaw?
//How about fusion thor where the plane is local to the machines, but is mounted on several machines.  very similar to nas, except that dafilesrv is supported.  Maybe identical.

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

void CStorageSystems::populateFromEnvironment(IPropertyTree * xml)
{
    INamedGroupStore & groups = queryNamedGroupStore();
    Owned<INamedGroupIterator> iter = groups.getIterator();
    ForEach(*iter)
    {
        StringBuffer groupName;
        StringBuffer directory;
        GroupType groupType;
        Owned<IGroup> group = groups.lookup(iter->get(groupName).str(), directory, groupType);
        const char * tailSep = strrchr(directory, '/');
        assertex(tailSep && tailSep != directory);
        StringBuffer tail(tailSep+1);
        directory.setLength(tailSep - directory.str());

        Owned<IPropertyTree> info = createPTree();
        info->setProp("@name", groupName);
        info->setProp("@protocol", "thor");

        unsigned numNodes = group->ordinality();
        for (unsigned i=0; i < numNodes; i++)
        {
            StringBuffer ipText;
            group->queryNode(i).endpoint().getIpText(ipText);
            info->addProp("ip", ipText);
        }

        info->setPropInt("@directCost", 5);
        info->setPropInt("@localCost", 10);
        info->setPropInt("@remoteCost", 30);

        info->setPropBool("@canReadRemote", true);
        info->setProp("@path", directory);

        if (groupType == grp_thor)
            info->setPropInt("@numReplicas", 2);

        info->setPropBool("@includeNameInPath", false);

        Owned<CStoragePlane> next = new CStoragePlane(info);
        assertex(!queryPlane(next->queryName()));
        planes.append(*LINK(next));
        addDefaultLocations(next, tail);
    }

    Owned<IPropertyTreeIterator> planeIter = xml->getElements("StoragePlanes/StoragePlane");
    ForEach(*planeIter)
    {
        CStoragePlane * next = new CStoragePlane(&planeIter->query());
        assertex(!queryPlane(next->queryName()));
        planes.append(*LINK(next));
        //MORE: implicit locations could be created on demand, which is best?
        addDefaultLocations(next, nullptr);
    }

    Owned<IPropertyTreeIterator> locationIter = xml->getElements("StorageLocations/StorageLocation");
    ForEach(*locationIter)
    {
        CStorageLocation * next = new CStorageLocation(*this, &locationIter->query());
        locations.append(*next);
        //MORE: implicit copy locations could be created on demand
        addCopyLocations(next);
    }

    numEnvPlanes = planes.ordinality();
    numEnvLocations = locations.ordinality();

    Owned<IPropertyTree> saved = createPTree("Storage");
    save(saved, true);
    printXML(saved);

}

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

CStoragePlane * CStorageSystems::queryPlane(const char * name) const
{
    ForEachItemIn(i, planes)
    {
        if (planes.item(i).matches(name))
            return &planes.item(i);
    }

    return nullptr;
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


void CStorageSystems::save(IPropertyTree * xml, bool includeEnvironment) const
{
    unsigned firstPlane = includeEnvironment ? 0 : numEnvPlanes;
    for (unsigned i=firstPlane; i < planes.ordinality(); i++)
    {
        planes.item(i).save(xml->addPropTree("StoragePlane"));
    }

    unsigned firstLocation  = includeEnvironment ? 0 : numEnvLocations;
    for (unsigned i2=firstLocation; i2 < locations.ordinality(); i2++)
    {
        locations.item(i2).save(xml->addPropTree("StorageLocation"));
    }
}

//---------------------------------------------------------------------------------------------------------------------

static CStorageSystems globalStorageSystems;
bool initialized = false;
extern THORHELPER_API CStorageSystems & queryGlobalStorageSystems()
{
    if (!initialized)
    {
        Owned<IPropertyTree> env = createPTreeFromXMLString(demoStoragePlanes);
        globalStorageSystems.populateFromEnvironment(env);
        initialized = true;
        DBGLOG("Using new GlobalStorageSystems");
    }
    //MORE: This needs to be initialized from the environment, for the moment using the Thor groups, but then extend it
    return globalStorageSystems;
}
