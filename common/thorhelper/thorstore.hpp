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

#ifndef __THORSTORE_HPP_
#define __THORSTORE_HPP_

#ifdef THORHELPER_EXPORTS
 #define THORHELPER_API DECL_EXPORT
#else
 #define THORHELPER_API DECL_IMPORT
#endif

#include <vector>

//Drop direct support for windows shares?   Or should we require them to be locally mounted using symlinks so they fit in with other OSs.
//a) windows 10 now supports non admin symlink creation, b) we don't currently run on windows c) they make everything complicated.
class THORHELPER_API LogicalUrl
{
    // Expanded as a URL in the form <protocol>:<protocol-extra>//<path> or  //<protocolExtra>/path if no protocol?
public:
    StringAttr protocol;
    StringAttr protocolExtra;  // host for thor protocol?
    StringAttr path;
};

static LogicalUrl examples[] = {
        { "s3", "east-coast",  "/bucket/x/y/z.1_of_1" },
        { "azure", nullptr, "/blob/x__y__z.1_of_1" },
        { "nas", nullptr, "/mnt/nas/cdrive/<sid>/x/y/z.1_of_1" },
        { "thor", "mynode", "/var/lib/HpccSystems/hpcc/<sid>/<cluster>/x/y/z.1_of_1" },
};

//All of the following will move to a different location (?dali) once the proof of concept is completed.

//=====================================================================================================================

/*
 * File processing sequence:
 *
 * a) create a CLogicalFileCollection
 *   A) with an implicit superfile
 *      process each of the implicit subfiles in turn
 *      Do we allow logical files and
 *   B) with a logical super file
 *      process each of the subfiles in turn
 *   C) With a logical file
 *      - Resolve the file
 *      - extract the file information from dali
 *      - extract the split points (from dali/disk).
 *        potentially quite large, and expensive?  May want a hint to indicate how many ways the file will be read
 *        to avoid retrieving if there will be no benefit.
 *   D) With a absolute filename  [<host>|group?][:port]//path   (or local path if contains a /)
 *      - NOTE: ./abc can be used to refer to a physical file in the current directory
 *      - Add as a single (or multiple) part physical file, no logical distribution.
 *      - not sure if we should allow the port or not.  Could be used to force use of dafilesrv??
 *      - Need to check access rights - otherwise could extract security keys etc.
 *   E) As a URL transport-protocol://path
 *      - Expand wild cards (or other magic) to produce a full list of parts and nodes.  No logical distribution.
 *      - Retrieve meta information for the file
 *      - Retrieve partition information for the parts.
 *      - possibly allow thor:array//path as another representation for (D).  What would make sense?
 *   F) FILE::...
 *      Check access rights, then translate to same as (D)
 *   G) REMOTE::...
 *      Call into the remote esp to extract the file information and merge it with the location information
 *
 * b) perform protocol dependent processing
 *    - possibly part of stage (a) or a separate phase
 *    - passing #parts that it will be read from to allow split information to be optimized.
 *    A) thor
         - Translate logical nodes to physical ips.
         - gather any missing file sizes
      B) s3/azure blobs
         - Expand any wildcards in the filenames and create entries for each expanded file.
         - gather file sizes
      C) HDFS
         - gather files sizes
         - gather split points
 * c) serialize to the slaves
 * d) deserialize from the master
 * e) call fileCollection::partition(numChannels, myChannel) to get a list of partitions
 * f) iterate though the partitions for the current channel
 *    a) need a reader for the format - how determine?
 *    b) Where we determine if it should be read directly or via dafilesrv?
 *    c) request a row reader for the
 *
 * Questions:
 *    Where are the translators created?
 *    What is signed?  It needs to be a self contained block of data that can easily be serialized and deserialized.
 *      I don't think it needs to contain information about the storage array - only the logical file.
 */

class CStorageSystems;
typedef StringArray LogicalGroup;
typedef std::vector<IpAddress> PhysicalGroup;

//What is a good term for a collection of drives
//storage array/system/

//This describes the a set of disks that can be used to store a logical file.
//  "device" is used to represent the index within the storage plane
class THORHELPER_API CStorageHostGroup : public CInterface
{
public:
    CStorageHostGroup(const IPropertyTree * _xml);

    bool isLocal(unsigned device) const;

private:
    const IPropertyTree * xml;
};


//This describes the a set of disks that can be used to store a logical file.
//  "device" is used to represent the index within the storage plane
class THORHELPER_API CStoragePlane : public CInterface
{
    friend class CStorageSystems;

public:
    CStoragePlane() = default;
    CStoragePlane(const IPropertyTree * _xml, const CStorageHostGroup * _host);

    bool containsHost(const char * host) const;
    bool containsPath(const char * path);
    bool matches(const char * search) const { return strsame(name, search); }
    bool matchesHost(const char * host);

    unsigned getCost(unsigned device, const IpAddress & accessIp, PhysicalGroup & peerIPs) const;
//    unsigned getInterleave() const { return interleave; }
    StringBuffer & getURL(StringBuffer & target, unsigned device, unsigned drive) const;
    unsigned getWidth() const;
    bool isLocal(unsigned device) const;
    bool onAttachedStorage() const;
    const char * queryName() const { return name; }

protected:
    void load(const IPropertyTree * xml);
    void save(IPropertyTree * xml);

private:
    const IPropertyTree * xml;
    const char * name;
    const CStorageHostGroup * host = nullptr;
    unsigned numDevices = 0;
    StringAttr protocol;          // thor/nas/s3/azure
    StringAttr protocolExtra;
    StringArray rootPaths;
    Owned<IPropertyTree> options;
//    bool isLocallyMounted = false; // a remote drive which is locally mounted
//    bool isLocalStorage = false; // is this storage tied to the compute nodes?  This should possibly be stored or calculated elsewhere
    unsigned interleave = 1;
    bool canReadRemote = false;         // has dafilesrv running on the nodes.
/*
 * NOTES:
 * Storage array information may not need to be serialized if the target already has the information
 */
};

#if 0
//Represents a subset of a StorageArray e.g. when writing to a subset of the nodes in a thor group, or a single part distributed to a random node
//A subgroup can be represented as name[:size][@offset] when stored in dali.
class CStorageLocation : public CInterface
{
    friend class CStorageSystems;  // could create accessor functions instead

public:
    CStorageLocation(const char * _name, CStoragePlane * _plane, unsigned _offset, unsigned _size, unsigned _copy, unsigned _startDelta, const char * _subDir);
    CStorageLocation(CStorageSystems & systems, IPropertyTree * xml);

    StringBuffer & getURL(StringBuffer & target, unsigned part) const;
    bool isLocal(unsigned part) const;
    bool onAttachedStorage() const;

    bool matches(const char * search) const { return strsame(name, search); }
    CStoragePlane * queryPlane() const { return plane; }
    const char * queryScopeSeparator() const { return plane->queryScopeSeparator(); }

protected:
    unsigned getDevice(unsigned part) const;
    unsigned getDrive(unsigned part) const;
    void load(CStorageSystems & systems, IPropertyTree * xml);
    void save(IPropertyTree * xml);

private:
    StringAttr name;
    StringAttr subDir;
    Linked<CStoragePlane> plane;
    unsigned offset = 0;  // offset + size <= plane->getWidth();
    unsigned size = 0;  // size <= plane->getWidth();
    unsigned copy = 0;  // Each copy creates a different location entry (with a different startDelta)
    unsigned startDelta = 0;  // allows different replication to be represented 0|1|width|....  Can be > size if plane has multiple drives
    unsigned startDrive = 0;
};
#endif

class CStorageSystems
{
public:
    void setFromMeta(IPropertyTree * _meta);

    const CStorageHostGroup * queryHostGroup(const char * search) const;
    const CStoragePlane * queryPlane(const char * search) const;

/*
    CStorageLocation * queryHostLocation(const char * host, const char * path) const;
    CStorageLocation * queryLocalLocation(const char * path) const;
    CStorageLocation * queryLocalSpillLocation() const;
    CStorageLocation * queryLocation(const char * name) const;
    CStorageLocation * queryProtocolLocation(const char * protocol, const char * protocolExtra) const;
    const char * queryTempfilePath();
    CStorageLocation * resolveLocation(const char * name, unsigned copy);
    CStoragePlane * queryPlane(const char * name) const;
*/
protected:
    CIArrayOf<CStorageHostGroup> hostGroups;
    CIArrayOf<CStoragePlane> planes;
};

extern THORHELPER_API CStorageSystems & queryGlobalStorageSystems();

#endif
