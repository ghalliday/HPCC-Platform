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

/*
 * Dali Quick Regression Suite: Tests Dali functionality on a programmatic way.
 *
 * Add as much as possible here to avoid having to run the Hthor/Thor regressions
 * all the time for Dali tests, since most of it can be tested quickly from here.
 */

#ifdef _USE_CPPUNIT
#include "mpbase.hpp"
#include "mpcomm.hpp"
#include "daclient.hpp"
#include "dadfs.hpp"
#include "dafdesc.hpp"
#include "dasds.hpp"
#include "danqs.hpp"
#include "dautils.hpp"
#include "dastats.hpp"

#include "wujobq.hpp"

#include <vector>
#include <future>
#include <math.h>

#include "jthread.hpp"

#include "unittests.hpp"
#include "sysinfologger.hpp"

//#define COMPAT

// ======================================================================= Support Functions / Classes

static __int64 subchangetotal;
static unsigned subchangenum;
static CriticalSection subchangesect;
static IRemoteConnection *Rconn;
static IDistributedFileDirectory & dir = queryDistributedFileDirectory();
static IUserDescriptor *user = createUserDescriptor();
static unsigned initCounter = 0; // counter for initialiser
static unsigned numStdFiles = 0;
static unsigned numSuperFiles = 0;

// Declared in dadfs.cpp *only* when CPPUNIT is active
extern void removeLogical(const char *fname, IUserDescriptor *user);

void daliClientInit()
{
    // Only initialise on first pass
    if (initCounter != 0)
        return;
    InitModuleObjects();
    user->set("user", "passwd");
    // Connect to local Dali
    SocketEndpoint ep;
    ep.set(".", 7070);
    SocketEndpointArray epa;
    epa.append(ep);
    Owned<IGroup> group = createIGroup(epa);
    initClientProcess(group, DCR_Testing);

    initCounter++;
}

void daliClientEnd()
{
    if (!initCounter)
        return;
    else if (1 == initCounter) // Only destroy on last pass
    {
        // Cleanup
        releaseAtoms();
        closedownClientProcess();
        setNodeCaching(false);
    }
    else
        initCounter--;
}

interface IChecker
{
    virtual void title(unsigned n,const char *s)=0;
    virtual void add(const char *s,__int64 v)=0;
    virtual void add(const char *s,const char* v)=0;
    virtual void add(unsigned n,const char *s,__int64 v)=0;
    virtual void add(unsigned n,const char *s,const char* v)=0;
    virtual void error(const char *txt)=0;
};

void checkFilePart(IChecker *checker,IDistributedFilePart *part,bool blocked)
{
    StringBuffer tmp;
    checker->add("getPartIndex",part->getPartIndex());
    unsigned n = part->numCopies();
    checker->add("numCopies",part->numCopies());
    checker->add("maxCopies",n);
    RemoteFilename rfn;
    for (unsigned copy=0;copy<n;copy++) {
        INode *node = part->queryNode(copy);
        if (node)
            checker->add(copy,"queryNode",node->endpoint().getEndpointHostText(tmp.clear()).str());
        else
            checker->error("missing node");
        checker->add(copy,"getFilename",part->getFilename(rfn,copy).getRemotePath(tmp.clear()).str());
    }
    checker->add("getPartName",part->getPartName(tmp.clear()).str());
#ifndef COMPAT
    checker->add("getPartDirectory",part->getPartDirectory(tmp.clear()).str());
#endif
    checker->add("queryProperties()",toXML(&part->queryAttributes(),tmp.clear()).str());
    checker->add("isHost",part->isHost()?1:0);
    checker->add("getFileSize",part->getFileSize(false,false));
    CDateTime dt;
    if (part->getModifiedTime(false,false,dt))
        dt.getString(tmp.clear());
    else
        tmp.clear().append("nodatetime");
    checker->add("getModifiedTime",tmp.str());
    unsigned crc;
    if (part->getCrc(crc)&&!blocked)
        checker->add("getCrc",crc);
    else
        checker->add("getCrc","nocrc");
}


void checkFile(IChecker *checker,IDistributedFile *file)
{
    StringBuffer tmp;
    checker->add("queryLogicalName",file->queryLogicalName());
    unsigned np = file->numParts();
    checker->add("numParts",np);
    checker->add("queryDefaultDir",file->queryDefaultDir());
    if (np>1)
        checker->add("queryPartMask",file->queryPartMask());
    checker->add("queryProperties()",toXML(&file->queryAttributes(),tmp.clear()).str());
    CDateTime dt;
    if (file->getModificationTime(dt))
        dt.getString(tmp.clear());
    else
        tmp.clear().append("nodatetime");

    // Owned<IFileDescriptor> desc = getFileDescriptor();
    // checkFileDescriptor(checker,desc);

    //virtual bool existsPhysicalPartFiles(unsigned short port) = 0;                // returns true if physical patrs all exist (on primary OR secondary)

    //Owned<IPropertyTree> tree = getTreeCopy();
    //checker->add("queryProperties()",toXML(tree,tmp.clear()).str());

    checker->add("getFileSize",file->getFileSize(false,false));
    bool blocked;
    checker->add("isCompressed",file->isCompressed(&blocked)?1:0);
    checker->add("blocked",blocked?1:0);
    unsigned csum;
    if (file->getFileCheckSum(csum)&&!blocked)
        checker->add("getFileCheckSum",csum);
    else
        checker->add("getFileCheckSum","nochecksum");
    StringBuffer clustname;
    checker->add("queryClusterName(0)",file->getClusterName(0,clustname).str());
    for (unsigned i=0;i<np;i++) {
        Owned<IDistributedFilePart> part = file->getPart(i);
        if (part)
            checkFilePart(checker,part,blocked);
    }
}

void checkFiles(const char *fn)
{
    class cChecker: implements IChecker
    {
    public:
        virtual void title(unsigned n,const char *s)
        {
            printf("Title[%d]='%s'\n",n,s);
        }
        virtual void add(const char *s,__int64 v)
        {
            printf("%s=%" I64F "d\n",s,v);
        }
        virtual void add(const char *s,const char* v)
        {
            printf("%s='%s'\n",s,v);
        }
        virtual void add(unsigned n,const char *s,__int64 v)
        {
            printf("%s[%d]=%" I64F "d\n",s,n,v);
        }
        virtual void add(unsigned n,const char *s,const char* v)
        {
            printf("%s[%d]='%s'\n",s,n,v);
        }
        virtual void error(const char *txt)
        {
            printf("ERROR '%s'\n",txt);
        }
    } checker;
    unsigned start = msTick();
    unsigned slowest = 0;
    StringAttr slowname;
    if (fn) {
        checker.title(1,fn);
        try {
            Owned<IDistributedFile> file=queryDistributedFileDirectory().lookup(fn,user,AccessMode::tbdRead,false,false,nullptr,defaultNonPrivilegedUser);
            if (!file)
                printf("file '%s' not found\n",fn);
            else
                checkFile(&checker,file);
        }
        catch (IException *e) {
            StringBuffer str;
            e->errorMessage(str);
            e->Release();
            checker.error(str.str());
        }
    }
    else {
        Owned<IDistributedFileIterator> iter = queryDistributedFileDirectory().getIterator("*",false,user,defaultNonPrivilegedUser);
        unsigned i=0;
        unsigned ss = msTick();
        ForEach(*iter) {
            i++;
            StringBuffer lfname;
            iter->getName(lfname);
            checker.title(i,lfname.str());
            try {
                IDistributedFile &file=iter->query();
                checkFile(&checker,&file);
                unsigned t = (msTick()-ss);
                if (t>slowest) {
                    slowest = t;
                    slowname.set(lfname.str());
                }
            }
            catch (IException *e) {
                StringBuffer str;
                e->errorMessage(str);
                e->Release();
                checker.error(str.str());
            }
            ss = msTick();
        }
    }
    unsigned t = msTick()-start;
    printf("Complete in %ds\n",t/1000);
    if (!slowname.isEmpty())
        printf("Slowest %s = %dms\n",slowname.get(),slowest);
};

const char *filelist=
    "thor_data400::gong_delete_plus,"
    "thor_data400::in::npanxx,"
    "thor_data400::tpm_deduped,"
    "thor_data400::base::movers_ingest_ll,"
    "thor_hank::cemtemp::fldl,"
    "thor_data400::patch,"
    "thor_data400::in::flvehreg_01_prethor_upd200204_v3,"
    "thor_data400::in::flvehreg_01_prethor_upd20020625_v3_flag,"
    "thor_data400::in::flvehreg_01_prethor_upd20020715_v3,"
    "thor_data400::in::flvehreg_01_prethor_upd20020715_v3_v3_flag,"
    "thor_data400::in::flvehreg_01_prethor_upd20020816_v3,"
    "thor_data400::in::flvehreg_01_prethor_upd20020816_v3_flag,"
    "thor_data400::in::flvehreg_01_prethor_upd20020625_v3,"
    "thor_data400::in::fl_lic_prethor_200208v2,"
    "thor_data400::in::fl_lic_prethor_200209,"
    "thor_data400::in::fl_lic_prethor_200210,"
    "thor_data400::in::fl_lic_prethor_200210_reclean,"
    "thor_data400::in::fl_lic_upd_200301,"
    "thor_data400::in::fl_lic_upd_200302,"
    "thor_data400::in::oh_lic_200209,"
    "thor_data400::in::ohio_lic_upd_200210,"
    "thor_data400::prepped_for_keys,"
    "a0e65__w20060224-155748,"
    "common_did,"
    "test::ftest1,"
    "thor_data50::BASE::chunk,"
    "hthor::key::codes_v320040901,"
    "thor_data400::in::ucc_direct_ok_99999999_event_20060124,"
    "thor400::ks_work::distancedetails";

#ifndef COMPAT

void dispFDesc(IFileDescriptor *fdesc)
{
    printf("======================================\n");
    Owned<IPropertyTree> pt = createPTree("File");
    fdesc->serializeTree(*pt);
    StringBuffer out;
    toXML(pt,out);
    printf("%s\n",out.str());
    Owned<IFileDescriptor> fdesc2 = deserializeFileDescriptorTree(pt);
    toXML(pt,out.clear());
    printf("%s\n",out.str());
    unsigned np = fdesc->numParts();
    unsigned ncl = fdesc->numClusters();
    printf("numclusters = %d, numparts=%d\n",ncl,np);
    for (unsigned pass=0;pass<1;pass++) {
        for (unsigned ip=0;ip<np;ip++) {
            IPartDescriptor *part = fdesc->queryPart(ip);
            unsigned nc = part->numCopies();
            for (unsigned ic=0;ic<nc;ic++) {
                StringBuffer tmp1;
                StringBuffer tmp2;
                StringBuffer tmp3;
                StringBuffer tmp4;
                RemoteFilename rfn;
                bool blocked;
                out.clear().appendf("%d,%d: '%s' '%s' '%s' '%s' '%s' '%s' %s%s%s",ip,ic,
                    part->getDirectory(tmp1,ic).str(),
                    part->getTail(tmp2).str(),
                    part->getPath(tmp3,ic).str(),
                    fdesc->getFilename(ip,ic,rfn).getRemotePath(tmp4).str(),  // multi TBD
                    fdesc->queryPartMask()?fdesc->queryPartMask():"",
                    fdesc->queryDefaultDir()?fdesc->queryDefaultDir():"",
                    fdesc->isGrouped()?"GROUPED ":"",
                    fdesc->queryKind()?fdesc->queryKind():"",
                    fdesc->isCompressed(&blocked)?(blocked?" BLOCKCOMPRESSED":" COMPRESSED"):""
                );
                printf("%s\n",out.str());
                if (1) {
                    MemoryBuffer mb;
                    part->serialize(mb);
                    Owned<IPartDescriptor> copypart;
                    copypart.setown(deserializePartFileDescriptor(mb));
                    StringBuffer out2;
                    out2.appendf("%d,%d: '%s' '%s' '%s' '%s' '%s' '%s' %s%s%s",ip,ic,
                        copypart->getDirectory(tmp1.clear(),ic).str(),
                        copypart->getTail(tmp2.clear()).str(),
                        copypart->getPath(tmp3.clear(),ic).str(),
                        copypart->getFilename(ic,rfn).getRemotePath(tmp4.clear()).str(),  // multi TBD
                        copypart->queryOwner().queryPartMask()?copypart->queryOwner().queryPartMask():"",
                        copypart->queryOwner().queryDefaultDir()?copypart->queryOwner().queryDefaultDir():"",
                        copypart->queryOwner().isGrouped()?"GROUPED ":"",
                        copypart->queryOwner().queryKind()?copypart->queryOwner().queryKind():"",
                        copypart->queryOwner().isCompressed(&blocked)?(blocked?" BLOCKCOMPRESSED":" COMPRESSED"):""
                    );
                    if (strcmp(out.str(),out2.str())!=0)
                        printf("FAILED!\n%s\n%s\n",out.str(),out2.str());
                    pt.setown(createPTree("File"));
                    copypart->queryOwner().serializeTree(*pt);
                    StringBuffer out;
                    toXML(pt,out);
                //  printf("%d,%d: \n%s\n",ip,ic,out.str());

                }
            }
        }
    }
}

#endif


// ================================================================================== UNIT TESTS

class CDaliTestsStress : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(CDaliTestsStress);
        CPPUNIT_TEST(testInit);
        CPPUNIT_TEST(testDFS);
//        CPPUNIT_TEST(testReadAllSDS); // Ignoring this test; See comments below
        CPPUNIT_TEST(testFiles);
#ifndef COMPAT
        CPPUNIT_TEST(testDF1);
        CPPUNIT_TEST(testDF2);
        CPPUNIT_TEST(testMisc);
        CPPUNIT_TEST(testDFile);
#endif
    CPPUNIT_TEST_SUITE_END();


    const IContextLogger &logctx;

public:
    CDaliTestsStress() : logctx(queryDummyContextLogger())
    {
    }
    ~CDaliTestsStress()
    {
        daliClientEnd();
    }
    void testInit()
    {
        daliClientInit();
    }
#ifndef COMPAT
    void testDF1()
    {
        const char * fname = "testing::propfile2";
        Owned<IFileDescriptor> fdesc = createFileDescriptor();
        Owned<IPropertyTree> pt = createPTree("Attr");
        RemoteFilename rfn;
        rfn.setRemotePath("//10.150.10.80/c$/thordata/test/part._1_of_3");
        pt->setPropInt("@size",123);
        fdesc->setPart(0,rfn,pt);
        rfn.setRemotePath("//10.150.10.81/c$/thordata/test/part._2_of_3");
        pt->setPropInt("@size",456);
        fdesc->setPart(1,rfn,pt);
        rfn.setRemotePath("//10.150.10.82/c$/thordata/test/part._3_of_3");
        pt->setPropInt("@size",789);
        fdesc->setPart(2,rfn,pt);
        dispFDesc(fdesc);
        try {
            removeLogical(fname, user);
            Owned<IDistributedFile> file = queryDistributedFileDirectory().createNew(fdesc);
            {
                DistributedFilePropertyLock lock(file);
                lock.queryAttributes().setProp("@testing","1");
            }
            file->attach(fname,user);
        } catch (IException *e) {
            StringBuffer msg;
            e->errorMessage(msg);
            logctx.CTXLOG("Caught exception while setting property: %s", msg.str());
            e->Release();
        }
    }
    void testDF2() // 4*3 superfile
    {
        Owned<IFileDescriptor> fdesc = createFileDescriptor();
        Owned<IPropertyTree> pt = createPTree("Attr");
        RemoteFilename rfn;
        rfn.setRemotePath("//10.150.10.80/c$/thordata/test/partone._1_of_3");
        pt->setPropInt("@size",1231);
        fdesc->setPart(0,rfn,pt);
        rfn.setRemotePath("//10.150.10.80/c$/thordata/test/parttwo._1_of_3");
        pt->setPropInt("@size",1232);
        fdesc->setPart(1,rfn,pt);
        rfn.setRemotePath("//10.150.10.80/c$/thordata/test/partthree._1_of_3");
        pt->setPropInt("@size",1233);
        fdesc->setPart(2,rfn,pt);
        rfn.setRemotePath("//10.150.10.80/c$/thordata/test2/partfour._1_of_3");
        pt->setPropInt("@size",1234);
        fdesc->setPart(3,rfn,pt);
        rfn.setRemotePath("//10.150.10.81/c$/thordata/test/partone._2_of_3");
        pt->setPropInt("@size",4565);
        fdesc->setPart(4,rfn,pt);
        rfn.setRemotePath("//10.150.10.81/c$/thordata/test/parttwo._2_of_3");
        pt->setPropInt("@size",4566);
        fdesc->setPart(5,rfn,pt);
        rfn.setRemotePath("//10.150.10.81/c$/thordata/test/partthree._2_of_3");
        pt->setPropInt("@size",4567);
        fdesc->setPart(6,rfn,pt);
        rfn.setRemotePath("//10.150.10.81/c$/thordata/test2/partfour._2_of_3");
        pt->setPropInt("@size",4568);
        fdesc->setPart(7,rfn,pt);
        rfn.setRemotePath("//10.150.10.82/c$/thordata/test/partone._3_of_3");
        pt->setPropInt("@size",7899);
        fdesc->setPart(8,rfn,pt);
        rfn.setRemotePath("//10.150.10.82/c$/thordata/test/parttwo._3_of_3");
        pt->setPropInt("@size",78910);
        fdesc->setPart(9,rfn,pt);
        rfn.setRemotePath("//10.150.10.82/c$/thordata/test/partthree._3_of_3");
        pt->setPropInt("@size",78911);
        fdesc->setPart(10,rfn,pt);
        rfn.setRemotePath("//10.150.10.82/c$/thordata/test2/partfour._3_of_3");
        pt->setPropInt("@size",78912);
        fdesc->setPart(11,rfn,pt);
        ClusterPartDiskMapSpec mspec;
        mspec.interleave = 4;
        fdesc->endCluster(mspec);
        dispFDesc(fdesc);
    }
    void testMisc()
    {
        ClusterPartDiskMapSpec mspec;
        Owned<IGroup> grp = createIGroup("10.150.10.1-3");
        RemoteFilename rfn;
        for (unsigned i=0;i<3;i++)
            for (unsigned ic=0;ic<mspec.defaultCopies;ic++)
            {
                constructPartFilename(grp,i+1,ic,3,0,0,false,(i==1)?"test.txt":NULL,"/c$/thordata/test","test._$P$_of_$N$",0,rfn);
                StringBuffer tmp;
                printf("%d,%d: %s\n",i,ic,rfn.getRemotePath(tmp).str());
            }
    }
    void testDFile()
    {
        ClusterPartDiskMapSpec map;
        {   // 1: single part file old method
#define TN "1"
            removeLogical("test::ftest" TN, user);
            Owned<IFileDescriptor> fdesc = createFileDescriptor();
            RemoteFilename rfn;
            rfn.setRemotePath("//10.150.10.1/c$/thordata/test/ftest" TN "._1_of_1");
            fdesc->setPart(0,rfn);
            fdesc->endCluster(map);
            fdesc->queryPart(0)->queryProperties().setPropInt64("@size", 123);
            Owned<IDistributedFile> file = queryDistributedFileDirectory().createNew(fdesc);
            file->attach("test::ftest" TN,user);
#undef TN
        }
        {   // 2: single part file new method
#define TN "2"
            removeLogical("test::ftest" TN, user);
            Owned<IFileDescriptor> fdesc = createFileDescriptor();
            fdesc->setPartMask("ftest" TN "._$P$_of_$N$");
            fdesc->setNumParts(1);
            fdesc->queryPart(0)->queryProperties().setPropInt64("@size", 123);
            Owned<IGroup> grp = createIGroup("10.150.10.1");
            fdesc->addCluster(grp,map);
            Owned<IDistributedFile> file = queryDistributedFileDirectory().createNew(fdesc);
            file->attach("test::ftest" TN,user);
#undef TN
        }
        queryNamedGroupStore().add("__testgroup3__", { "10.150.10.1", "10.150.10.2", "10.150.10.3" },true);
        Owned<IGroup> grp3 = queryNamedGroupStore().lookup("test_dummy_group");
        {   // 3: three parts file old method
#define TN "3"
            removeLogical("test::ftest" TN, user);
            Owned<IFileDescriptor> fdesc = createFileDescriptor();
            RemoteFilename rfn;
            rfn.setRemotePath("//10.150.10.1/c$/thordata/test/ftest" TN "._1_of_3");
            fdesc->setPart(0,rfn);
            rfn.setRemotePath("//10.150.10.2/c$/thordata/test/ftest" TN "._2_of_3");
            fdesc->setPart(1,rfn);
            rfn.setRemotePath("//10.150.10.3/c$/thordata/test/ftest" TN "._3_of_3");
            fdesc->setPart(2,rfn);
            fdesc->endCluster(map);
            for (unsigned p=0; p<fdesc->numParts(); p++)
                fdesc->queryPart(p)->queryProperties().setPropInt64("@size", 10);
            Owned<IDistributedFile> file = queryDistributedFileDirectory().createNew(fdesc);
            file->attach("test::ftest" TN,user);
#undef TN
        }
        {   // 4: three part file new method
#define TN "4"
            removeLogical("test::ftest" TN, user);
            Owned<IFileDescriptor> fdesc = createFileDescriptor();
            fdesc->setPartMask("ftest" TN "._$P$_of_$N$");
            fdesc->setNumParts(3);
            for (unsigned p=0; p<fdesc->numParts(); p++)
                fdesc->queryPart(p)->queryProperties().setPropInt64("@size", 10);
            fdesc->addCluster(grp3,map);
            Owned<IDistributedFile> file = queryDistributedFileDirectory().createNew(fdesc);
            file->attach("test::ftest" TN,user);
#undef TN
        }
    }
#endif
    /*
     * This test is invasive, obsolete and the main source of
     * errors in the DFS code. It was created on a time where
     * the DFS API was spread open and methods could openly
     * fiddle with its internals without injury. Times have changed.
     *
     * TODO: Convert this test into a proper test of the DFS as
     * it currently stands, not work around its deficiencies.
     *
     * Unfortunately, to do that, some functionality has to be
     * re-worked (like creating groups, adding files to it,
     * creating physical temporary files, etc).
     */
    void testDFS()
    {
        const size32_t recsize = 17;
        StringBuffer s;
        unsigned i;
        unsigned n;
        unsigned t;
        queryNamedGroupStore().remove("daregress_group");
        dir.removeEntry("daregress::superfile1", user);
        std::vector<std::string> hosts;
        for (n=0;n<400;n++)
        {
            s.clear().append("192.168.").append(n/256).append('.').append(n%256);
            hosts.push_back(s.str());
        }
        queryNamedGroupStore().add("daregress_group", hosts, true);
        Owned<IGroup> group = queryNamedGroupStore().lookup("daregress_group");
        ASSERT(queryNamedGroupStore().find(group,s.clear()) && "Created logical group not found");
        ASSERT(stricmp(s.str(),"daregress_group")==0 && "Created logical group found with wrong name");
        group.setown(queryNamedGroupStore().lookup("daregress_group"));
        ASSERT(group && "named group lookup failed");
        logctx.CTXLOG("Named group created    - 400 nodes");
        for (i=0;i<100;i++) {
            Owned<IPropertyTree> pp = createPTree("Part");
            Owned<IFileDescriptor>fdesc = createFileDescriptor();
            fdesc->setDefaultDir("thordata/regress");
            n = 9;
            for (unsigned k=0;k<400;k++) {
                s.clear().append("192.168.").append(n/256).append('.').append(n%256);
                Owned<INode> node = createINode(s.str());
                pp->setPropInt64("@size",(n*777+i)*recsize);
                s.clear().append("daregress_test").append(i).append("._").append(n+1).append("_of_400");
                fdesc->setPart(n,node,s.str(),pp);
                n = (n+9)%400;
            }
            fdesc->queryProperties().setPropInt("@recordSize",17);
            s.clear().append("daregress::test").append(i);
            removeLogical(s.str(), user);
            StringBuffer cname;
            Owned<IDistributedFile> dfile = dir.createNew(fdesc);
            ASSERT(stricmp(dfile->getClusterName(0,cname),"daregress_group")==0 && "Cluster name wrong");
            s.clear().append("daregress::test").append(i);
            dfile->attach(s.str(),user);
        }
        logctx.CTXLOG("DFile create done      - 100 files");
        unsigned samples = 5;
        t = 33;
        for (i=0;i<100;i++) {
            s.clear().append("daregress::test").append(t);
            ASSERT(dir.exists(s.str(),user) && "Could not find sub-file");
            Owned<IDistributedFile> dfile = dir.lookup(s.str(), user, AccessMode::tbdRead, false, false, nullptr, false);
            ASSERT(dfile && "Could not find sub-file");
            offset_t totsz = 0;
            n = 11;
            for (unsigned k=0;k<400;k++) {
                Owned<IDistributedFilePart> part = dfile->getPart(n);
                ASSERT(part && "part not found");
                s.clear().append("192.168.").append(n/256).append('.').append(n%256);
                Owned<INode> node = createINode(s.str());
                ASSERT(node->equals(part->queryNode()) && "part node mismatch");
                ASSERT(part->getFileSize(false,false)==(n*777+t)*recsize && "size node mismatch");
                s.clear().append("daregress_test").append(t).append("._").append(n+1).append("_of_400");
    /* ** TBD
                if (stricmp(s.str(),part->queryPartName())!=0)
                    ERROR4("part name mismatch %d, %d '%s' '%s'",t,n,s.str(),part->queryPartName());
    */
                totsz += (n*777+t)*recsize;
                if ((samples>0)&&(i+n+t==k)) {
                    samples--;
                    RemoteFilename rfn;
                    part->getFilename(rfn,samples%2);
                    StringBuffer fn;
                    rfn.getRemotePath(fn);
                    logctx.CTXLOG("SAMPLE: %d,%d %s",t,n,fn.str());
                }
                n = (n+11)%400;
            }
            ASSERT(totsz==dfile->getFileSize(false,false) && "total size mismatch");
            t = (t+33)%100;
        }
        logctx.CTXLOG("DFile lookup done      - 100 files");

        // check iteration
        __int64 crctot = 0;
        unsigned np = 0;
        unsigned totrows = 0;
        Owned<IDistributedFileIterator> fiter = dir.getIterator("daregress::*",false, user, defaultNonPrivilegedUser);
        Owned<IDistributedFilePartIterator> piter;
        ForEach(*fiter) {
            piter.setown(fiter->query().getIterator());
            ForEach(*piter) {
                RemoteFilename rfn;
                StringBuffer s;
                piter->query().getFilename(rfn,0);
                rfn.getRemotePath(s);
                piter->query().getFilename(rfn,1);
                rfn.getRemotePath(s);
                crctot += crc32(s.str(),s.length(),0);
                np++;
                totrows += (unsigned)(piter->query().getFileSize(false,false)/fiter->query().queryAttributes().getPropInt("@recordSize",-1));
            }
        }
        piter.clear();
        fiter.clear();
        logctx.CTXLOG("DFile iterate done     - %d parts, %d rows, CRC sum %" I64F "d",np,totrows,crctot);
        Owned<IDistributedSuperFile> sfile;
        sfile.setown(dir.createSuperFile("daregress::superfile1",user,true,false));
        for (i = 0;i<100;i++) {
            s.clear().append("daregress::test").append(i);
            sfile->addSubFile(s.str());
        }
        sfile.clear();
        sfile.setown(dir.lookupSuperFile("daregress::superfile1", user, AccessMode::readMeta));
        ASSERT(sfile && "Could not find added superfile");
        __int64 savcrc = crctot;
        crctot = 0;
        np = 0;
        totrows = 0;
        size32_t srs = (size32_t)sfile->queryAttributes().getPropInt("@recordSize",-1);
        ASSERT(srs==17 && "Superfile does not match subfile row size");
        piter.setown(sfile->getIterator());
        ForEach(*piter) {
            RemoteFilename rfn;
            StringBuffer s;
            piter->query().getFilename(rfn,0);
            rfn.getRemotePath(s);
            piter->query().getFilename(rfn,1);
            rfn.getRemotePath(s);
            crctot += crc32(s.str(),s.length(),0);
            np++;
            totrows += (unsigned)(piter->query().getFileSize(false,false)/srs);
        }
        piter.clear();
        logctx.CTXLOG("Superfile iterate done - %d parts, %d rows, CRC sum %" I64F "d",np,totrows,crctot);
        ASSERT(crctot==savcrc && "SuperFile does not match sub files");
        unsigned tr = (unsigned)(sfile->getFileSize(false,false)/srs);
        ASSERT(totrows==tr && "Superfile size does not match part sum");
        sfile->detach();
        sfile.clear();
        sfile.setown(dir.lookupSuperFile("daregress::superfile1", user, AccessMode::writeMeta));
        ASSERT(!sfile && "Superfile deletion failed");
        t = 37;
        for (i=0;i<100;i++) {
            s.clear().append("daregress::test").append(t);
            removeLogical(s.str(), user);
            t = (t+37)%100;
        }
        logctx.CTXLOG("DFile removal complete");
        t = 39;
        for (i=0;i<100;i++) {
            ASSERT(!dir.exists(s.str(),user) && "Found dir after deletion");
            Owned<IDistributedFile> dfile = dir.lookup(s.str(), user, AccessMode::tbdRead, false, false, nullptr, false);
            ASSERT(!dfile && "Found file after deletion");
            t = (t+39)%100;
        }
        logctx.CTXLOG("DFile removal check complete");
        queryNamedGroupStore().remove("daregress_group");
        ASSERT(!queryNamedGroupStore().lookup("daregress_group") && "Named group not removed");
    }
    void testFiles()
    {
        StringBuffer fn;
        const char *s = filelist;
        unsigned slowest = 0;
        StringAttr slowname;
        unsigned tot = 0;
        unsigned n = 0;
        while (*s) {
            fn.clear();
            while (*s==',')
                s++;
            while (*s&&(*s!=','))
                fn.append(*(s++));
            if (fn.length()) {
                n++;
                unsigned ss = msTick();
                checkFiles(fn);
                unsigned t = (msTick()-ss);
                if (t>slowest) {
                    slowest = t;
                    slowname.set(fn);
                }
                tot += t;
            }
        }
        printf("Complete in %ds avg %dms\n",tot/1000,tot/(n?n:1));
        if (!slowname.isEmpty())
            printf("Slowest %s = %dms\n",slowname.get(),slowest);
    }
    void testReadBranch(const char *path)
    {
        PROGLOG("Connecting to %s",path);
        Owned<IRemoteConnection> conn = querySDS().connect(path, myProcessSession(), RTM_LOCK_READ, 10000);
        ASSERT(conn && "Could not connect");
        IPropertyTree *root = conn->queryRoot();
        Owned<IAttributeIterator> aiter = root->getAttributes();
        StringBuffer s;
        ForEach(*aiter)
            aiter->getValue(s.clear());
        aiter.clear();
        root->getProp(NULL,s.clear());
        Owned<IPropertyTreeIterator> iter = root->getElements("*");
        StringAttrArray children;
        UnsignedArray childidx;
        ForEach(*iter) {
            children.append(*new StringAttrItem(iter->query().queryName()));
            childidx.append(root->queryChildIndex(&iter->query()));
        }
        iter.clear();
        conn.clear();
        ForEachItemIn(i,children) {
            s.clear().append(path);
            if (path[strlen(path)-1]!='/')
                s.append('/');
            s.append(children.item(i).text).append('[').append(childidx.item(i)+1).append(']');
            testReadBranch(s.str());
        }
    }
    /*
     * This test is silly and can take a very long time on clusters with
     * a large file-system. But keeping it here for further reference.
     * MORE: Maybe, this could be added to daliadmin or a thorough check
     * on the filesystem, together with super-file checks et al.
    void testReadAllSDS()
    {
        logctx.CTXLOG("Test SDS connecting to every branch");
        testReadBranch("/");
        logctx.CTXLOG("Connected to every branch");
    }
    */

};

CPPUNIT_TEST_SUITE_REGISTRATION( CDaliTestsStress );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( CDaliTestsStress, "CDaliTestsStress" );

// ================================================================================== UNIT TESTS

class CDaliSDSStressTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(CDaliSDSStressTests);
        CPPUNIT_TEST(testInit);
        CPPUNIT_TEST(testSDSRW);
        CPPUNIT_TEST(testSDSSubs);
        CPPUNIT_TEST(testSDSSubs2);
        CPPUNIT_TEST(testSDSNodeSubs);
        CPPUNIT_TEST(testEphemeralLocks);
        CPPUNIT_TEST(testSiblingPerfLocal);
        CPPUNIT_TEST(testSiblingPerfDali);
        CPPUNIT_TEST(testSiblingPerfContention);
    CPPUNIT_TEST_SUITE_END();

    const IContextLogger &logctx;

    static const unsigned numCChangeTests = 1;
    static const unsigned subsPerCChange = 10;
    static const unsigned numCChangeCommits = 10;

    void sdsNodeCommit(const char *test, unsigned from, unsigned to, bool finalDelete)
    {
        class CNodeSubCommitThread : public CInterface, implements IThreaded
        {
            StringAttr xpath;
            bool finalDelete;
            CThreaded threaded;
        public:
            IMPLEMENT_IINTERFACE;

            CNodeSubCommitThread(const char *_xpath, bool _finalDelete) : xpath(_xpath), finalDelete(_finalDelete), threaded("CNodeSubCommitThread")
            {
            }
            virtual void threadmain() override
            {
                unsigned mode = RTM_LOCK_WRITE;
                if (finalDelete)
                    mode |= RTM_DELETE_ON_DISCONNECT;
                Owned<IRemoteConnection> conn = querySDS().connect(xpath, myProcessSession(), mode, 1000000);
                assertex(conn);
                for (unsigned i=0; i<5; i++)
                {
                    VStringBuffer val("newval%d", i+1);
                    conn->queryRoot()->setProp(NULL, val.str());
                    conn->commit();
                }
                conn->queryRoot()->setProp("subnode", "newval");
                conn->commit();
                conn.clear(); // if finalDelete=true, deletes subscribed node in process, should get notificaiton

            }
            void start()
            {
                threaded.init(this, false);
            }
            void join()
            {
                threaded.join();
            }
        };
        CIArrayOf<CNodeSubCommitThread> commitThreads;
        for (unsigned i=from; i<=to; i++)
        {
            VStringBuffer path("/DAREGRESS/NodeSubTest/node%d", i);
            CNodeSubCommitThread *commitThread = new CNodeSubCommitThread(path, finalDelete);
            commitThreads.append(* commitThread);
        }
        ForEachItemIn(t, commitThreads)
            commitThreads.item(t).start();
        ForEachItemIn(t2, commitThreads)
            commitThreads.item(t2).join();
    }
    unsigned fn(unsigned n, unsigned m, unsigned seed, unsigned depth, IPropertyTree *parent)
    {
        __int64 val = parent->getPropInt64("val",0);
        parent->setPropInt64("val",n+val);
        val = parent->getPropInt64("@val",0);
        parent->setPropInt64("@val",m+val);
        val = parent->getPropInt64(NULL,0);
        parent->setPropInt64(NULL,seed+val);
        if (Rconn&&((n+m+seed)%100==0))
            Rconn->commit();
        if (!seed)
            return m+n;
        if (n==m)
            return seed;
        if (depth>10)
            return seed+n+m;
        if (seed%7==n%7)
            return n;
        if (seed%7==m%7)
            return m;
        char name[64];
        unsigned v = seed;
        name[0] = 's';
        name[1] = 'u';
        name[2] = 'b';
        unsigned i = 3;
        while (v) {
            name[i++] = ('A'+v%26 );
            v /= 26;
        }
        name[i] = 0;
        IPropertyTree *child = parent->queryPropTree(name);
        if (!child)
            child = parent->addPropTree(name, createPTree(name));
        return fn(fn(n,seed,seed*17+11,depth+1,child),fn(seed,m,seed*11+17,depth+1,child),seed*19+7,depth+1,child);
    }

    unsigned fn2(unsigned n, unsigned m, unsigned seed, unsigned depth, StringBuffer &parentname)
    {
        if (!Rconn)
            return 0;
        if ((n+m+seed)%25==0) {
            Rconn->commit();
            Rconn->Release();
            Rconn = querySDS().connect("/DAREGRESS",myProcessSession(), 0, 1000000);
            ASSERT(Rconn && "Failed to connect to /DAREGRESS");
        }
        IPropertyTree *parent = parentname.length()?Rconn->queryRoot()->queryPropTree(parentname.str()):Rconn->queryRoot();
        ASSERT(parent && "Failed to connect to parent");
        __int64 val = parent->getPropInt64("val",0);
        parent->setPropInt64("val",n+val);
        val = parent->getPropInt64("@val",0);
        parent->setPropInt64("@val",m+val);
        val = parent->getPropInt64(NULL,0);
        parent->setPropInt64(NULL,seed+val);
        if (!seed)
            return m+n;
        if (n==m)
            return seed;
        if (depth>10)
            return seed+n+m;
        if (seed%7==n%7)
            return n;
        if (seed%7==m%7)
            return m;
        char name[64];
        unsigned v = seed;
        name[0] = 's';
        name[1] = 'u';
        name[2] = 'b';
        unsigned i = 3;
        while (v) {
            name[i++] = ('A'+v%26 );
            v /= 26;
        }
        name[i] = 0;
        unsigned l = parentname.length();
        if (parentname.length())
            parentname.append('/');
        parentname.append(name);
        IPropertyTree *child = parent->queryPropTree(name);
        if (!child)
            child = parent->addPropTree(name, createPTree(name));
        unsigned ret = fn2(fn2(n,seed,seed*17+11,depth+1,parentname),fn2(seed,m,seed*11+17,depth+1,parentname),seed*19+7,depth+1,parentname);
        parentname.setLength(l);
        return ret;
    }
public:
    CDaliSDSStressTests() : logctx(queryDummyContextLogger())
    {
    }
    ~CDaliSDSStressTests()
    {
        daliClientEnd();
    }
    void testInit()
    {
        daliClientInit();
    }
    void testSDSRW()
    {
        Owned<IPropertyTree> ref = createPTree("DAREGRESS");
        fn(1,2,3,0,ref);
        StringBuffer refstr;
        toXML(ref,refstr,0,XML_SortTags|XML_Format);
        logctx.CTXLOG("Created reference size %d",refstr.length());
        Owned<IRemoteConnection> conn = querySDS().connect("/DAREGRESS",myProcessSession(), RTM_CREATE, 1000000);
        Rconn = conn;
        IPropertyTree *root = conn->queryRoot();
        fn(1,2,3,0,root);
        conn.clear();
        logctx.CTXLOG("Created test branch 1");
        conn.setown(querySDS().connect("/DAREGRESS",myProcessSession(), RTM_DELETE_ON_DISCONNECT, 1000000));
        root = conn->queryRoot();
        StringBuffer s;
        toXML(root,s,0,XML_SortTags|XML_Format);
        ASSERT(strcmp(s.str(),refstr.str())==0 && "Branch 1 does not match");
        conn.clear();
        conn.setown(querySDS().connect("/DAREGRESS",myProcessSession(), 0, 1000000));
        ASSERT(!conn && "RTM_DELETE_ON_DISCONNECT failed");
        Rconn = querySDS().connect("/DAREGRESS",myProcessSession(), RTM_CREATE, 1000000);
        StringBuffer pn;
        fn2(1,2,3,0,pn);
        ::Release(Rconn);
        logctx.CTXLOG("Created test branch 2");
        Rconn = NULL;
        conn.setown(querySDS().connect("/DAREGRESS",myProcessSession(), RTM_DELETE_ON_DISCONNECT, 1000000));
        root = conn->queryRoot();
        toXML(root,s.clear(),0,XML_SortTags|XML_Format);
        ASSERT(strcmp(s.str(),refstr.str())==0 && "Branch 2 does not match");
        conn.clear();
        conn.setown(querySDS().connect("/DAREGRESS",myProcessSession(), 0, 1000000));
        ASSERT(!conn && "RTM_DELETE_ON_DISCONNECT failed");
    }
    void testSDSSubs()
    {
        class CChange : public Thread
        {
            class CCSub : public CInterface, implements ISDSConnectionSubscription, implements ISDSSubscription
            {
                unsigned n;
                unsigned &count;
            public:
                IMPLEMENT_IINTERFACE;

                CCSub(unsigned _n,unsigned &_count)
                    : count(_count)
                {
                    n = _n;
                }
                virtual void notify()
                {
                    CriticalBlock block(subchangesect);
                    subchangetotal += n;
                    subchangenum++;
                    count++;
                }
                virtual void notify(SubscriptionId id, const char *xpath, SDSNotifyFlags flags, unsigned valueLen, const void *valueData)
                {
                    CriticalBlock block(subchangesect);
                    subchangetotal += n;
                    subchangenum++;
                    subchangetotal += (unsigned)flags;
                    subchangetotal += crc32(xpath,strlen(xpath),0);
                    if (valueLen)
                        subchangetotal += crc32((const char *)valueData,valueLen,0);
                    count++;

                }

            };
            Owned<IRemoteConnection> conn;
            StringAttr path;
            SubscriptionId id[10];
            unsigned n;
            unsigned count;

        public:
            Semaphore stopsem;
            CChange(unsigned _n)
            {
                n = _n;
                StringBuffer s("/DAREGRESS/CONSUB");
                s.append(n+1);
                path.set(s.str());
                conn.setown(querySDS().connect(path, myProcessSession(), RTM_CREATE|RTM_DELETE_ON_DISCONNECT, 1000000));
                unsigned i;
                for (i=0;i<subsPerCChange/2;i++)
                {
                    Owned<CCSub> sub = new CCSub(n*subsPerCChange+i,count);
                    id[i] = conn->subscribe(*sub);
                }
                s.append("/testprop");
                for (;i<subsPerCChange;i++)
                {
                    Owned<CCSub> sub = new CCSub(n*subsPerCChange+i,count);
                    id[i] = querySDS().subscribe(s.str(),*sub,false,true);
                }
                count = 0;
                start(false);
            }

            virtual int run()
            {
                unsigned i;
                for (i = 0;i<numCChangeCommits; i++) {
                    conn->queryRoot()->setPropInt("testprop", (i*17+n*21)%100);
                    conn->commit();
                    for (unsigned j=0;j<1000;j++) {
                        {
                            CriticalBlock block(subchangesect);
                            if (count>=(i+1)*10)
                                break;
                        }
                        Sleep(10);
                    }
                }
                stopsem.wait();
                for (i=0;i<subsPerCChange/2;i++)
                    conn->unsubscribe(id[i]);
                for (;i<subsPerCChange;i++)
                    querySDS().unsubscribe(id[i]);

                return 0;
            }
        };
        subchangenum = 0;
        subchangetotal = 0;
        IArrayOf<CChange> a;
        for (unsigned i=0; i<numCChangeTests ; i++)
            a.append(*new CChange(i));
        unsigned last = 0;
        for (;;)
        {
            Sleep(1000);
            {
                CriticalBlock block(subchangesect);
                if (subchangenum==last)
                    break;
                last = subchangenum;
            }
        }
        ForEachItemIn(i1, a)
            a.item(i1).stopsem.signal();
        ForEachItemIn(i2, a)
            a.item(i2).join();
        logctx.CTXLOG("%d subscription notifications, check sum = %" I64F "d",subchangenum,subchangetotal);
        ASSERT(subchangenum==( numCChangeTests * subsPerCChange * numCChangeCommits) && "Not all notifications received");
    }
    void testSDSSubs2()
    {
        class CResult
        {
            Semaphore sem;
            StringBuffer resultString;
            CriticalSection crit;
        public:
            CResult()
            {
            }
            void add(const char *message)
            {
                CriticalBlock b(crit);
                if (resultString.length())
                    resultString.append("|");
                resultString.append(message);
                sem.signal();
            }
            Semaphore &querySem() { return sem; }
            void clear() { resultString.clear(); }
            bool wait(unsigned numExpected)
            {
                for (unsigned t=0; t<numExpected; t++)
                {
                    if (!sem.wait(5000))
                        return false;
                }
                return true;
            }
            StringBuffer &getResultsClear(StringBuffer &ret)
            {
                StringArray array;
                array.appendList(resultString, "|");
                resultString.clear();
                array.sortAscii();
                ForEachItemIn(r, array)
                {
                    if (ret.length())
                        ret.append("|");
                    ret.append(array.item(r));
                }
                return ret;
            }
        } result;
        class CSubscriberContainer : public CInterface
        {
            class CSubscriber : public CSimpleInterfaceOf<ISDSSubscription>
            {
                StringAttr xpath;
                bool sub;
                CResult &result;
            public:
                CSubscriber(CResult &_result, const char *_xpath, bool _sub) : xpath(_xpath), sub(_sub), result(_result)
                {
                }
                virtual void notify(SubscriptionId id, const char *_xpath, SDSNotifyFlags flags, unsigned valueLen, const void *valueData)
                {
                    PROGLOG("CSubscriber notified path=%s for subscriber=%s, sub=%s", _xpath, xpath.get(), sub?"true":"false");
                    StringBuffer message(xpath);
                    if (!sub && valueLen)
                        message.append(",").append(valueLen, (const char *)valueData);
                    result.add(message);
                }
            };
            Owned<CSubscriber> subscriber;
            SubscriptionId id;
        public:
            CSubscriberContainer(CResult &result, const char *xpath, bool sub)
            {
                subscriber.setown(new CSubscriber(result, xpath, sub));
                id = querySDS().subscribe(xpath, *subscriber, sub, !sub);
                PROGLOG("Subscribed to %s", xpath);
            }
            virtual void beforeDispose() override
            {
                querySDS().unsubscribe(id);
            }
        };
        Owned<IRemoteConnection> conn = querySDS().connect("/", myProcessSession(), RTM_LOCK_WRITE, INFINITE);
        IPropertyTree *root = conn->queryRoot();
        IPropertyTree *daRegress = root->setPropTree("DAREGRESS");
        Owned<IPropertyTree> tree = createPTreeFromXMLString("<TestSub2><a><b1><c/></b1><b2/><b3><d><e/></d></b3><b4><f/></b4></a></TestSub2>");
        daRegress->setPropTree("TestSub2", tree.getClear());
        conn->commit();

        Owned<CSubscriberContainer> sub1 = new CSubscriberContainer(result, "/DAREGRESS/TestSub2/a", true);
        Owned<CSubscriberContainer> sub2 = new CSubscriberContainer(result, "/DAREGRESS/TestSub2/a/b1", false);
        Owned<CSubscriberContainer> sub3 = new CSubscriberContainer(result, "/DAREGRESS/TestSub2/a/b2", false);
        Owned<CSubscriberContainer> sub4 = new CSubscriberContainer(result, "/DAREGRESS/TestSub2/a/b1/c", false);
        Owned<CSubscriberContainer> sub5 = new CSubscriberContainer(result, "/DAREGRESS/TestSub2/a/b3", true);
        Owned<CSubscriberContainer> sub6 = new CSubscriberContainer(result, "/DAREGRESS/TestSub2/a/b4", false);
        Owned<CSubscriberContainer> sub7 = new CSubscriberContainer(result, "/DAREGRESS/TestSub2/a/b4/sub", false);

        StringArray expectedResults;
        expectedResults.append("/DAREGRESS/TestSub2/a");
        expectedResults.append("/DAREGRESS/TestSub2/a|/DAREGRESS/TestSub2/a/b1,testv");
        expectedResults.append("/DAREGRESS/TestSub2/a|/DAREGRESS/TestSub2/a/b2,testv");
        expectedResults.append("/DAREGRESS/TestSub2/a|/DAREGRESS/TestSub2/a/b1/c,testv");
        expectedResults.append("/DAREGRESS/TestSub2/a|/DAREGRESS/TestSub2/a/b1,testv");
        expectedResults.append("/DAREGRESS/TestSub2/a|/DAREGRESS/TestSub2/a/b2,testv");
        expectedResults.append("/DAREGRESS/TestSub2/a");
        expectedResults.append("/DAREGRESS/TestSub2/a|/DAREGRESS/TestSub2/a/b3");
        expectedResults.append("/DAREGRESS/TestSub2/a|/DAREGRESS/TestSub2/a/b3");
        expectedResults.append("/DAREGRESS/TestSub2/a|/DAREGRESS/TestSub2/a/b4,b4value|/DAREGRESS/TestSub2/a/b4/sub,subvalue");

        StringArray props;
        props.appendList("S:TestSub2/a,S:TestSub2/a/b1,S:TestSub2/a/b2,S:TestSub2/a/b1/c,S:TestSub2/a/b1/d,S:TestSub2/a/b2/e,S:TestSub2/a/b2/e/f,D:TestSub2/a/b3/d/e,D:TestSub2/a/b3/d,R:TestSub2/a/b4", ",");

        assertex(expectedResults.ordinality() == props.ordinality());

        ForEachItemIn(p, props)
        {
            const char *cmd = props.item(p);
            const char *propPath=cmd+2;
            switch (*cmd)
            {
                case 'S':
                {
                    PROGLOG("Changing %s", propPath);
                    daRegress->setProp(propPath, "testv");
                    break;
                }
                case 'D':
                {
                    PROGLOG("Deleting %s", propPath);
                    daRegress->removeProp(propPath);
                    break;
                }
                case 'R':
                {
                    PROGLOG("Replacing tree %s", propPath);
                    Owned<IPropertyTree> tree = createPTreeFromXMLString("<b4><sub><g/>subvalue</sub>b4value</b4>");
                    daRegress->setPropTree(propPath, tree.getClear());
                    break;
                }
                default:
                    throwUnexpected();
            }
            conn->commit();

            const char *expectedResult = expectedResults.item(p);
            StringArray expectedResultArray;
            expectedResultArray.appendList(expectedResult, "|"); // just to get #
            if (!result.wait(expectedResultArray.ordinality()))
            {
                VStringBuffer errMsg("Timeout waiting for subcription notificaitons, where expected result = '%s',", expectedResult);
                CPPUNIT_ASSERT_MESSAGE(errMsg.str(), 0);
            }
            StringBuffer results;
            result.getResultsClear(results);
            PROGLOG("Checking results");
            if (0 == strcmp(expectedResult, results))
                PROGLOG("testSDSSubs2 [ %s ]: MATCH", cmd);
            else
            {
                VStringBuffer errMsg("testSDSSubs2 [ %s ]: MISMATCH", cmd);
                errMsg.newline().append("Expected: ").append(expectedResult);
                errMsg.newline().append("Got: ").append(results);
                PROGLOG("%s", errMsg.str());
                CPPUNIT_ASSERT_MESSAGE(errMsg.str(), 0);
            }
        }
    }
    void testSDSNodeSubs()
    {
        class CResults
        {
            StringArray results;
            CRC32 crc;
            CriticalSection crit;
        public:
            void add(const char *out)
            {
                PROGLOG("%s", out);
                CriticalBlock b(crit); // notify() and therefore add() can be called on multiple threads
                results.append(out);
            }
            unsigned getCRC()
            {
                results.sortAscii();
                ForEachItemIn(r, results)
                {
                    const char *result = results.item(r);
                    crc.tally(strlen(result), result);
                }
                PROGLOG("CRC = %x", crc.get());
                results.kill();
                return crc.get();
            }
        };
        class CSubscriber : CSimpleInterface, implements ISDSNodeSubscription
        {
            StringAttr path;
            CResults &results;
            unsigned expectedNotifications;
            std::atomic<unsigned> notifications = {0};
            Semaphore joinSem;
        public:
            IMPLEMENT_IINTERFACE_USING(CSimpleInterface);

            CSubscriber(const char *_path, CResults &_results, unsigned _expectedNotifications)
                : path(_path), results(_results), expectedNotifications(_expectedNotifications)
            {
                if (0 == expectedNotifications)
                    joinSem.signal();
            }
            virtual void notify(SubscriptionId id, SDSNotifyFlags flags, unsigned valueLen, const void *valueData)
            {
                StringAttr value;
                if (valueLen)
                    value.set((const char *)valueData, valueLen);
                VStringBuffer res("Subscriber(%s): flags=%d, value=%s", path.get(), flags, 0==valueLen ? "(none)" : value.get());
                results.add(res);
                if (++notifications == expectedNotifications)
                    joinSem.signal();
            }
            void join()
            {
                if (joinSem.wait(5000))
                {
                    MilliSleep(100); // wait a bit, see if get more than expected
                    unsigned n = notifications;
                    if (n == expectedNotifications)
                    {
                        VStringBuffer out("Subscriber(%s): %d notifications received", path.get(), n);
                        results.add(out);
                        return;
                    }
                }
                VStringBuffer out("Expected %d notifications, received %d", expectedNotifications, notifications.load());
                results.add(out);
            }
        };
        // setup
        Owned<IRemoteConnection> conn = querySDS().connect("/DAREGRESS/NodeSubTest", myProcessSession(), RTM_CREATE, 1000000);
        IPropertyTree *root = conn->queryRoot();
        unsigned i, ai;
        for (i=0; i<10; i++)
        {
            VStringBuffer name("node%d", i+1);
            IPropertyTree *sub = root->setPropTree(name, createPTree());
            for (ai=0; ai<2; ai++)
            {
                VStringBuffer name("@attr%d", i+1);
                VStringBuffer val("val%d", i+1);
                sub->setProp(name, val);
            }
        }
        conn.clear();

        CResults results;

        {
            const char *testPath = "/DAREGRESS/NodeSubTest/doesnotexist";
            Owned<CSubscriber> subscriber = new CSubscriber(testPath, results, 0);
            try
            {
                querySDS().subscribeExact(testPath, *subscriber, true);
                throwUnexpected();
            }
            catch(IException *e)
            {
                if (SDSExcpt_SubscriptionNoMatch != e->errorCode())
                    throw;
                results.add("Correctly failed to add subscriber to non-existent node.");
            }
            subscriber.clear();
        }

        {
            const char *testPath = "/DAREGRESS/NodeSubTest/node1";
            Owned<CSubscriber> subscriber = new CSubscriber(testPath, results, 2*5+1+1);
            SubscriptionId id = querySDS().subscribeExact(testPath, *subscriber, false);

            sdsNodeCommit(testPath, 1, 1, false);
            sdsNodeCommit(testPath, 1, 1, true); // will delete 'node1'

            subscriber->join();
            querySDS().unsubscribeExact(id); // will actually be a NOP, as will be already unsubscribed when 'node1' deleted.
        }

        {
            const char *testPath = "/DAREGRESS/NodeSubTest/node*";
            Owned<CSubscriber> subscriber = new CSubscriber(testPath, results, 9*6);
            SubscriptionId id = querySDS().subscribeExact(testPath, *subscriber, false);

            sdsNodeCommit(testPath, 2, 10, false);

            subscriber->join();
            querySDS().unsubscribeExact(id);
        }

        {
            UInt64Array subscriberIds;
            IArrayOf<CSubscriber> subscribers;
            for (i=2; i<=10; i++) // NB: from 2, as 'node1' deleted in previous tests
            {
                for (ai=0; ai<2; ai++)
                {
                    VStringBuffer path("/DAREGRESS/NodeSubTest/node%d[@attr%d=\"val%d\"]", i, i, i);
                    Owned<CSubscriber> subscriber = new CSubscriber(path, results, 11);
                    SubscriptionId id = querySDS().subscribeExact(path, *subscriber, 0==ai);
                    subscribers.append(* subscriber.getClear());
                    subscriberIds.append(id);
                }
            }
            const char *testPath = "/DAREGRESS/NodeSubTest/node*";
            Owned<CSubscriber> subscriber = new CSubscriber(testPath, results, 9*5+9*(5+1));
            SubscriptionId id = querySDS().subscribeExact(testPath, *subscriber, false);

            sdsNodeCommit(testPath, 2, 10, false);
            sdsNodeCommit(testPath, 2, 10, true);

            subscriber->join();
            querySDS().unsubscribeExact(id);
            ForEachItemIn(s, subscriberIds)
            {
                subscribers.item(s).join();
                querySDS().unsubscribeExact(subscriberIds.item(s));
            }
        }

        ASSERT(0xa68e2324 == results.getCRC() && "SDS Node notifcation differences");
    }
    void testEphemeralLocks()
    {
        auto createEphemeralLock = [&](const char *xpath, unsigned timeout, unsigned type)
        {
            unsigned mode = RTM_LOCK_WRITE | RTM_CREATE_QUERY | RTM_DELETE_ON_DISCONNECT;
            Owned<IRemoteConnection> conn = querySDS().connect(xpath, myProcessSession(), mode, timeout);
            conn->commit();
        };

        unsigned numThreads = 100;
        std::vector<std::future<void>> results;
        const char *xpath = "/Locks/TestEphemeralLock";
        unsigned timeout = 2000;
        for (unsigned t=0; t<numThreads; t++)
            results.push_back(std::async(std::launch::async, createEphemeralLock, xpath, timeout, t%2));
        for (auto &f: results)
            f.get();
    }
    void createLevel(IPropertyTree *parent, unsigned nodeSiblings, unsigned leafSiblings, unsigned attributes, unsigned depth, unsigned level)
    {
        StringBuffer aname;
        StringBuffer avalue;
        if (2 == level) // 1st level down
        {
            printf(".");
            fflush(stdout);
        }
        unsigned levelSiblings = depth==level ? leafSiblings : nodeSiblings;
        for (unsigned s=0; s<levelSiblings; s++)
        {
            IPropertyTree *child = parent->addPropTree("Child");
            if (level<depth)
                createLevel(child, nodeSiblings, leafSiblings, attributes, depth, level+1);
            for (unsigned a=0; a<attributes; a++)
            {
                avalue.clear().appendf("%u_%u", level, s+1);
                child->setProp(aname.clear().appendf("@aname%u", a+1), avalue.str());
            }
        }
        if (1 == level) // back at top
            printf("\n");
    }
    static StringBuffer &constructXPath(StringBuffer &xpath, unsigned depth, unsigned nodeSibling, unsigned leafSibling, unsigned attr, unsigned level, const char *extra=nullptr)
    {
        while (true)
        {
            unsigned sibling = depth == level ? leafSibling : nodeSibling;
            xpath.appendf("Child[@aname%u=\"%u_%u", attr, level, sibling);
            if (extra && (depth == level))
                xpath.append(extra);
            xpath.append("\"]");
            if (level == depth)
                break;
            xpath.append('/');
            level++;
        }
        return xpath;
    }

    void createSiblings(IPropertyTree *root, unsigned depth, unsigned attributes, unsigned nodeSiblings, unsigned leafSiblings)
    {
        unsigned nodes = 0;
        for (unsigned d=1; d<=depth; d++)
        {
            if (d==depth)
                nodes += pow(nodeSiblings, d-1) * leafSiblings;
            else
                nodes += pow(nodeSiblings, d);
        }
        printf("Creating %u nodes\n", nodes);

        CCycleTimer timer;
        createLevel(root, nodeSiblings, leafSiblings, attributes, depth, 1);
        printf("%6u ms : create time\n", timer.elapsedMs());
    }

    void testSiblingPerf(std::function<IPropertyTree *(StringBuffer &, unsigned, unsigned, unsigned, unsigned, const char *)> searchFunc, unsigned depth, unsigned attributes, unsigned nodeSiblings, unsigned leafSiblings, unsigned secondaryTests)
    {
        try
        {
            StringBuffer xpath;
            StringBuffer v;
            xpath.ensureCapacity(1024);
            v.ensureCapacity(1024);

            auto firstSearchFunc = [&](unsigned attr)
            {
                CCycleTimer timer;
                Owned<IPropertyTree> search = searchFunc(xpath.clear(), attr, depth, nodeSiblings, leafSiblings, nullptr);
                assertex(search);
                printf("%6u ms : 1st search (xpath=%s) time\n", timer.elapsedMs(), xpath.str());
                xpath.clear().appendf("@aname%u", attributes);
                v.clear().appendf("%u_%u", depth, leafSiblings);
                assertex(streq(v, search->queryProp(xpath)));
            };

            auto secondarySearchFunc = [&](unsigned attr, const char *extra=nullptr, unsigned siblingOffset=0)->unsigned
            {
                unsigned leafSibling = siblingOffset+leafSiblings-1;
                unsigned foundCount = 0;
                CCycleTimer timer;
                for (unsigned t=0; t<secondaryTests; t++)
                {
                    Owned<IPropertyTree> search = searchFunc(xpath.clear(), attr, depth, nodeSiblings, leafSibling, extra);
                    if (search)
                        foundCount++;
                    leafSibling--;
                    if (siblingOffset == leafSibling)
                        leafSibling = siblingOffset+leafSiblings;
                }
                VStringBuffer msg("%6u ms : Next ", timer.elapsedMs());
                msg.append(secondaryTests).append(" searches for aname").append(attr);
                if (extra)
                    msg.append(" [extra=\"").append(extra).append("\"]");
                msg.append(" time");
                printf("%s\n", msg.str());
                return foundCount;
            };

            firstSearchFunc(1); // first attribute

            verifyex(secondaryTests == secondarySearchFunc(1)); // first attribute

            firstSearchFunc(attributes); // 1st attribute
            verifyex(secondaryTests == secondarySearchFunc(attributes)); // last attribute
            verifyex(secondaryTests == secondarySearchFunc(1)); // first attribute

            Owned<IPropertyTree> parent = searchFunc(xpath.clear(), 1, depth-1, nodeSiblings, nodeSiblings, nullptr);
            verifyex(parent);
            CCycleTimer timer;
            unsigned max = leafSiblings;
            if (max > 1000)
                max = 1000;
            unsigned removedEntries = 0, newEntries = 0, changedEntries = 0;
            unsigned step = leafSiblings / max;
            unsigned s = 1;
            unsigned which = 0;
            while (true)
            {
                constructXPath(xpath.clear(), depth, nodeSiblings, s, 1, depth);
                IPropertyTree *search = parent->queryPropTree(xpath);
                assertex(search);
                switch (which)
                {
                    case 0:
                    {
                        verifyex(parent->removeTree(search));
                        removedEntries++;
                        break;
                    }
                    case 1:
                    {
                        IPropertyTree *newChild = parent->addPropTree("Child");
                        newChild->setProp("@aname1", "NEW");
                        newEntries++;
                        break;
                    }
                    case 2:
                    {
                        search->setProp("@aname1", v.clear().appendf("%u_%u - CHANGED", depth, s));
                        changedEntries++;
                        break;
                    }
                }
                s += step;
                if (s >= leafSiblings)
                    break;
                ++which;
                if (which>2)
                    which = 0;
            }
            printf("%6u ms : Modify, delete and create elements time\n", timer.elapsedMs());

            parent.clear(); // if Dali test, then this will commit the above changes
            parent.setown(searchFunc(xpath.clear(), 1, depth-1, nodeSiblings, nodeSiblings, nullptr));

            timer.reset();
            Owned<IPropertyTreeIterator> iter = parent->getElements(xpath.clear().append("Child[@aname1=\"NEW\"]"));
            unsigned count = 0;
            ForEach (*iter)
                ++count;
            assertex(count == newEntries);
            printf("%6u ms : Scan of new entries time\n", timer.elapsedMs());

            s = 1;
            which = 0;
            timer.reset();
            while (true)
            {
                if (which==2)
                {
                    Owned<IPropertyTree> search = searchFunc(xpath.clear(), 1, depth, nodeSiblings, s, " - CHANGED");
                    assertex(search);
                }
                s += step;
                if (s >= leafSiblings)
                    break;
                which++;
                if (which>2)
                    which = 0;
            }
            printf("%6u ms : scans for %u changed entries time\n", timer.elapsedMs(), changedEntries);
        }
        catch (IException *e)
        {
            EXCLOG(e, nullptr);
            e->Release();
        }
    }
    void testSiblingPerfLocal()
    {
        unsigned depth = 3;
        unsigned attributes = 3;
        unsigned nodeSiblings = 10;
        unsigned leafSiblings = 100000;
        unsigned secondaryTests = 1000;
        unsigned mappingThreshold = 10;

        printf("Performing testSiblingPerfLocal\n\n");

        Owned<IPropertyTree> root = createPTree();
        auto searchFunc = [&root](StringBuffer &xpath, unsigned attr, unsigned depth, unsigned nodeSiblings, unsigned leafSibling, const char *extra=nullptr)->IPropertyTree *
        {
            constructXPath(xpath.clear(), depth, nodeSiblings, leafSibling, attr, 1, extra);
            return root->getPropTree(xpath.str());
        };

        createSiblings(root, depth, attributes, nodeSiblings, leafSiblings);

        printf("cloning tree for 2nd test\n");
        Owned<IPropertyTree> copyRoot = createPTreeFromIPT(root);

        setPTreeMappingThreshold(0); // disable
        printf("Performing tests with mapping disabled\n");
        testSiblingPerf(searchFunc, depth, attributes, nodeSiblings, leafSiblings, secondaryTests);

        root.setown(copyRoot.getClear());

        setPTreeMappingThreshold(mappingThreshold);
        printf("Performing tests with mapping enabled (mappingThreshold=%u)\n", mappingThreshold);
        testSiblingPerf(searchFunc, depth, attributes, nodeSiblings, leafSiblings, secondaryTests);
    }

    void testSiblingPerfDali()
    {
        unsigned depth = 3;
        unsigned attributes = 3;
        unsigned nodeSiblings = 2;
        unsigned leafSiblings = 100000;
        unsigned secondaryTests = 100;
        unsigned mappingThreshold = 10;

        printf("Performing testSiblingPerfDali\n\n");

        setPTreeMappingThreshold(mappingThreshold);

        Owned<IRemoteConnection> conn = querySDS().connect("/testmaps", myProcessSession(), RTM_CREATE, 10000);
        createSiblings(conn->queryRoot(), depth, attributes, nodeSiblings, leafSiblings);
        conn.clear(); // commit

        auto searchFunc = [](StringBuffer &xpath, unsigned attr, unsigned depth, unsigned nodeSiblings, unsigned leafSibling, const char *extra=nullptr)->IPropertyTree *
        {
            constructXPath(xpath.clear().append("/testmaps/"), depth, nodeSiblings, leafSibling, attr, 1, extra);
            Owned<IRemoteConnection> conn = querySDS().connect(xpath.str(), myProcessSession(), 0, 10000);
            assertex(conn);
            return conn->getRoot();
        };

        printf("Performing tests with both client and server side mapping enabled (mappingThreshold=%u)\n", mappingThreshold);
        testSiblingPerf(searchFunc, depth, attributes, nodeSiblings, leafSiblings, secondaryTests);
    }

    void testSiblingPerfContention()
    {
        unsigned depth = 1;
        unsigned attributes = 10;
        unsigned nodeSiblings = 1;
        unsigned leafSiblings = 1000;
        unsigned mappingThreshold = 10;
        unsigned threads = 20;

        printf("Performing testSiblingPerfContention\n\n");

        Owned<IPropertyTree> root = createPTree();
        createSiblings(root, depth, attributes, nodeSiblings, leafSiblings);

        setPTreeMappingThreshold(mappingThreshold);

        std::vector<std::future<void>> results;

        auto searchFunc = [&root](unsigned a)
        {
            /*
             * NB: initially the 1st lookups are likely to clash and only 1 will create the initial map.
             * i.e. the other threads will not use the map.
             */
            StringBuffer xpath;
            for (unsigned s=1; s<=1000; s++)
            {
                constructXPath(xpath.clear(), 1, 1, s, a, 1, nullptr);
                IPropertyTree *search = root->queryPropTree(xpath);
                assertex(search);
            }
        };

        for (unsigned t=0; t<threads; t++)
        {
            unsigned a = (t % (threads/2))+1;
            results.push_back(std::async(std::launch::async, searchFunc, a));
        }
        for (auto &f: results)
            f.get();
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION( CDaliSDSStressTests );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( CDaliSDSStressTests, "CDaliSDSStressTests" );

// ================================================================================== UNIT TESTS

static IFileDescriptor *createDescriptor(const char* dir, const char* name, unsigned parts, unsigned recSize, unsigned index=0)
{
    Owned<IPropertyTree> pp = createPTree("Part");
    Owned<IFileDescriptor>fdesc = createFileDescriptor();
    fdesc->setDefaultDir(dir);
    StringBuffer s;
    SocketEndpoint ep;
    ep.setLocalHost(0);
    StringBuffer ip;
    ep.getHostText(ip);
    for (unsigned k=0;k<parts;k++) {
        s.clear().append(ip);
        Owned<INode> node = createINode(s.str());
        pp->setPropInt64("@size",recSize);
        s.clear().append(name);
        if (index)
            s.append(index);
        s.append("._").append(k+1).append("_of_").append(parts);
        fdesc->setPart(k,node,s.str(),pp);
    }
    fdesc->queryProperties().setPropInt("@recordSize",recSize);
    fdesc->setDefaultDir(dir);
    return fdesc.getClear();
}

static void setupDFS(const IContextLogger &logctx, const char *scope, unsigned supersToDel=3, unsigned subsToCreate=4)
{
    StringBuffer bufScope;
    bufScope.append("regress::").append(scope);
    StringBuffer bufDir;
    bufDir.append("regress/").append(scope);

    logctx.CTXLOG("Cleaning up '%s' scope", bufScope.str());
    for (unsigned i=1; i<=supersToDel; i++) {
        StringBuffer super(bufScope);
        super.append("::super").append(i);
        if (dir.exists(super.str(),user,false,true))
            ASSERT(dir.removeEntry(super.str(), user) && "Can't remove super-file");
    }

    logctx.CTXLOG("Creating 'regress::trans' subfiles(1,%d)", subsToCreate);
    for (unsigned i=1; i<=subsToCreate; i++) {
        StringBuffer name;
        name.append("sub").append(i);
        StringBuffer sub(bufScope);
        sub.append("::").append(name);

        // Remove first
        if (dir.exists(sub.str(),user,true,false))
            ASSERT(dir.removeEntry(sub.str(), user) && "Can't remove sub-file");

        try {
            // Create the sub file with an arbitrary format
            Owned<IFileDescriptor> subd = createDescriptor(bufDir.str(), name.str(), 1, 17);
            Owned<IPartDescriptor> partd = subd->getPart(0);
            RemoteFilename rfn;
            partd->getFilename(0, rfn);
            StringBuffer fname;
            rfn.getPath(fname);
            recursiveCreateDirectoryForFile(fname.str());
            OwnedIFile ifile = createIFile(fname.str());
            Owned<IFileIO> io;
            io.setown(ifile->open(IFOcreate));
            io->write(0, 17, "12345678901234567");
            io->close();
            Owned<IDistributedFile> dsub = dir.createNew(subd);
            dsub->attach(sub.str(),user);
        } catch (IException *e) {
            StringBuffer msg;
            e->errorMessage(msg);
            logctx.CTXLOG("Caught exception while creating file in DFS: %s", msg.str());
            e->Release();
            ASSERT(0 && "Exception Caught in setupDFS - is the directory writeable by this user?");
        }

        // Make sure it got created
        ASSERT(dir.exists(sub.str(),user,true,false) && "Can't add physical files");
    }
    numStdFiles = subsToCreate;
}

class CDaliDFSStressTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(CDaliDFSStressTests);
        CPPUNIT_TEST(testInit);
        CPPUNIT_TEST(testGroups);
        CPPUNIT_TEST(testMultiCluster);
        CPPUNIT_TEST(testDFSTrans);
        CPPUNIT_TEST(testDFSPromote);
        CPPUNIT_TEST(testDFSDel);
        CPPUNIT_TEST(testDFSRename);
        CPPUNIT_TEST(testDFSClearAdd);
        CPPUNIT_TEST(testDFSRename2);
        CPPUNIT_TEST(testDFSRenameThenDelete);
        CPPUNIT_TEST(testDFSRemoveSuperSub);
// This test requires access to an external IP with dafilesrv running
//        CPPUNIT_TEST(testDFSRename3);
    CPPUNIT_TEST_SUITE_END();

    void testGrp(SocketEndpointArray &epa)
    {
        Owned<IGroup> grp = createIGroup(epa);
        StringBuffer s;
        grp->getText(s);
        printf("'%s'\n",s.str());
        Owned<IGroup> grp2 = createIGroup(s.str());
        if (grp->compare(grp2)!=GRidentical)
        {
            s.clear().append("Group did not match: ");
            grp->getText(s);
            CPPUNIT_ASSERT_MESSAGE(s.str(), 0);
        }
    }

    const IContextLogger &logctx;

public:
    CDaliDFSStressTests() : logctx(queryDummyContextLogger())
    {
    }
    ~CDaliDFSStressTests()
    {
        daliClientEnd();
    }
    void testInit()
    {
        daliClientInit();
    }
    void testDFSTrans()
    {
        setupDFS(logctx, "trans");

        Owned<IDistributedFileTransaction> transaction = createDistributedFileTransaction(user);

        // Auto-commit
        logctx.CTXLOG("Auto-commit test (inactive transaction)");
        Owned<IDistributedSuperFile> sfile1 = dir.createSuperFile("regress::trans::super1",user , false, false, transaction);
        sfile1->addSubFile("regress::trans::sub1", false, NULL, false, transaction);
        sfile1->addSubFile("regress::trans::sub2", false, NULL, false, transaction);
        sfile1.clear();
        sfile1.setown(dir.lookupSuperFile("regress::trans::super1", user, AccessMode::readMeta, transaction));
        ASSERT(sfile1.get() && "non-transactional add super1 failed");
        ASSERT(sfile1->numSubFiles() == 2 && "auto-commit add sub failed, not all subs were added");
        ASSERT(strcmp(sfile1->querySubFile(0).queryLogicalName(), "regress::trans::sub1") == 0 && "auto-commit add sub failed, wrong name for sub1");
        ASSERT(strcmp(sfile1->querySubFile(1).queryLogicalName(), "regress::trans::sub2") == 0 && "auto-commit add sub failed, wrong name for sub2");
        sfile1.clear();

        // Rollback
        logctx.CTXLOG("Rollback test (active transaction)");
        transaction->start();
        Owned<IDistributedSuperFile> sfile2 = dir.createSuperFile("regress::trans::super2", user, false, false, transaction);
        sfile2->addSubFile("regress::trans::sub3", false, NULL, false, transaction);
        sfile2->addSubFile("regress::trans::sub4", false, NULL, false, transaction);
        transaction->rollback();
        ASSERT(sfile2->numSubFiles() == 0 && "transactional rollback failed, some subs were added");
        sfile2.clear();
        sfile2.setown(dir.lookupSuperFile("regress::trans::super2", user, AccessMode::readMeta, transaction));
        ASSERT(!sfile2.get() && "transactional rollback super2 failed, it exists!");

        // Commit
        logctx.CTXLOG("Commit test (active transaction)");
        transaction->start();
        Owned<IDistributedSuperFile> sfile3 = dir.createSuperFile("regress::trans::super3", user, false, false, transaction);
        sfile3->addSubFile("regress::trans::sub3", false, NULL, false, transaction);
        sfile3->addSubFile("regress::trans::sub4", false, NULL, false, transaction);
        transaction->commit();
        sfile3.clear();
        sfile3.setown(dir.lookupSuperFile("regress::trans::super3", user, AccessMode::readMeta, transaction));
        ASSERT(sfile3.get() && "transactional add super3 failed");
        ASSERT(sfile3->numSubFiles() == 2 && "transactional add sub failed, not all subs were added");
        ASSERT(strcmp(sfile3->querySubFile(0).queryLogicalName(), "regress::trans::sub3") == 0 && "transactional add sub failed, wrong name for sub3");
        ASSERT(strcmp(sfile3->querySubFile(1).queryLogicalName(), "regress::trans::sub4") == 0 && "transactional add sub failed, wrong name for sub4");
        sfile3.clear();
    }

    void testDFSPromote()
    {
        setupDFS(logctx, "trans");

        unsigned timeout = 1000; // 1s

        /* Make the meta info of one of the subfiles mismatch the rest, as subfiles are promoted through
         * the super files, this should _not_ cause an issue, as no single super file will contain
         * mismatched subfiles.
        */
        Owned<IDistributedFile> sub1 = dir.lookup("regress::trans::sub1", user, AccessMode::tbdRead, false, false, NULL, false, timeout);
        assertex(sub1);
        sub1->lockProperties();
        sub1->queryAttributes().setPropBool("@local", true);
        sub1->unlockProperties();
        sub1.clear();

        Owned<IDistributedFileTransaction> transaction = createDistributedFileTransaction(user);

        // ===============================================================================
        // Don't change these parameters, or you'll have to change all ERROR tests below
        const char *sfnames[3] = {
            "regress::trans::super1", "regress::trans::super2", "regress::trans::super3"
        };
        bool delsub = false;
        bool createonlyone = true;
        // ===============================================================================
        StringArray outlinked;

        logctx.CTXLOG("Promote (1, -, -) - first iteration");
        dir.promoteSuperFiles(3, sfnames, "regress::trans::sub1", delsub, createonlyone, user, timeout, outlinked);
        {
            Owned<IDistributedSuperFile> sfile1 = dir.lookupSuperFile("regress::trans::super1", user, AccessMode::readMeta, NULL, timeout);
            ASSERT(sfile1.get() && "promote failed, super1 doesn't exist");
            ASSERT(sfile1->numSubFiles() == 1 && "promote failed, super1 should have one subfile");
            ASSERT(strcmp(sfile1->querySubFile(0).queryLogicalName(), "regress::trans::sub1") == 0 && "promote failed, wrong name for sub1");
            Owned<IDistributedSuperFile> sfile2 = dir.lookupSuperFile("regress::trans::super2", user, AccessMode::readMeta, NULL, timeout);
            ASSERT(!sfile2.get() && "promote failed, super2 does exist");
            ASSERT(outlinked.length() == 0 && "promote failed, outlinked expected empty");
        }

        logctx.CTXLOG("Promote (2, 1, -) - second iteration");
        dir.promoteSuperFiles(3, sfnames, "regress::trans::sub2", delsub, createonlyone, user, timeout, outlinked);
        {
            Owned<IDistributedSuperFile> sfile1 = dir.lookupSuperFile("regress::trans::super1", user, AccessMode::readMeta, NULL, timeout);
            ASSERT(sfile1.get() && "promote failed, super1 doesn't exist");
            ASSERT(sfile1->numSubFiles() == 1 && "promote failed, super1 should have one subfile");
            ASSERT(strcmp(sfile1->querySubFile(0).queryLogicalName(), "regress::trans::sub2") == 0 && "promote failed, wrong name for sub2");
            Owned<IDistributedSuperFile> sfile2 = dir.lookupSuperFile("regress::trans::super2", user, AccessMode::readMeta, NULL, timeout);
            ASSERT(sfile2.get() && "promote failed, super2 doesn't exist");
            ASSERT(sfile2->numSubFiles() == 1 && "promote failed, super2 should have one subfile");
            ASSERT(strcmp(sfile2->querySubFile(0).queryLogicalName(), "regress::trans::sub1") == 0 && "promote failed, wrong name for sub1");
            Owned<IDistributedSuperFile> sfile3 = dir.lookupSuperFile("regress::trans::super3", user, AccessMode::readMeta, NULL, timeout);
            ASSERT(!sfile3.get() && "promote failed, super3 does exist");
            ASSERT(outlinked.length() == 0 && "promote failed, outlinked expected empty");
        }

        logctx.CTXLOG("Promote (3, 2, 1) - third iteration");
        dir.promoteSuperFiles(3, sfnames, "regress::trans::sub3", delsub, createonlyone, user, timeout, outlinked);
        {
            Owned<IDistributedSuperFile> sfile1 = dir.lookupSuperFile("regress::trans::super1", user, AccessMode::readMeta, NULL, timeout);
            ASSERT(sfile1.get() &&* "promote failed, super1 doesn't exist");
            ASSERT(sfile1->numSubFiles() == 1 && "promote failed, super1 should have one subfile");
            ASSERT(strcmp(sfile1->querySubFile(0).queryLogicalName(), "regress::trans::sub3") == 0 && "promote failed, wrong name for sub3");
            Owned<IDistributedSuperFile> sfile2 = dir.lookupSuperFile("regress::trans::super2", user, AccessMode::readMeta, NULL, timeout);
            ASSERT(sfile2.get() && "promote failed, super2 doesn't exist");
            ASSERT(sfile2->numSubFiles() == 1 && "promote failed, super2 should have one subfile");
            ASSERT(strcmp(sfile2->querySubFile(0).queryLogicalName(), "regress::trans::sub2") == 0 && "promote failed, wrong name for sub2");
            Owned<IDistributedSuperFile> sfile3 = dir.lookupSuperFile("regress::trans::super3", user, AccessMode::readMeta, NULL, timeout);
            ASSERT(sfile3.get() && "promote failed, super3 doesn't exist");
            ASSERT(sfile3->numSubFiles() == 1 && "promote failed, super3 should have one subfile");
            ASSERT(strcmp(sfile3->querySubFile(0).queryLogicalName(), "regress::trans::sub1") == 0 && "promote failed, wrong name for sub1");
            ASSERT(outlinked.length() == 0 && "promote failed, outlinked expected empty");
        }

        logctx.CTXLOG("Promote (4, 3, 2) - fourth iteration, expect outlinked");
        dir.promoteSuperFiles(3, sfnames, "regress::trans::sub4", delsub, createonlyone, user, timeout, outlinked);
        {
            Owned<IDistributedSuperFile> sfile1 = dir.lookupSuperFile("regress::trans::super1", user, AccessMode::readMeta, NULL, timeout);
            ASSERT(sfile1.get() && "promote failed, super1 doesn't exist");
            ASSERT(sfile1->numSubFiles() == 1 && "promote failed, super1 should have one subfile");
            ASSERT(strcmp(sfile1->querySubFile(0).queryLogicalName(), "regress::trans::sub4") == 0 && "promote failed, wrong name for sub4");
            Owned<IDistributedSuperFile> sfile2 = dir.lookupSuperFile("regress::trans::super2", user, AccessMode::readMeta, NULL, timeout);
            ASSERT(sfile2.get() && "promote failed, super2 doesn't exist");
            ASSERT(sfile2->numSubFiles() == 1 && "promote failed, super2 should have one subfile");
            ASSERT(strcmp(sfile2->querySubFile(0).queryLogicalName(), "regress::trans::sub3") == 0 && "promote failed, wrong name for sub3");
            Owned<IDistributedSuperFile> sfile3 = dir.lookupSuperFile("regress::trans::super3", user, AccessMode::readMeta, NULL, timeout);
            ASSERT(sfile3.get() && "promote failed, super3 doesn't exist");
            ASSERT(sfile3->numSubFiles() == 1 && "promote failed, super3 should have one subfile");
            ASSERT(strcmp(sfile3->querySubFile(0).queryLogicalName(), "regress::trans::sub2") == 0 && "promote failed, wrong name for sub2");
            ASSERT(outlinked.length() == 1 && "promote failed, outlinked expected only one item");
            ASSERT(strcmp(outlinked.popGet(), "regress::trans::sub1") == 0 && "promote failed, outlinked expected to be sub1");
            Owned<IDistributedFile> sub1 = dir.lookup("regress::trans::sub1", user, AccessMode::tbdRead, false, false, NULL, false, timeout);
            ASSERT(sub1.get() && "promote failed, sub1 was physically deleted");
        }

        logctx.CTXLOG("Promote ([2,3], 4, 3) - fifth iteration, two in-files");
        dir.promoteSuperFiles(3, sfnames, "regress::trans::sub2,regress::trans::sub3", delsub, createonlyone, user, timeout, outlinked);
        {
            Owned<IDistributedSuperFile> sfile1 = dir.lookupSuperFile("regress::trans::super1", user, AccessMode::readMeta, NULL, timeout);
            ASSERT(sfile1.get() && "promote failed, super1 doesn't exist");
            ASSERT(sfile1->numSubFiles() == 2 && "promote failed, super1 should have two subfiles");
            ASSERT(strcmp(sfile1->querySubFile(0).queryLogicalName(), "regress::trans::sub2") == 0 && "promote failed, wrong name for sub1");
            ASSERT(strcmp(sfile1->querySubFile(1).queryLogicalName(), "regress::trans::sub3") == 0 && "promote failed, wrong name for sub2");
            Owned<IDistributedSuperFile> sfile2 = dir.lookupSuperFile("regress::trans::super2", user, AccessMode::readMeta, NULL, timeout);
            ASSERT(sfile2.get() && "promote failed, super2 doesn't exist");
            ASSERT(sfile2->numSubFiles() == 1 && "promote failed, super2 should have one subfile");
            ASSERT(strcmp(sfile2->querySubFile(0).queryLogicalName(), "regress::trans::sub4") == 0 && "promote failed, wrong name for sub4");
            Owned<IDistributedSuperFile> sfile3 = dir.lookupSuperFile("regress::trans::super3", user, AccessMode::readMeta, NULL, timeout);
            ASSERT(sfile3.get() && "promote failed, super3 doesn't exist");
            ASSERT(sfile3->numSubFiles() == 1 && "promote failed, super3 should have one subfile");
            ASSERT(strcmp(sfile3->querySubFile(0).queryLogicalName(), "regress::trans::sub3") == 0 && "promote failed, wrong name for sub3");
            ASSERT(outlinked.length() == 1 && "promote failed, outlinked expected only one item");
            ASSERT(strcmp(outlinked.popGet(), "regress::trans::sub2") == 0 && "promote failed, outlinked expected to be sub2");
            Owned<IDistributedFile> sub1 = dir.lookup("regress::trans::sub1", user, AccessMode::tbdRead, false, false, NULL, false, timeout);
            ASSERT(sub1.get() && "promote failed, sub1 was physically deleted");
            Owned<IDistributedFile> sub2 = dir.lookup("regress::trans::sub2", user, AccessMode::tbdRead, false, false, NULL, false, timeout);
            ASSERT(sub2.get() && "promote failed, sub2 was physically deleted");
        }
    }
    void testMultiCluster()
    {
        queryNamedGroupStore().add("testgrp1", { "192.168.51.1-5" });
        queryNamedGroupStore().add("testgrp2", { "192.168.16.1-5" });
        queryNamedGroupStore().add("testgrp3", { "192.168.53.1-5" });

        Owned<IFileDescriptor> fdesc = createFileDescriptor();
        fdesc->setDefaultDir("/c$/thordata/test");
        fdesc->setPartMask("testfile1._$P$_of_$N$");
        fdesc->setNumParts(5);
        for (unsigned p=0; p<fdesc->numParts(); p++)
            fdesc->queryPart(p)->queryProperties().setPropInt64("@size", 10);
        ClusterPartDiskMapSpec mapping;
        fdesc->addCluster("testgrp1", nullptr, mapping);
        fdesc->addCluster("testgrp2", nullptr, mapping);
        fdesc->addCluster("testgrp3", nullptr, mapping);
        removeLogical("test::testfile1", user);
        Owned<IDistributedFile> file = queryDistributedFileDirectory().createNew(fdesc);
        removeLogical("test::testfile1", user);
        file->attach("test::testfile1",user);
        StringBuffer name;
        unsigned i;
        for (i=0;i<file->numClusters();i++)
            PROGLOG("cluster[%d] = %s",i,file->getClusterName(i,name.clear()).str());
        file.clear();
        file.setown(queryDistributedFileDirectory().lookup("test::testfile1",user,AccessMode::tbdRead,false,false,nullptr,defaultNonPrivilegedUser));
        for (i=0;i<file->numClusters();i++)
            PROGLOG("cluster[%d] = %s",i,file->getClusterName(i,name.clear()).str());
        file.clear();
        file.setown(queryDistributedFileDirectory().lookup("test::testfile1@testgrp1",user,AccessMode::tbdRead,false,false,nullptr,defaultNonPrivilegedUser));
        for (i=0;i<file->numClusters();i++)
            PROGLOG("cluster[%d] = %s",i,file->getClusterName(i,name.clear()).str());
        file.clear();
        removeLogical("test::testfile1@testgrp2", user);
        file.setown(queryDistributedFileDirectory().lookup("test::testfile1",user,AccessMode::tbdRead,false,false,nullptr,defaultNonPrivilegedUser));
        for (i=0;i<file->numClusters();i++)
            PROGLOG("cluster[%d] = %s",i,file->getClusterName(i,name.clear()).str());
    }
    void testGroups()
    {
        SocketEndpointArray epa;
        SocketEndpoint ep;
        Owned<IGroup> grp;
        ep.set("10.150.10.80");
        epa.append(ep);
        testGrp(epa);
        ep.set("10.150.10.81");
        epa.append(ep);
        testGrp(epa);
        ep.set("10.150.10.82");
        epa.append(ep);
        testGrp(epa);
        ep.set("10.150.10.83:111");
        epa.append(ep);
        testGrp(epa);
        ep.set("10.150.10.84:111");
        epa.append(ep);
        testGrp(epa);
        ep.set("10.150.10.84:111");
        epa.append(ep);
        testGrp(epa);
        ep.set("10.150.10.84:111");
        epa.append(ep);
        testGrp(epa);
        ep.set("10.150.10.84:111");
        epa.append(ep);
        testGrp(epa);
        ep.set("10.150.10.85:111");
        epa.append(ep);
        testGrp(epa);
        ep.set("10.150.10.86:111");
        epa.append(ep);
        testGrp(epa);
        ep.set("10.150.10.87");
        epa.append(ep);
        testGrp(epa);
        ep.set("10.150.10.87");
        epa.append(ep);
        testGrp(epa);
        ep.set("10.150.10.88");
        epa.append(ep);
        testGrp(epa);
        ep.set("10.150.11.88");
        epa.append(ep);
        testGrp(epa);
        ep.set("10.173.10.88");
        epa.append(ep);
        testGrp(epa);
        ep.set("10.173.10.88:22222");
        epa.append(ep);
        testGrp(epa);
        ep.set("192.168.10.88");
        epa.append(ep);
        testGrp(epa);
    }

    void testDFSDel()
    {
        Owned<IDistributedFileTransaction> transaction = createDistributedFileTransaction(user); // disabled, auto-commit

        setupDFS(logctx, "del");

        // Sub-file deletion
        logctx.CTXLOG("Creating regress::del::super1 and attaching sub");
        Owned<IDistributedSuperFile> sfile = dir.createSuperFile("regress::del::super1", user, false, false, transaction);
        sfile->addSubFile("regress::del::sub1", false, NULL, false, transaction);
        sfile->addSubFile("regress::del::sub4", false, NULL, false, transaction);
        sfile.clear();

        logctx.CTXLOG("Deleting regress::del::sub1, should fail");
        try {
            if (dir.removeEntry("regress::del::sub1", user, transaction)) {
                ASSERT(0 && "Could remove sub, this will make the DFS inconsistent!");
                return;
            }
        } catch (IException *e) {
            // expecting an exception
            e->Release();
        }


        logctx.CTXLOG("Removing regress::del::sub1 from super1, no del");
        sfile.setown(transaction->lookupSuperFile("regress::del::super1", AccessMode::tbdWrite));
        sfile->removeSubFile("regress::del::sub1", false, false, transaction);
        ASSERT(sfile->numSubFiles() == 1 && "File sub1 was not removed from super1");
        sfile.clear();
        ASSERT(dir.exists("regress::del::sub1", user) && "File sub1 was removed from the file system");

        logctx.CTXLOG("Removing regress::del::sub4 from super1, del");
        sfile.setown(transaction->lookupSuperFile("regress::del::super1", AccessMode::tbdWrite));
        sfile->removeSubFile("regress::del::sub4", true, false, transaction);
        ASSERT(!sfile->numSubFiles() && "File sub4 was not removed from super1");
        sfile.clear();
        ASSERT(!dir.exists("regress::del::sub4", user) && "File sub4 was NOT removed from the file system");

        // Logical Remove
        logctx.CTXLOG("Deleting 'regress::del::super1, should work");
        ASSERT(dir.removeEntry("regress::del::super1", user) && "Can't remove super1");
        logctx.CTXLOG("Deleting 'regress::del::sub1 autoCommit, should work");
        ASSERT(dir.removeEntry("regress::del::sub1", user) && "Can't remove sub1");

        logctx.CTXLOG("Removing 'regress::del::sub2 - rollback");
        transaction->start();
        dir.removeEntry("regress::del::sub2", user, transaction);
        transaction->rollback();

        ASSERT(dir.exists("regress::del::sub2", user, true, false) && "Shouldn't have removed sub2 on rollback");

        logctx.CTXLOG("Removing 'regress::del::sub2 - commit");
        transaction->start();
        dir.removeEntry("regress::del::sub2", user, transaction);
        transaction->commit();

        ASSERT(!dir.exists("regress::del::sub2", user, true, false) && "Should have removed sub2 on commit");

        // Physical Remove
        logctx.CTXLOG("Physically removing 'regress::del::sub3 - rollback");
        transaction->start();
        dir.removeEntry("regress::del::sub3", user, transaction);
        transaction->rollback();

        ASSERT(dir.exists("regress::del::sub3", user, true, false) && "Shouldn't have removed sub3 on rollback");

        logctx.CTXLOG("Physically removing 'regress::del::sub3 - commit");
        transaction->start();
        dir.removeEntry("regress::del::sub3", user, transaction);
        transaction->commit();

        ASSERT(!dir.exists("regress::del::sub3", user, true, false) && "Should have removed sub3 on commit");
    }

    void testDFSRename()
    {
        Owned<IDistributedFileTransaction> transaction = createDistributedFileTransaction(user); // disabled, auto-commit

        if (dir.exists("regress::rename::other1",user,false,false))
            ASSERT(dir.removeEntry("regress::rename::other1", user) && "Can't remove 'regress::rename::other1'");
        if (dir.exists("regress::rename::other2",user,false,false))
            ASSERT(dir.removeEntry("regress::rename::other2", user) && "Can't remove 'regress::rename::other2'");

        setupDFS(logctx, "rename");

        try {
            logctx.CTXLOG("Renaming 'regress::rename::sub1 to 'sub2' with auto-commit, should fail");
            dir.renamePhysical("regress::rename::sub1", "regress::rename::sub2", user, transaction);
            ASSERT(0 && "Renamed to existing file should have failed!");
            return;
        } catch (IException *e) {
            // Expecting exception
            e->Release();
        }

        logctx.CTXLOG("Renaming 'regress::rename::sub1 to 'other1' with auto-commit");
        dir.renamePhysical("regress::rename::sub1", "regress::rename::other1", user, transaction);
        ASSERT(dir.exists("regress::rename::other1", user, true, false) && "Renamed to other failed");

        logctx.CTXLOG("Renaming 'regress::rename::sub2 to 'other2' and rollback");
        transaction->start();
        dir.renamePhysical("regress::rename::sub2", "regress::rename::other2", user, transaction);
        transaction->rollback();
        ASSERT(!dir.exists("regress::rename::other2", user, true, false) && "Renamed to other2 when it shouldn't");

        logctx.CTXLOG("Renaming 'regress::rename::sub2 to 'other2' and commit");
        transaction->start();
        dir.renamePhysical("regress::rename::sub2", "regress::rename::other2", user, transaction);
        transaction->commit();
        ASSERT(dir.exists("regress::rename::other2", user, true, false) && "Renamed to other failed");

        try {
            logctx.CTXLOG("Renaming 'regress::rename::sub3 to 'sub3' with auto-commit, should fail");
            dir.renamePhysical("regress::rename::sub3", "regress::rename::sub3", user, transaction);
            ASSERT(0 && "Renamed to same file should have failed!");
            return;
        } catch (IException *e) {
            // Expecting exception
            e->Release();
        }

        // To make sure renamed files are cleaned properly
        printf("Renaming 'regress::rename::other2 to 'sub2' on auto-commit\n");
        dir.renamePhysical("regress::rename::other2", "regress::rename::sub2", user, transaction);
        ASSERT(dir.exists("regress::rename::sub2", user, true, false) && "Renamed from other2 failed");
    }

    void testDFSClearAdd()
    {
        setupDFS(logctx, "clearadd");

        Owned<IDistributedFileTransaction> transaction = createDistributedFileTransaction(user); // disabled, auto-commit

        logctx.CTXLOG("Creating regress::clearadd::super1 and attaching sub1 & sub4");
        Owned<IDistributedSuperFile> sfile = dir.createSuperFile("regress::clearadd::super1", user, false, false, transaction);
        sfile->addSubFile("regress::clearadd::sub1", false, NULL, false, transaction);
        sfile->addSubFile("regress::clearadd::sub4", false, NULL, false, transaction);
        sfile.clear();

        transaction.setown(createDistributedFileTransaction(user)); // disabled, auto-commit
        transaction->start();

        logctx.CTXLOG("Removing sub1 from super1, within transaction");
        sfile.setown(transaction->lookupSuperFile("regress::clearadd::super1", AccessMode::tbdWrite));
        sfile->removeSubFile("regress::clearadd::sub1", false, false, transaction);
        sfile.clear();

        logctx.CTXLOG("Adding sub1 back into to super1, within transaction");
        sfile.setown(transaction->lookupSuperFile("regress::clearadd::super1", AccessMode::tbdWrite));
        sfile->addSubFile("regress::clearadd::sub1", false, NULL, false, transaction);
        sfile.clear();
        try
        {
            transaction->commit();
        }
        catch (IException *e)
        {
            StringBuffer eStr;
            e->errorMessage(eStr);
            CPPUNIT_ASSERT_MESSAGE(eStr.str(), 0);
            e->Release();
        }
        sfile.setown(dir.lookupSuperFile("regress::clearadd::super1", user, AccessMode::readMeta));
        ASSERT(NULL != sfile->querySubFileNamed("regress::clearadd::sub1") && "regress::clearadd::sub1, should be a subfile of super1");

        // same but remove all (clear)
        transaction.setown(createDistributedFileTransaction(user)); // disabled, auto-commit
        transaction->start();

        logctx.CTXLOG("Adding sub2 into to super1, within transaction");
        sfile.setown(transaction->lookupSuperFile("regress::clearadd::super1", AccessMode::tbdWrite));
        sfile->addSubFile("regress::clearadd::sub2", false, NULL, false, transaction);
        sfile.clear();

        logctx.CTXLOG("Removing all sub files from super1, within transaction");
        sfile.setown(transaction->lookupSuperFile("regress::clearadd::super1", AccessMode::tbdWrite));
        sfile->removeSubFile(NULL, false, false, transaction);
        sfile.clear();

        logctx.CTXLOG("Adding sub2 back into to super1, within transaction");
        sfile.setown(transaction->lookupSuperFile("regress::clearadd::super1", AccessMode::tbdWrite));
        sfile->addSubFile("regress::clearadd::sub2", false, NULL, false, transaction);
        sfile.clear();
        try
        {
            transaction->commit();
        }
        catch (IException *e)
        {
            StringBuffer eStr;
            e->errorMessage(eStr);
            CPPUNIT_ASSERT_MESSAGE(eStr.str(), 0);
            e->Release();
        }
        sfile.setown(dir.lookupSuperFile("regress::clearadd::super1", user, AccessMode::readMeta));
        ASSERT(NULL != sfile->querySubFileNamed("regress::clearadd::sub2") && "regress::clearadd::sub2, should be a subfile of super1");
        ASSERT(NULL == sfile->querySubFileNamed("regress::clearadd::sub1") && "regress::clearadd::sub1, should NOT be a subfile of super1");
        ASSERT(NULL == sfile->querySubFileNamed("regress::clearadd::sub4") && "regress::clearadd::sub4, should NOT be a subfile of super1");
        ASSERT(1 == sfile->numSubFiles() && "regress::clearadd::super1 should contain 1 subfile");
    }
    void testDFSRename2()
    {
        setupDFS(logctx, "rename2");

        /* Create a super and sub1 and sub4 in a auto-commit transaction
         * Inside a transaction, do:
         * a) rename sub2 to renamedsub2
         * b) remove sub1
         * c) add sub1
         * d) add renamedsub2
         * e) commit transaction
         * f) renamed files existing and superfile contents
         */
        Owned<IDistributedFileTransaction> transaction = createDistributedFileTransaction(user); // disabled, auto-commit

        logctx.CTXLOG("Creating regress::rename2::super1 and attaching sub1 & sub4");
        Owned<IDistributedSuperFile> sfile = dir.createSuperFile("regress::rename2::super1", user, false, false, transaction);
        sfile->addSubFile("regress::rename2::sub1", false, NULL, false, transaction);
        sfile->addSubFile("regress::rename2::sub4", false, NULL, false, transaction);
        sfile.clear();

        if (dir.exists("regress::rename2::renamedsub2",user,false,false))
            ASSERT(dir.removeEntry("regress::rename2::renamedsub2", user) && "Can't remove 'regress::rename2::renamedsub2'");

        transaction.setown(createDistributedFileTransaction(user)); // disabled, auto-commit

        logctx.CTXLOG("Starting transaction");
        transaction->start();

        logctx.CTXLOG("Renaming regress::rename2::sub2 TO regress::rename2::renamedsub2");
        dir.renamePhysical("regress::rename2::sub2", "regress::rename2::renamedsub2", user, transaction);

        logctx.CTXLOG("Removing regress::rename2::sub1 from regress::rename2::super1");
        sfile.setown(transaction->lookupSuperFile("regress::rename2::super1", AccessMode::tbdWrite));
        sfile->removeSubFile("regress::rename2::sub1", false, false, transaction);
        sfile.clear();

        logctx.CTXLOG("Adding renamedsub2 to super1");
        sfile.setown(transaction->lookupSuperFile("regress::rename2::super1", AccessMode::tbdWrite));
        sfile->addSubFile("regress::rename2::renamedsub2", false, NULL, false, transaction);
        sfile.clear();

        logctx.CTXLOG("Adding back sub1 to super1");
        sfile.setown(transaction->lookupSuperFile("regress::rename2::super1", AccessMode::tbdWrite));
        sfile->addSubFile("regress::rename2::sub1", false, NULL, false, transaction);
        sfile.clear();

        try
        {
            logctx.CTXLOG("Committing transaction");
            transaction->commit();
        }
        catch (IException *e)
        {
            StringBuffer eStr;
            e->errorMessage(eStr);
            CPPUNIT_ASSERT_MESSAGE(eStr.str(), 0);
            e->Release();
        }
        transaction.clear();

        // validate..
        ASSERT(dir.exists("regress::rename2::renamedsub2", user, true, false) && "regress::rename2::renamedsub2 should exist now transaction committed");
        sfile.setown(dir.lookupSuperFile("regress::rename2::super1", user, AccessMode::readMeta));

        ASSERT(NULL != sfile->querySubFileNamed("regress::rename2::renamedsub2") && "regress::rename2::renamedsub2, should be a subfile of super1");
        ASSERT(NULL != sfile->querySubFileNamed("regress::rename2::sub1") && "regress::rename2::sub1, should be a subfile of super1");
        ASSERT(NULL == sfile->querySubFileNamed("regress::rename2::sub2") && "regress::rename2::sub2, should NOT be a subfile of super1");
        ASSERT(NULL != sfile->querySubFileNamed("regress::rename2::sub4") && "regress::rename2::sub4, should be a subfile of super1");
        ASSERT(3 == sfile->numSubFiles() && "regress::rename2::super1 should contain 4 subfiles");
    }

    void testDFSRenameThenDelete()
    {
        setupDFS(logctx, "renamedelete");
        if (dir.exists("regress::renamedelete::renamedsub2",user,false,false))
            ASSERT(dir.removeEntry("regress::renamedelete::renamedsub2", user) && "Can't remove 'regress::renamedelete::renamedsub2'");

        Owned<IDistributedFileTransaction> transaction = createDistributedFileTransaction(user); // disabled, auto-commit

        logctx.CTXLOG("Starting transaction");
        transaction->start();

        logctx.CTXLOG("Renaming regress::renamedelete::sub2 TO regress::renamedelete::renamedsub2");
        dir.renamePhysical("regress::renamedelete::sub2", "regress::renamedelete::renamedsub2", user, transaction);

        logctx.CTXLOG("Removing regress::renamedelete::renamedsub2");
        ASSERT(dir.removeEntry("regress::renamedelete::renamedsub2", user, transaction) && "Can't remove 'regress::rename2::renamedsub2'");


        try
        {
            logctx.CTXLOG("Committing transaction");
            transaction->commit();
        }
        catch (IException *e)
        {
            StringBuffer eStr;
            e->errorMessage(eStr);
            CPPUNIT_ASSERT_MESSAGE(eStr.str(), 0);
            e->Release();
        }
        transaction.clear();

        // validate..
        ASSERT(!dir.exists("regress::renamedelete::sub2", user, true, false) && "regress::renamedelete::sub2 should NOT exist now transaction has been committed");
        ASSERT(!dir.exists("regress::renamedelete::renamedsub2", user, true, false) && "regress::renamedelete::renamedsub2 should NOT exist now transaction has been committed");
    }

// NB: This test requires access (via dafilesrv) to an external IP (10.239.222.21 used below, but could be any)
    void testDFSRename3()
    {
        setupDFS(logctx, "rename3");

        Owned<IDistributedFileTransaction> transaction = createDistributedFileTransaction(user); // disabled, auto-commit

        if (dir.exists("regress::tenwayfile",user))
            ASSERT(dir.removeEntry("regress::tenwayfile", user) && "Can't remove");

        Owned<IFileDescriptor> fdesc = createDescriptor("regress", "tenwayfile", 1, 17);
        Owned<IGroup> grp1 = createIGroup("10.239.222.1");
        ClusterPartDiskMapSpec mapping;
        fdesc->setClusterGroup(0, grp1);

        Linked<IPartDescriptor> part = fdesc->getPart(0);
        RemoteFilename rfn;
        part->getFilename(0, rfn);
        StringBuffer path;
        rfn.getPath(path);
        recursiveCreateDirectoryForFile(path.str());
        OwnedIFile ifile = createIFile(path.str());
        Owned<IFileIO> io;
        io.setown(ifile->open(IFOcreate));
        io->write(0, 17, "12345678901234567");
        io->close();

        Owned<IDistributedFile> dsub = dir.createNew(fdesc);
        dsub->attach("regress::tenwayfile", user);
        dsub.clear();
        fdesc.clear();

        transaction.setown(createDistributedFileTransaction(user)); // disabled, auto-commit
        transaction->start();

        logctx.CTXLOG("Renaming regress::rename3::sub2 TO regress::tenwayfile@mythor");
        dir.renamePhysical("regress::rename3::sub2", "regress::tenwayfile@mythor", user, transaction);

        try
        {
            transaction->commit();
        }
        catch (IException *e)
        {
            StringBuffer eStr;
            e->errorMessage(eStr);
            CPPUNIT_ASSERT_MESSAGE(eStr.str(), 0);
            e->Release();
        }

        transaction.setown(createDistributedFileTransaction(user));

        transaction.setown(createDistributedFileTransaction(user)); // disabled, auto-commit
        transaction->start();

        logctx.CTXLOG("Renaming regress::tenwayfile TO regress::rename3::sub2");
        dir.renamePhysical("regress::tenwayfile@mythor", "regress::rename3::sub2", user, transaction);

        try
        {
            transaction->commit();
        }
        catch (IException *e)
        {
            StringBuffer eStr;
            e->errorMessage(eStr);
            CPPUNIT_ASSERT_MESSAGE(eStr.str(), 0);
            e->Release();
        }
    }

    void testDFSRemoveSuperSub()
    {
        setupDFS(logctx, "removesupersub");

        Owned<IDistributedFileTransaction> transaction = createDistributedFileTransaction(user);

        logctx.CTXLOG("Creating regress::removesupersub::super1 and attaching sub1 & sub4");
        Owned<IDistributedSuperFile> sfile = dir.createSuperFile("regress::removesupersub::super1", user, false, false, transaction);
        sfile->addSubFile("regress::removesupersub::sub1", false, NULL, false, transaction);
        sfile->addSubFile("regress::removesupersub::sub4", false, NULL, false, transaction);
        sfile.clear();

        transaction.setown(createDistributedFileTransaction(user)); // disabled, auto-commit

        logctx.CTXLOG("Starting transaction");
        transaction->start();

        logctx.CTXLOG("Removing super removesupersub::super1 along with it's subfiles sub1 and sub4");
        dir.removeSuperFile("regress::removesupersub::super1", true, user, transaction);

        try
        {
            logctx.CTXLOG("Committing transaction");
            transaction->commit();
        }
        catch (IException *e)
        {
            StringBuffer eStr;
            e->errorMessage(eStr);
            CPPUNIT_ASSERT_MESSAGE(eStr.str(), 0);
            e->Release();
        }
        transaction.clear();

        // validate..
        ASSERT(!dir.exists("regress::removesupersub::super1", user, true, false) && "regress::removesupersub::super1 should NOT exist");
        ASSERT(!dir.exists("regress::removesupersub::sub1", user, true, false) && "regress::removesupersub::sub1 should NOT exist");
        ASSERT(!dir.exists("regress::removesupersub::sub4", user, true, false) && "regress::removesupersub::sub4 should NOT exist");
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION( CDaliDFSStressTests );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( CDaliDFSStressTests, "CDaliDFSStressTests" );


class DaliDFSIteratorTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(DaliDFSIteratorTests);
        CPPUNIT_TEST(testInit);
        CPPUNIT_TEST(testIteratorSetup);
        CPPUNIT_TEST(testDirectoryIterator);
        CPPUNIT_TEST(testDFAttrIteratorSimple);
        CPPUNIT_TEST(testDFAttrIterator);
        CPPUNIT_TEST(testDFAttrIteratorSelectFields);
        CPPUNIT_TEST(testGetLogicalFilesSorted);
    CPPUNIT_TEST_SUITE_END();

    const IContextLogger &logctx;

public:
    DaliDFSIteratorTests() : logctx(queryDummyContextLogger())
    {
    }
    ~DaliDFSIteratorTests()
    {
        daliClientEnd();
    }
    void testInit()
    {
        daliClientInit();
    }
    void testIteratorSetup()
    {
        setupDFS(logctx, "iterator", 1, 4);
        Owned<IDistributedSuperFile> sfile1 = dir.createSuperFile("regress::iterator::super1", user, false);
        sfile1->addSubFile("regress::iterator::sub1");
        sfile1->addSubFile("regress::iterator::sub2");
        sfile1.clear();
        numSuperFiles++;

        // Remove first
        if (dir.exists("regress::iterator::othergroup", user, true, false))
            ASSERT(dir.removeEntry("regress::iterator::othergroup", user) && "Can't remove sub-file");

        Owned<IFileDescriptor> fdesc = createDescriptor("regress/iterator", "othergroup", 1, 17);
        Owned<IGroup> grp1 = createIGroup("127.0.0.1");
        ClusterPartDiskMapSpec mapping;
        fdesc->setClusterGroup(0, grp1);
        fdesc->setClusterGroupName(0, "grp1");
        Owned<IGroup> grp2 = createIGroup("127.0.0.2");
        fdesc->addCluster("grp2", grp2, mapping);
        Owned<IDistributedFile> dsub = dir.createNew(fdesc);
        dsub->attach("regress::iterator::othergroup", user);
        dsub.clear();
        fdesc.clear();
        numStdFiles += 2; // iterators treats as one per group
    }

    void testDirectoryIterator()
    {
        // Test basic directory iterator functionality, check counts
        // and populate some attributes for other tests

        IDistributedFileDirectory &dir = queryDistributedFileDirectory();

        Owned<IDistributedFileIterator> iter = dir.getIterator("regress::iterator::*", false, user, false);
        unsigned count = 0;
        ForEach(*iter)
            ++count;
        CPPUNIT_ASSERT_MESSAGE("testDirectoryIterator: expected 5 sub files", count == numStdFiles);

        iter.setown(dir.getIterator("regress::iterator::*", true, nullptr, false));
        count = 0;
        ForEach(*iter)
            ++count;
        CPPUNIT_ASSERT_MESSAGE("testDirectoryIterator: expected 6 sub files", count == (numStdFiles+numSuperFiles));

        iter.setown(dir.getIterator("regress::iterator::*", false, nullptr, false));

        ForEach(*iter)
        {
            IDistributedFile &file = iter->query();
            const char *logicalName = file.queryLogicalName();
            CDfsLogicalFileName lfn;
            lfn.set(logicalName);

            CDateTime dt;
            dt.setNow();
            file.setAccessedTime(dt);
            file.setExpire(4);

            DistributedFilePropertyLock lock(&file);
            IPropertyTree &attr = lock.queryAttributes();
            const char *tail = lfn.queryTail();
            StringBuffer attrValue(tail);
            attr.setProp("@job", attrValue);
            attr.setProp("@owner", attrValue);
            attr.setProp("@workunit", attrValue);
        }
    }

    void testDFAttrIteratorSimple()
    {
        IDistributedFileDirectory &dir = queryDistributedFileDirectory();

        const char *wildName = "regress::iterator::*";
        Owned<IPropertyTreeIterator> iter = dir.getDFAttributesIterator(wildName, user, true, false);
        unsigned count = 0;
        ForEach(*iter)
            ++count;
        VStringBuffer msg("testDFAttrIteratorSimple: expected %u files", numStdFiles);
        CPPUNIT_ASSERT_MESSAGE(msg, count == numStdFiles);
    }

    void testDFAttrIterator()
    {
        IDistributedFileDirectory &dir = queryDistributedFileDirectory();

        const char *wildName = "regress::iterator::*";
        StringBuffer filterBuf;
        // all non-superfiles
        filterBuf.append(DFUQFTspecial).append(DFUQFilterSeparator).append(DFUQSFFileType).append(DFUQFilterSeparator).append(DFUQFFTnonsuperfileonly).append(DFUQFilterSeparator);
        filterBuf.append(DFUQFTspecial).append(DFUQFilterSeparator).append(DFUQSFFileNameWithPrefix).append(DFUQFilterSeparator).append(wildName).append(DFUQFilterSeparator);

        bool allReceived = false;
        Owned<IPropertyTreeIterator> iter = dir.getDFAttributesFilteredIterator(filterBuf,
            nullptr,                      // no local filters
            nullptr,                      // no fields specified (default all)
            user,                         // user context
            true,                         // recursive
            allReceived                   // output parameter
        );
        CPPUNIT_ASSERT(allReceived);

        unsigned count = 0;
        ForEach(*iter)
        {
            const IPropertyTree &attrs = iter->query();
            CPPUNIT_ASSERT_MESSAGE("testDFAttrIterator: missing 'name' attribute", attrs.hasProp(getDFUQResultFieldName(DFUQResultField::name)));
            CPPUNIT_ASSERT_MESSAGE("testDFAttrIterator: missing 'group' attribute", attrs.hasProp(getDFUQResultFieldName(DFUQResultField::nodegroup)));
            CPPUNIT_ASSERT_MESSAGE("testDFAttrIterator: missing 'modified' attribute", attrs.hasProp(getDFUQResultFieldName(DFUQResultField::timemodified)));
            CPPUNIT_ASSERT_MESSAGE("testDFAttrIterator: missing 'numparts' attribute", attrs.hasProp(getDFUQResultFieldName(DFUQResultField::numparts)));
            CPPUNIT_ASSERT_MESSAGE("testDFAttrIterator: missing 'partmask' attribute", attrs.hasProp(getDFUQResultFieldName(DFUQResultField::partmask)));
            ++count;
        }
        VStringBuffer msg("testDFAttrIterator: expected %u files", numStdFiles);
        CPPUNIT_ASSERT_MESSAGE(msg, count == numStdFiles);

        // very fake local filter test - DFUQFFgroup is the only local filter supported
        const char *groupText = "grp2";
        // server side filter to get only files with this group
        filterBuf.append(DFUQFTcontainString).append(DFUQFilterSeparator).append(getDFUQFilterFieldName(DFUQFFgroup)).append(DFUQFilterSeparator).append(groupText).append(DFUQFilterSeparator).append(",").append(DFUQFilterSeparator);

        StringBuffer localFilterBuf;
        localFilterBuf.append(DFUQFTwildcardMatch).append(DFUQFilterSeparator).append(getDFUQFilterFieldName(DFUQFFgroup)).append(DFUQFilterSeparator).append(groupText);

        iter.setown(dir.getDFAttributesFilteredIterator(filterBuf,
            localFilterBuf,
            nullptr,                      // no fields specified (default all)
            user,                         // user context
            true,                         // recursive
            allReceived                   // output parameter
        ));
        CPPUNIT_ASSERT(allReceived);

        count = 0;
        ForEach(*iter)
            ++count;
        CPPUNIT_ASSERT_MESSAGE("testDFAttrIterator: expected 1 files", count == 1);
    }

    void testDFAttrIteratorSelectFields()
    {
        IDistributedFileDirectory &dir = queryDistributedFileDirectory();

        const char *wildName = "regress::iterator::sub4";
        StringBuffer filterBuf;
        // all non-superfiles
        filterBuf.append(DFUQFTspecial).append(DFUQFilterSeparator).append(DFUQSFFileType).append(DFUQFilterSeparator).append(DFUQFFTnonsuperfileonly).append(DFUQFilterSeparator);
        filterBuf.append(DFUQFTspecial).append(DFUQFilterSeparator).append(DFUQSFFileNameWithPrefix).append(DFUQFilterSeparator).append(wildName).append(DFUQFilterSeparator);

        std::vector<DFUQResultField> fields = {DFUQResultField::recordsize, DFUQResultField::term};

        bool allReceived = false;
        Owned<IPropertyTreeIterator> iter = dir.getDFAttributesFilteredIterator(filterBuf,
            nullptr,                      // no local filters
            fields.data(),                // select fields
            user,                         // user context
            true,                         // recursive
            allReceived                   // output parameter
        );
        CPPUNIT_ASSERT(allReceived);

        unsigned count = 0;
        ForEach(*iter)
        {
            const IPropertyTree &attrs = iter->query();
            CPPUNIT_ASSERT_MESSAGE("testDFAttrIteratorSelectFields: recordSize attribute should be present", attrs.hasProp("@recordSize"));
            CPPUNIT_ASSERT_MESSAGE("testDFAttrIteratorSelectFields: directory attribute should NOT be present", !attrs.hasProp("@directory"));
            ++count;
        }
        CPPUNIT_ASSERT_MESSAGE("testDFAttrIteratorSelectFields: expected 1 file", count == 1);

        auto testInclusionExclusion = [&](bool inclusion)
        {
            // test includeAll with an exclusion
            if (inclusion)
                fields = { DFUQResultField::includeAll, DFUQResultField::recordsize|DFUQResultField::exclude, DFUQResultField::term};
            else
                fields = { DFUQResultField::includeAll|DFUQResultField::exclude, DFUQResultField::recordsize, DFUQResultField::term};

            iter.setown(dir.getDFAttributesFilteredIterator(filterBuf,
                nullptr,                      // no local filters
                fields.data(),                // select fields
                user,                         // user context
                true,                         // recursive
                allReceived                   // output parameter
            ));
            CPPUNIT_ASSERT(allReceived);
            count = 0;
            ForEach(*iter)
            {
                const IPropertyTree &attrs = iter->query();
                CPPUNIT_ASSERT(inclusion == attrs.hasProp("@directory"));
                CPPUNIT_ASSERT(inclusion == attrs.hasProp("@job"));
                CPPUNIT_ASSERT(inclusion == attrs.hasProp("@owner"));
                CPPUNIT_ASSERT(inclusion == attrs.hasProp("@workunit"));
                CPPUNIT_ASSERT(inclusion == !attrs.hasProp("@recordSize"));
                ++count;
            }
            CPPUNIT_ASSERT_MESSAGE("testDFAttrIteratorSelectFields: expected 1 file", count == 1);
        };
        testInclusionExclusion(true);
        testInclusionExclusion(false);
    }

    void testGetLogicalFilesSorted()
    {
        IDistributedFileDirectory &dir = queryDistributedFileDirectory();

        const char *wildName = "regress::iterator::sub*";
        StringBuffer filterBuf;
        // all non-superfiles
        filterBuf.append(DFUQFTspecial).append(DFUQFilterSeparator).append(DFUQSFFileType).append(DFUQFilterSeparator).append(DFUQFFTnonsuperfileonly).append(DFUQFilterSeparator);
        filterBuf.append(DFUQFTspecial).append(DFUQFilterSeparator).append(DFUQSFFileNameWithPrefix).append(DFUQFilterSeparator).append(wildName).append(DFUQFilterSeparator);

        std::vector<DFUQResultField> fields = {DFUQResultField::size, DFUQResultField::cost, DFUQResultField::term};

        DFUQResultField sortOrder[2] = {DFUQResultField::job|DFUQResultField::reverse, DFUQResultField::term};
        __int64 hint;
        unsigned totalFiles;
        bool allMatchingFilesReceived;
        Owned<IPropertyTreeIterator> iter = dir.getLogicalFilesSorted(nullptr, sortOrder, filterBuf.str(),
            nullptr, fields.data(), 0, INFINITE, &hint, &totalFiles, &allMatchingFilesReceived);

        unsigned subJobs = numStdFiles - 2; // -2 exclude iterator::othergroup files
        VStringBuffer expectedJob("sub%u", subJobs); // <numStdFiles> subs, <numStdFiles> jobs, reverse order
        ForEach(*iter)
        {
            const IPropertyTree &attrs = iter->query();
            CPPUNIT_ASSERT_MESSAGE("testGetLogicalFilesSorted: group property missing", attrs.hasProp("@group"));
            VStringBuffer msg("testGetLogicalFilesSorted: expected @job == %s", expectedJob.str());
            CPPUNIT_ASSERT_MESSAGE(msg.str(), streq(attrs.queryProp("@job"), expectedJob.str()));
            expectedJob.setf("sub%u", --subJobs);
            bool costAttrsPresent = attrs.hasProp("@cost") && attrs.hasProp("@readCost") && attrs.hasProp("@writeCost") && attrs.hasProp("@atRestCost");
            CPPUNIT_ASSERT_MESSAGE("testGetLogicalFilesSorted: Missing cost attributes", costAttrsPresent);
            bool dirAttrsPresent = attrs.hasProp("@directory");
            CPPUNIT_ASSERT_MESSAGE("testGetLogicalFilesSorted: directory attribute should NOT be present", !dirAttrsPresent);
        }
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION( DaliDFSIteratorTests );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( DaliDFSIteratorTests, "DaliDFSIteratorTests" );

class CDaliDFSRetrySlowTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(CDaliDFSRetrySlowTests);
        CPPUNIT_TEST(testInit);
        CPPUNIT_TEST(testDFSAddFailReAdd);
        CPPUNIT_TEST(testDFSRetrySuperLock);
    CPPUNIT_TEST_SUITE_END();

    const IContextLogger &logctx;

public:
    CDaliDFSRetrySlowTests() : logctx(queryDummyContextLogger())
    {
    }
    ~CDaliDFSRetrySlowTests()
    {
        daliClientEnd();
    }
    void testInit()
    {
        daliClientInit();
    }
    class CShortLock : implements IThreaded
    {
        StringAttr fileName;
        unsigned secDelay;
        CThreaded threaded;
    public:
        CShortLock(const char *_fileName, unsigned _secDelay) : fileName(_fileName), secDelay(_secDelay), threaded("CShortLock", this) { }
        ~CShortLock()
        {
            threaded.join();
        }
        virtual void threadmain() override
        {
            Owned<IDistributedFile> file=queryDistributedFileDirectory().lookup(fileName,nullptr,AccessMode::tbdRead,false,false,nullptr,defaultNonPrivilegedUser);

            if (!file)
            {
                PROGLOG("File %s not found", fileName.get());
                return;
            }
            PROGLOG("Locked file: %s, sleeping (before unlock) for %d secs", fileName.get(), secDelay);

            MilliSleep(secDelay * 1000);

            PROGLOG("Unlocking file: %s", fileName.get());
        }
        void start() { threaded.start(false); }
    };

    void testDFSAddFailReAdd()
    {
        setupDFS(logctx, "addreadd");

        Owned<IDistributedFileTransaction> transaction = createDistributedFileTransaction(user); // disabled, auto-commit

        logctx.CTXLOG("Creating super1 and supet2, adding sub1 and sub2 to super1 and sub3 to super2");
        Owned<IDistributedSuperFile> sfile = dir.createSuperFile("regress::addreadd::super1", user, false, false, transaction);
        sfile->addSubFile("regress::addreadd::sub1", false, NULL, false, transaction);
        sfile->addSubFile("regress::addreadd::sub2", false, NULL, false, transaction);
        sfile.clear();
        Owned<IDistributedSuperFile> sfile2 = dir.createSuperFile("regress::addreadd::super2", user, false, false, transaction);
        sfile2->addSubFile("regress::addreadd::sub3", false, NULL, false, transaction);
        sfile2.clear();

        /* Tests transaction failing, due to lock and retrying after having partial success */

        CShortLock sL("regress::addreadd::sub2", 30); // the 2nd subfile of super1
        sL.start();

        transaction.setown(createDistributedFileTransaction(user)); // disabled, auto-commit
        logctx.CTXLOG("Starting transaction");
        transaction->start();

        logctx.CTXLOG("Adding contents of regress::addreadd::super1 to regress::addreadd::super2, within transaction");
        sfile.setown(transaction->lookupSuperFile("regress::addreadd::super2", AccessMode::tbdWrite));
        sfile->addSubFile("regress::addreadd::super1", false, NULL, true, transaction); // add contents of super1 to super2
        sfile.setown(transaction->lookupSuperFile("regress::addreadd::super1", AccessMode::tbdWrite));
        sfile->removeSubFile(NULL, false, false, transaction); // clears super1
        sfile.clear();

        try
        {
            transaction->commit();
        }
        catch (IException *e)
        {
            StringBuffer eStr;
            e->errorMessage(eStr);
            CPPUNIT_ASSERT_MESSAGE(eStr.str(), 0);
            e->Release();
        }
        transaction.clear();
        sfile.setown(dir.lookupSuperFile("regress::addreadd::super2", user, AccessMode::readMeta));
        ASSERT(3 == sfile->numSubFiles() && "regress::addreadd::super2 should contain 3 subfiles");
        ASSERT(NULL != sfile->querySubFileNamed("regress::addreadd::sub1") && "regress::addreadd::sub1, should be a subfile of super2");
        ASSERT(NULL != sfile->querySubFileNamed("regress::addreadd::sub2") && "regress::addreadd::sub2, should be a subfile of super2");
        ASSERT(NULL != sfile->querySubFileNamed("regress::addreadd::sub3") && "regress::addreadd::sub3, should be a subfile of super2");
        sfile.setown(dir.lookupSuperFile("regress::addreadd::super1", user, AccessMode::readMeta));
        ASSERT(0 == sfile->numSubFiles() && "regress::addreadd::super1 should contain 0 subfiles");
    }
    void testDFSRetrySuperLock()
    {
        setupDFS(logctx, "retrysuperlock");

        logctx.CTXLOG("Creating regress::retrysuperlock::super1 and regress::retrysuperlock::sub1");
        Owned<IDistributedSuperFile> sfile = dir.createSuperFile("regress::retrysuperlock::super1", user, false, false);
        sfile->addSubFile("regress::retrysuperlock::sub1", false, NULL, false);
        sfile.clear();

        /* Tests transaction failing, due to lock and retrying after having partial success */

        CShortLock sL("regress::retrysuperlock::super1", 15);
        sL.start();

        sfile.setown(dir.lookupSuperFile("regress::retrysuperlock::super1", user, AccessMode::tbdWrite));
        if (sfile)
        {
            logctx.CTXLOG("Removing subfiles from regress::retrysuperlock::super1");
            sfile->removeSubFile(NULL, false, false);
            logctx.CTXLOG("SUCCEEDED");
        }
        // put it back, for next test
        sfile->addSubFile("regress::retrysuperlock::sub1", false, NULL, false);
        sfile.clear();

        // try again, this time in transaction
        Owned<IDistributedFileTransaction> transaction = createDistributedFileTransaction(user); // disabled, auto-commit
        logctx.CTXLOG("Starting transaction");
        transaction->start();

        sfile.setown(transaction->lookupSuperFile("regress::retrysuperlock::super1", AccessMode::tbdWrite));
        if (sfile)
        {
            logctx.CTXLOG("Removing subfiles from regress::retrysuperlock::super1 with transaction");
            sfile->removeSubFile(NULL, false, false, transaction);
            logctx.CTXLOG("SUCCEEDED");
        }
        sfile.clear();
        logctx.CTXLOG("Committing transaction");
        transaction->commit();
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION( CDaliDFSRetrySlowTests );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( CDaliDFSRetrySlowTests, "CDaliDFSRetrySlowTests" );


class CDaliUtils : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(CDaliUtils);
      CPPUNIT_TEST(testDFSLfn);
    CPPUNIT_TEST_SUITE_END();
public:
    void testDFSLfn()
    {
        const char *lfns[] = { "~foreign::192.168.16.1::scope1::file1",
                               "~file::192.168.16.1::dir1::file1",
                               "~file::192.168.16.1::>some query or another",
                               "~file::192.168.16.1::wild?card1",
                               "~file::192.168.16.1::wild*card2",
                               "~file::192.168.16.1::^C^a^S^e^d",
                               "~file::192.168.16.1::file@cluster1",
                               "~prefix::{multi1*,multi2*}",
                               "{~foreign::192.168.16.1::multi1, ~foreign::192.168.16.2::multi2}",

                               // NB: CDfsLogicalFileName allows these with strict=false (the default)
                               ". :: scope1 :: file",
                               ":: scope1 :: file",
                               "~ scope1 :: scope2 :: file  ",
                               ". :: scope1 :: file nine",
                               ". :: scope1 :: file ten  ",
                               ". :: scope1 :: file",
                               NULL                                             // terminator
                             };
        const char *invalidLfns[] = {
                               ". :: sc~ope1::file",
                               ". ::  ::file",
                               "~~scope1::file",
                               "~sc~ope1::file2",
                               ".:: scope1::file*",
                               NULL                                             // terminator
                             };

        const char *translatedLfns[][2] = {
                               { ".::fname", ".::fname" },
                               { "fname", ".::fname" },
                               { "~.::fname", ".::fname" },
                               { ".::.::.::fname", ".::fname" },
                               { ".::.::.::sname::.::.::fname", "sname::fname" },
                               { "~.::.::scope2::.::.::.::fname", "scope2::fname" },
                               { "{.::.::multitest1f1, .::.::multitest1f2}", "{.::multitest1f1,.::multitest1f2}" },
                               { ".::.::.::.sname.::.::.fname.", ".sname.::.fname." },
                               { "~foreign::192.168.16.1::.::scope1::file1", "foreign::192.168.16.1::scope1::file1" },
                               { "{~foreign::192.168.16.1::.::.::multi1, ~foreign::192.168.16.2::multi2::.::fname}", "{foreign::192.168.16.1::multi1,foreign::192.168.16.2::multi2::fname}" },
                               { nullptr, nullptr }                             // terminator
                             };
        const char *scopes[][2] = {
                               { "~file::127.0.0.1::var::lib::^H^P^C^C^Systems::mydropzone::afile", "file::127.0.0.1::var::lib::^h^p^c^c^systems::mydropzone"},
                               { "~remote::dfs1::ascope::afile", "remote::dfs1::ascope"},
                               { nullptr, nullptr }                            // terminator
        };

        const char *externalUrlChecks[][2] = {
                               { "~file::127.0.0.1::var::lib::^H^P^C^C^Systems::mydropzone::ascope::afile", "/var/lib/HPCCSystems/mydropzone/ascope/afile"},
                               { "~file::10.3.2.1::var::lib::^H^P^C^C^Systems::mydropzone::ascope::afile", "//10.3.2.1/var/lib/HPCCSystems/mydropzone/ascope/afile"},
                               { "~plane::mydropzone::ascope::afile", "/var/lib/HPCCSystems/mydropzone/ascope/afile"},
                               { "~plane::dropzone2::ascope::afile", "//10.4.3.2/var/lib/HPCCSystems/mydropzone/ascope/afile"},
                               { nullptr, nullptr }                            // terminator
        };
        PROGLOG("Checking valid logical filenames");
        unsigned nlfn=0;
        for (;;)
        {
            const char *lfn = lfns[nlfn++];
            if (NULL == lfn)
                break;
            PROGLOG("lfn = %s", lfn);
            CDfsLogicalFileName dlfn;
            try
            {
                dlfn.set(lfn);
            }
            catch (IException *e)
            {
                VStringBuffer err("Logical filename '%s' failed.", lfn);
                EXCLOG(e, err.str());
                e->Release();
                CPPUNIT_FAIL(err.str());
            }
        }
        PROGLOG("Checking translations");
        nlfn = 0;
        for (;;)
        {
            const char **entry = translatedLfns[nlfn++];
            if (nullptr == entry[0])
                break;
            const char *lfn = entry[0];
            const char *expected = entry[1];
            PROGLOG("lfn = %s, expect = %s", lfn, expected);
            CDfsLogicalFileName dlfn;
            StringBuffer err;
            try
            {
                dlfn.set(lfn);
                const char *result = dlfn.get();
                if (!streq(result, expected))
                    err.appendf("Logical filename '%s' should have translated to '%s', but result was '%s'.", lfn, expected, result);
            }
            catch (IException *e)
            {
                err.appendf("Logical filename '%s' failed: ", lfn);
                e->errorMessage(err);
                e->Release();
            }
            if (err.length())
            {
                ERRLOG("%s", err.str());
                CPPUNIT_FAIL(err.str());
            }
        }
        PROGLOG("Checking getScopes");
        nlfn = 0;
        for (;;)
        {
            const char **entry = scopes[nlfn++];
            if (nullptr == entry[0])
                break;
            const char *lfn = entry[0];
            const char *expected = entry[1];
            PROGLOG("lfn = %s, expect scopes = %s", lfn, expected);
            CDfsLogicalFileName dlfn;
            StringBuffer err;
            try
            {
                dlfn.set(lfn);
                StringBuffer result;
                dlfn.getScopes(result);
                if (!streq(result, expected))
                    err.appendf("Logical filename '%s' scopes should be '%s', but result was '%s'.", lfn, expected, result.str());
            }
            catch (IException *e)
            {
                err.appendf("Logical filename '%s' failed: ", lfn);
                e->errorMessage(err);
                e->Release();
            }
            if (err.length())
            {
                ERRLOG("%s", err.str());
                CPPUNIT_FAIL(err.str());
            }
        }

        constexpr const char * globalConfigYaml = R"!!(
        storage:
          planes:
          - name: data
            category: data
            prefix: /var/lib/HPCCSystems/hpcc-data
          - name: mydropzone
            category: lz
            prefix: /var/lib/HPCCSystems/mydropzone
          - name: dropzone2
            category: lz
            prefix: /var/lib/HPCCSystems/mydropzone
            hosts:
            - 10.4.3.2
        )!!";
        Owned<IPropertyTree> globalConfig = createPTreeFromYAMLString(globalConfigYaml);
        replaceComponentConfig(getComponentConfigSP(), globalConfig);

        PROGLOG("Checking physical file paths");
        nlfn = 0;
        for (;;)
        {
            const char **entry = externalUrlChecks[nlfn++];
            if (nullptr == entry[0])
                break;
            const char *lfn = entry[0];
            const char *expected = entry[1];
            PROGLOG("lfn = %s, expect = %s", lfn, expected);
            CDfsLogicalFileName dlfn;
            StringBuffer err;
            try
            {
                dlfn.set(lfn);
                RemoteFilename rfn;
                dlfn.getExternalFilename(rfn);
                StringBuffer filePath;
                rfn.getPath(filePath);
                if (!streq(filePath, expected))
                    err.appendf("Logical filename '%s' external url should be '%s', but result was '%s'.", lfn, expected, filePath.str());
            }
            catch (IException *e)
            {
                err.appendf("Logical filename '%s' failed: ", lfn);
                e->errorMessage(err);
                e->Release();
            }
            if (err.length())
            {
                ERRLOG("%s", err.str());
                CPPUNIT_FAIL(err.str());
            }
        }
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION( CDaliUtils );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( CDaliUtils, "DaliUtils" );

class CFileNameNormalizeUnitTest : public CppUnit::TestFixture, CDfsLogicalFileName
{
    CPPUNIT_TEST_SUITE(CFileNameNormalizeUnitTest);
      CPPUNIT_TEST(testFileNameNormalize);
    CPPUNIT_TEST_SUITE_END();

public:
    void testFileNameNormalize()
    {
        //Columns
        const int inFileName = 0;
        const int normalizedFileName = 1;

        const char *validExternalLfns[][2] = {
                //     input file name                          expected normalized file name
                {"~file::192.168.16.1::dir1::file1",            "file::192.168.16.1::dir1::file1"},
                {"~  file::  192.168.16.1::dir1::file1",        "file::192.168.16.1::dir1::file1"},
                {"~file::192.168.16.1::>some query or another", "file::192.168.16.1::>some query or another"},
                {"~file::192.168.16.1::>Some Query or Another", "file::192.168.16.1::>Some Query or Another"},
                {"~file::192.168.16.1::wild?card1",             "file::192.168.16.1::wild?card1"},
                {"~file::192.168.16.1::wild*card2",             "file::192.168.16.1::wild*card2"},
                {"~file::192.168.16.1::^C^a^S^e^d",             "file::192.168.16.1::^c^a^s^e^d"},
                {nullptr,                                       nullptr }        // terminator
                 };

         const char *validInternalLfns[][2] = {
                 //     input file name                    expected normalized file name
                 {"~foreign::192.168.16.1::scope1::file1", "foreign::192.168.16.1::scope1::file1"},
                 {".::scope1::file",                       "scope1::file"},
                 {"~ scope1 :: scope2 :: file  ",          "scope1::scope2::file"},
                 {". :: scope1 :: file nine",              "scope1::file nine"},
                 {". :: scope1 :: file ten  ",             "scope1::file ten"},
                 {". :: scope1 :: file",                   "scope1::file"},
                 {"~scope1::file@cluster1",                "scope1::file"},
                 {"~scope::^C^a^S^e^d",                    "scope::^c^a^s^e^d"},
                 {"~scope::CaSed",                         "scope::cased"},
                 {"~scope::^CaSed",                        "scope::^cased"},
                 {nullptr,                                 nullptr}   // terminator
                 };

        // Check results
        const bool externalFile = true;
        const bool internalFile = false;
        const bool fileNameMatch = true;

        PROGLOG("Checking external filenames detection and normalization");
        unsigned nlfn=0;
        for (;;)
        {
            const char *lfn = validExternalLfns[nlfn][inFileName];
            if (nullptr == lfn)
                break;
            PROGLOG("lfn = '%s'", lfn);
            StringAttr res;
            try
            {
                ASSERT(externalFile == normalizeExternal(lfn, res, false));
                PROGLOG("res = '%s'", res.str());
                ASSERT(fileNameMatch == streq(res.str(), validExternalLfns[nlfn][normalizedFileName]))
            }
            catch (IException *e)
            {
                VStringBuffer err("External filename '%s' ('%s') failed.", lfn, res.str());
                EXCLOG(e, err.str());
                e->Release();
                CPPUNIT_FAIL(err.str());
            }
            nlfn++;
        }

        PROGLOG("Checking valid internal filenames");
        nlfn=0;
        for (;;)
        {
            const char *lfn = validInternalLfns[nlfn][inFileName];
            if (nullptr == lfn)
                break;
            PROGLOG("lfn = '%s'", lfn);
            StringAttr res;
            try
            {
                ASSERT(internalFile == normalizeExternal(lfn, res, false));
                normalizeName(lfn, res, false, true);
                PROGLOG("res = '%s'", res.str());
                ASSERT(fileNameMatch == streq(res.str(), validInternalLfns[nlfn][normalizedFileName]))
            }
            catch (IException *e)
            {
                VStringBuffer err("Internal filename '%s' ('%s') failed.", lfn, res.str());
                EXCLOG(e, err.str());
                e->Release();
                CPPUNIT_FAIL(err.str());
            }
            nlfn++;
        }
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION( CFileNameNormalizeUnitTest );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( CFileNameNormalizeUnitTest, "CFileNameNormalizeUnitTest" );

#define SOURCE_COMPONENT_UNITTEST "sysinfologger-unittest"

class DaliSysInfoLoggerTester : public CppUnit::TestFixture
{
    /* Note: global messages will be written for dates between 2000-02-04 and 2000-02-05 */
    /* Note: All global messages with time stamp before 2000-03-31 will be deleted */
    CPPUNIT_TEST_SUITE(DaliSysInfoLoggerTester);
        CPPUNIT_TEST(testInit);
        CPPUNIT_TEST(testSysInfoLogger);
    CPPUNIT_TEST_SUITE_END();

    struct TestCase
    {
        LogMsgCategory cat;
        LogMsgCode code;
        bool hidden;
        const char * dateTimeStamp;
        const char * msg;
    };

    const std::vector<TestCase> testCases =
    {
        {
            LogMsgCategory(MSGAUD_operator, MSGCLS_information, DefaultDetail),
            42301,
            false,
            "2000-02-03T10:01:22.342343",
            "CSysInfoLogger Unit test message 1"
        },
        {
            LogMsgCategory(MSGAUD_operator, MSGCLS_information, DefaultDetail),
            42302,
            false,
            "2000-02-03T12:03:42.114233",
            "CSysInfoLogger Unit test message 2"
        },
        {
            LogMsgCategory(MSGAUD_operator, MSGCLS_information, DefaultDetail),
            42303,
            true,
            "2000-02-03T14:02:13.678443",
            "CSysInfoLogger Unit test message 3"
        },
        {
            LogMsgCategory(MSGAUD_operator, MSGCLS_information, DefaultDetail),
            42304,
            true,
            "2000-02-03T16:05:18.8324832",
            "CSysInfoLogger Unit test message 4"
        },
        {
            LogMsgCategory(MSGAUD_operator, MSGCLS_information, DefaultDetail),
            42301,
            false,
            "2000-02-04T03:01:42.5754",
            "CSysInfoLogger Unit test message 5"
        },
        {
            LogMsgCategory(MSGAUD_operator, MSGCLS_information, DefaultDetail),
            42302,
            false,
            "2000-02-04T09:06:25.133132",
            "CSysInfoLogger Unit test message 6"
        },
        {
            LogMsgCategory(MSGAUD_operator, MSGCLS_information, DefaultDetail),
            42303,
            false,
            "2000-02-04T11:09:32.78439",
            "CSysInfoLogger Unit test message 7"
        },
        {
            LogMsgCategory(MSGAUD_operator, MSGCLS_information, DefaultDetail),
            42304,
            true,
            "2000-02-04T13:02:12.82821",
            "CSysInfoLogger Unit test message 8"
        },
        {
            LogMsgCategory(MSGAUD_operator, MSGCLS_information, DefaultDetail),
            42304,
            true,
            "2000-02-04T18:32:11.23421",
            "CSysInfoLogger Unit test message 9"
        }
    };

    struct WrittenLogMessage
    {
        unsigned __int64 msgId;
        unsigned __int64 ts;
        unsigned testCaseIndex;
    };
    std::vector<WrittenLogMessage> writtenMessages;

    unsigned testRead(bool hiddenOnly=false, bool visibleOnly=false, unsigned year=0, unsigned month=0, unsigned day=0)
    {
        unsigned readCount=0;
        try
        {
            std::set<unsigned> matchedMessages; // used to make sure every message written has been read back
            Owned<ISysInfoLoggerMsgIterator> iter = createSysInfoLoggerMsgIterator(visibleOnly, hiddenOnly, year, month, day, SOURCE_COMPONENT_UNITTEST);
            ForEach(*iter)
            {
                const IConstSysInfoLoggerMsg & sysInfoMsg = iter->query();

                if (strcmp(sysInfoMsg.querySource(), SOURCE_COMPONENT_UNITTEST)!=0)
                    continue; // not a message written by this unittest so ignore

                // Check written message matches read message
                unsigned __int64 msgId = sysInfoMsg.queryLogMsgId();
                auto matched = std::find_if(writtenMessages.begin(), writtenMessages.end(), [msgId] (const auto & wm){ return (wm.msgId == msgId); });
                CPPUNIT_ASSERT_MESSAGE("Message read back not matching messages written by this unittest", matched!=writtenMessages.end());

                // Make sure written messages matches message read back
                matchedMessages.insert(matched->testCaseIndex);
                const TestCase & testCase = testCases[matched->testCaseIndex];
                ASSERT(testCase.hidden==sysInfoMsg.queryIsHidden());
                ASSERT(testCase.code==sysInfoMsg.queryLogMsgCode());
                ASSERT(strcmp(testCase.msg,sysInfoMsg.queryMsg())==0);
                ASSERT(testCase.cat.queryAudience()==sysInfoMsg.queryAudience());
                ASSERT(testCase.cat.queryClass()==sysInfoMsg.queryClass());

                readCount++;
            }
            ASSERT(readCount==matchedMessages.size()); // make sure there are no duplicates
        }
        catch (IException *e)
        {
            StringBuffer msg;
            msg.appendf("testRead(hidden=%s, visible=%s) failed: ", boolToStr(hiddenOnly), boolToStr(visibleOnly));
            e->errorMessage(msg);
            msg.appendf("(code %d)", e->errorCode());
            e->Release();
            CPPUNIT_FAIL(msg.str());
        }
        return readCount;
    }

public:
    ~DaliSysInfoLoggerTester()
    {
        daliClientEnd();
    }
    void testInit()
    {
        daliClientInit();
    }
    void testWrite()
    {
        writtenMessages.clear();
        unsigned testCaseIndex=0;
        for (const auto & testCase: testCases)
        {
            try
            {
                CDateTime dateTime;
                dateTime.setString(testCase.dateTimeStamp);

                unsigned __int64 ts = dateTime.getTimeStamp();
                unsigned __int64 msgId = logSysInfoError(testCase.cat, testCase.code, SOURCE_COMPONENT_UNITTEST, testCase.msg, ts);
                writtenMessages.push_back({msgId, ts, testCaseIndex++});
                if (testCase.hidden)
                {
                    Owned<ISysInfoLoggerMsgFilter> msgFilter = createSysInfoLoggerMsgFilter(msgId, SOURCE_COMPONENT_UNITTEST);
                    ASSERT(hideLogSysInfoMsg(msgFilter)==1);
                }
            }
            catch (IException *e)
            {
                StringBuffer msg;
                msg.append("logSysInfoError failed: ");
                e->errorMessage(msg);
                msg.appendf("(code %d)", e->errorCode());
                e->Release();
                CPPUNIT_FAIL(msg.str());
            }
        }
        ASSERT(testCases.size()==writtenMessages.size());
    }
    void testSysInfoLogger()
    {
        // cleanup - remove messages that may have been left over from previous run
        deleteOlderThanLogSysInfoMsg(false, false, 2001, 03, 00, SOURCE_COMPONENT_UNITTEST);
        // Start of tests
        testWrite();
        ASSERT(testRead(false, false)==9);
        ASSERT(testRead(false, false, 2000, 02, 03)==4);
        ASSERT(testRead(false, false, 2000, 02, 04)==5);
        ASSERT(testRead(false, true)==5); //all visible messages
        ASSERT(testRead(true, false)==4); //all hidden messages
        ASSERT(deleteOlderThanLogSysInfoMsg(false, true, 2000, 02, 03, SOURCE_COMPONENT_UNITTEST)==2);
        ASSERT(deleteOlderThanLogSysInfoMsg(true, false, 2000, 02, 04, SOURCE_COMPONENT_UNITTEST)==5);

        // testCase[7] and [8] are the only 2 remaining
        // Delete single message test: delete testCase[7]
        unsigned testCaseId = 7;
        auto matched = std::find_if(writtenMessages.begin(), writtenMessages.end(), [testCaseId] (const auto & wm){ return (wm.testCaseIndex == testCaseId); });
        if (matched==writtenMessages.end())
            throw makeStringExceptionV(-1, "Can't find test case %u in written messages", testCaseId);

        Owned<ISysInfoLoggerMsgFilter> msgFilter = createSysInfoLoggerMsgFilter(matched->msgId, SOURCE_COMPONENT_UNITTEST);
        ASSERT(deleteLogSysInfoMsg(msgFilter)==1);

        // Verify only 1 message remaining
        ASSERT(testRead(false, false)==1);
        // Delete 2000/02/04 and 2000/02/03 (one message but there are 2 parents remaining)
        ASSERT(deleteOlderThanLogSysInfoMsg(false, false, 2000, 02, 05, SOURCE_COMPONENT_UNITTEST)==1);
        // There shouldn't be any records remaining
        ASSERT(testRead(false, false)==0);

        testWrite();

        // delete all messages with MsgCode 42303 -> 3 messages
        msgFilter.setown(createSysInfoLoggerMsgFilter(SOURCE_COMPONENT_UNITTEST));
        msgFilter->setMatchCode(42304);
        ASSERT(deleteLogSysInfoMsg(msgFilter)==3);

        // delete all messages matching source=SOURCE_COMPONENT_UNITTEST
        msgFilter.setown(createSysInfoLoggerMsgFilter(SOURCE_COMPONENT_UNITTEST));
        ASSERT(deleteLogSysInfoMsg(msgFilter)==6);
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION( DaliSysInfoLoggerTester );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( DaliSysInfoLoggerTester, "DaliSysInfoLoggerTester" );




static constexpr bool traceJobQueue = false;
static unsigned jobQueueStartTick = 0;
//The following allows the tests to be slowed down to make it easier to debug problems
static constexpr unsigned tickScaling = 1;
static unsigned getJobQueueTick()
{
    return (msTick() - jobQueueStartTick) / tickScaling;
}
static void jobQueueSleep(unsigned ms)
{
    MilliSleep(ms * tickScaling);
}
class DaliJobQueueTester : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(DaliJobQueueTester);
        CPPUNIT_TEST(testInit);
        CPPUNIT_TEST(testCppServer);
        CPPUNIT_TEST(testSingle);
        CPPUNIT_TEST(testDouble);
        CPPUNIT_TEST(testMany);
        CPPUNIT_TEST(testCleanup);
    CPPUNIT_TEST_SUITE_END();

    static constexpr const char * mainQueueName = "DaliTestJobQueue";
    static constexpr const char * childQueueName = "DaliTestCppQueue";

    struct JobEntry
    {
        unsigned delayMs;
        const char * name;
        unsigned processingMs;
        int priority;
    };

    class JobProcessor : public Thread
    {
    public:
        JobProcessor(Semaphore & _startedSem, Semaphore & _processedSem, IJobQueue * _queue, unsigned _id)
         : startedSem(_startedSem), processedSem(_processedSem), queue(_queue), id(_id)
        {
        }

         virtual int run() override
         {
            startedSem.signal();
            try
            {
                processAll();
            }
            catch (IException * _e)
            {
                e.setown(_e);
            }
            return 0;
         }

         bool processItem(IJobQueueItem * item)
         {
            assertex(item);
            const char * name = item->queryWUID();
            if (traceJobQueue)
                DBGLOG("===%s===@%u", name, getJobQueueTick());
            if (name[0] == '!')
                return false;
            output.append(name);
            if (!log.isEmpty())
                log.append(",");
            log.append(name).append("@").append(getJobQueueTick());
            unsigned delay = item->getPort();
            jobQueueSleep(delay);
            processedSem.signal();
            return true;
         }

        const char * queryOutput()
        {
            if (e)
                throw e.getClear();
            return output.str();
        }

        const char * queryLog()
        {
            return log.str();
        }

        virtual void processAll() = 0;

    public:
        Semaphore & startedSem;
        Semaphore & processedSem;
        Linked<IJobQueue> queue;
        StringBuffer output;
        StringBuffer log;
        Owned<IException> e;
        unsigned id;
    };

    // Read the first (highest priority) item on the queue - no minimum priority
    class StandardJobProcessor : public JobProcessor
    {
    public:
        StandardJobProcessor(Semaphore & _startedSem, Semaphore & _processedSem, IJobQueue * _queue, unsigned _id)
        : JobProcessor(_startedSem, _processedSem, _queue, _id)
        {
        }

        virtual void processAll() override
        {
            for (;;)
            {
                Owned<IJobQueueItem> item = queue->dequeue();
                if (!processItem(item))
                    break;
            }
        }

    };

    // Read the first item on the queue, but wait for 200ms to see if there is an item queued with a prioprity >= the last item that was dequeued.
    // This is only used for bare metal, and I'm not sure the semantics are very helpful.  I think NewThor may be better approach.
    class ThorJobProcessor : public JobProcessor
    {
    public:
        ThorJobProcessor(Semaphore & _startedSem, Semaphore & _processedSem, IJobQueue * _queue, unsigned _id)
        : JobProcessor(_startedSem, _processedSem, _queue, _id)
        {
        }

        virtual void processAll() override
        {
            for (;;)
            {
                Owned<IJobQueueItem> item = queue->dequeue(0, INFINITE, 200*tickScaling);
                bool ret = processItem(item);
                if (!ret)
                    break;
            }
        }
    };

    // For 200ms check to see if there is an item queued with the same priority that this thread last dequeued.  Then wait for any item.
    // This means if there is a single high priority job, the other threads do not wait for that high priority job.
    class NewThorJobProcessor : public JobProcessor
    {
    public:
        NewThorJobProcessor(Semaphore & _startedSem, Semaphore & _processedSem, IJobQueue * _queue, unsigned _id)
        : JobProcessor(_startedSem, _processedSem, _queue, _id)
        {
        }

        virtual void processAll() override
        {
            for (;;)
            {
                Owned<IJobQueueItem> item = queue->dequeue(lastPrio, 200*tickScaling, 0);
                if (!item)
                    item.setown(queue->dequeue(0, INFINITE, 0));
                lastPrio = item->getPriority();
                bool ret = processItem(item);
                if (!ret)
                    break;
            }
        }

    protected:
        int lastPrio = 0;

    };

    class PriorityJobProcessor : public JobProcessor
    {
    public:
        PriorityJobProcessor(Semaphore & _startedSem, Semaphore & _processedSem, IJobQueue * _queue, unsigned _id)
        : JobProcessor(_startedSem, _processedSem, _queue, _id)
        {
        }

        virtual void processAll() override
        {
            __uint64 priority = 0;
            for (;;)
            {
                Owned<IJobQueueItem> item = queue->dequeuePriority(priority);
                bool ret = processItem(item);
                if (!ret)
                    break;
                priority = getTimeStampNowValue();
            }
        }
    };

    //This class mimics the behaviour of the c++ job that is started to compile a single workunit.
    //It reads items off a child queue and lingers to see if any other workunits are ready to be compiled.
    class CppJobProcessor : public JobProcessor
    {
    public:
        CppJobProcessor(Semaphore & _startedSem, Semaphore & _processedSem, IJobQueue * _queue, unsigned _id)
        : JobProcessor(_startedSem, _processedSem, _queue, _id)
        {
        }

        virtual void processAll() override
        {
            for (;;)
            {
                __uint64 priority = getTimeStampNowValue();
                unsigned cppTimeout = 100 * tickScaling;
                Owned<IJobQueueItem> item = queue->dequeuePriority(priority, cppTimeout);
                //Check for time out - linger period has expired so this job will exit
                if (!item)
                {
                    output.append("*");
                    log.append("*");
                    break;
                }
                bool ret = processItem(item);
                if (!ret)
                {
                    //Test case has finished - requeue the item so the calling eclccserver will
                    //also terminate, and then exit.
                    queue->setActiveQueue(mainQueueName);
                    queue->enqueue(item.getClear());
                    break;
                }
            }
        }
    };

    //This class mimics the behaviour of the cppserver.
    //It listens on the main queue with a default priority.  When it gets an item it adds it to a child
    //queue and then starts a job to process items from that queue.  (This may be picked up by a job that
    //is already running - deliberate design.)  It waits until that job terminates and then listens to
    //the queue again.
    class CppServerJobProcessor : public JobProcessor
    {
    public:
        CppServerJobProcessor(Semaphore & _startedSem, Semaphore & _processedSem, IJobQueue * _queue, unsigned _id)
        : JobProcessor(_startedSem, _processedSem, _queue, _id)
        {
        }

        virtual void processAll() override
        {
            Owned<IJobQueue> childQueue = createJobQueue(childQueueName);
            for (;;)
            {
                Owned<IJobQueueItem> item = queue->dequeue();
                assertex(item);
                const char * name = item->queryWUID();
                if (name[0] == '!')
                    break;

                childQueue->enqueue(item.getClear());

                StringBuffer cppQueues;
                cppQueues.append(mainQueueName).append(",").append(childQueueName);

                Owned<IJobQueue> localQueues = createJobQueue(cppQueues);
                Semaphore dummySem;
                Owned<JobProcessor> child = new CppJobProcessor(dummySem, processedSem, localQueues, id);

                child->start(true);
                child->join();
                output.append(child->output);
                log.append(child->log);
            }
        }
    };

    enum JobProcessorType
    {
        StandardProcessor,
        ThorProcessor,
        NewThorProcessor,
        PriorityProcessor,
        CppServerProcessor,
    };

    void testInit()
    {
        daliClientInit();
    }

    void testCleanup()
    {
        daliClientEnd();
    }

    void runTestCase(const char * name, const std::initializer_list<JobEntry> & jobs, const std::initializer_list<JobProcessorType> & processors, const std::initializer_list<const char *> & expectedResults)
    {
        try
        {
            Owned<IJobQueue> queue = createJobQueue(mainQueueName);
            queue->connect(true);
            queue->clear();

            Semaphore startedSem;
            Semaphore processedSem;

            CIArrayOf<JobProcessor> jobProcessors;
            for (auto & processor : processors)
            {
                JobProcessor * cur = nullptr;
                //All listening threads must have a unique queue objects
                Owned<IJobQueue> localQueue = createJobQueue(mainQueueName);

                switch (processor)
                {
                case StandardProcessor:
                    cur = new StandardJobProcessor(startedSem, processedSem, localQueue, jobProcessors.ordinality());
                    break;
                case ThorProcessor:
                    cur = new ThorJobProcessor(startedSem, processedSem, localQueue, jobProcessors.ordinality());
                    break;
                case NewThorProcessor:
                    cur = new NewThorJobProcessor(startedSem, processedSem, localQueue, jobProcessors.ordinality());
                    break;
                case PriorityProcessor:
                    cur = new PriorityJobProcessor(startedSem, processedSem, localQueue, jobProcessors.ordinality());
                    break;
                case CppServerProcessor:
                    cur = new CppServerJobProcessor(startedSem, processedSem, localQueue, jobProcessors.ordinality());
                    break;
                default:
                    UNIMPLEMENTED;
                }
                jobProcessors.append(*cur);
                cur->start(true);
            }

            for (auto & processor : processors)
                startedSem.wait();

            IArrayOf<IConversation> conversations;
            jobQueueStartTick = msTick();
            for (auto & job : jobs)
            {
                jobQueueSleep(job.delayMs);
                if (traceJobQueue)
                    DBGLOG("Add (%s, %d, %d) @%u", job.name, job.delayMs, job.processingMs, getJobQueueTick());
                if (job.name)
                {
                    Owned<IJobQueueItem> item = createJobQueueItem(job.name);
                    item->setPort(job.processingMs);
                    item->setPriority(job.priority);

                    queue->enqueue(item.getClear());
                }
            }

            for (;;)
            {
                //Wait until all the items have been processed before adding the special end markers
                //otherwise the ends will be interpreted as valid items, and may cause the items to
                //be dequeued by the wrong thread.
                unsigned connected;
                unsigned waiting;
                unsigned enqueued;
                queue->getStats(connected,waiting,enqueued);
                if (enqueued == 0)
                    break;
                MilliSleep(100 * tickScaling);
            }

            ForEachItemIn(i1, jobProcessors)
            {
                if (traceJobQueue)
                    DBGLOG("Add (eoj) @%u", getJobQueueTick());

                //The queue code dedups by "wuid", so we need to add a unique "stop" entry
                std::string end = std::string("!") + std::to_string(i1);
                Owned<IJobQueueItem> item = createJobQueueItem(end.c_str());
                queue->enqueue(item.getClear());
            }

            ForEachItemIn(i2, jobProcessors)
            {
                if (traceJobQueue)
                    DBGLOG("Wait for %u", i2);
                jobProcessors.item(i2).join();
            }

            DBGLOG("%s: %ums", name, getJobQueueTick());
            ForEachItemIn(i3, jobProcessors)
            {
                JobProcessor & cur = jobProcessors.item(i3);
                DBGLOG("  Result: '%s' '%s'", cur.queryOutput(), cur.queryLog());
            }

            ForEachItemIn(i4, jobProcessors)
            {
                //If expected results are provided, check that the result matches one of them (it is undefined which
                //processor will match which result)
                JobProcessor & cur = jobProcessors.item(i4);
                if (expectedResults.size())
                {
                    bool matched = false;
                    StringBuffer expectedText;
                    for (auto & expected : expectedResults)
                    {
                        if (streq(expected, cur.queryOutput()))
                        {
                            matched = true;
                            break;
                        }
                        expectedText.append(", ").append(expected);
                    }
                    if (!matched)
                    {
                        DBGLOG("Test %s: No match for output %u: %s", name, i4, expectedText.str()+2);
                        CPPUNIT_ASSERT_MESSAGE("Result does not match any of the expected results", false);
                    }
                }
            }
        }
        catch (IException * e)
        {
            StringBuffer msg("Fail: ");
            e->errorMessage(msg);
            e->Release();
            CPPUNIT_ASSERT_MESSAGE(msg.str(), 0);
        }
    }

    static constexpr std::initializer_list<JobEntry> singleWuTest = {
        { 0, "a", 90, 0 },
        { 100, "b", 90, 0 },
        { 100, "c", 90, 0 },
        { 100, "d", 90, 0 },
    };

    static constexpr std::initializer_list<JobEntry> dripSingleTest = {
        { 0, "a", 10, 0 },
        { 200, "b", 10, 0 },
        { 200, "c", 10, 0 },
        { 200, "d", 10, 0 },
        { 20, nullptr, 0, 0 },      // Ensure d has completed before termination is sent
    };

    static constexpr std::initializer_list<JobEntry> twoWuTest = {
        { 0, "a", 90, 0 },
        { 50, "A", 90, 0},
        { 50, "b", 90, 0 },
        { 50, "B", 90, 0 },
        { 50, "c", 90, 0 },
        { 50, "C", 90, 0 },
        { 50, "d", 90, 0 },
        { 50, "D", 90, 0 },
    };

    static constexpr std::initializer_list<JobEntry> lowHighTest = {
        { 0, "a", 90, 0 },
        { 50, "A", 90, 1},
        { 50, "b", 90, 0 },
        { 50, "B", 90, 1 },
        { 50, "c", 90, 0 },
        { 50, "C", 90, 1 },
        { 50, "d", 90, 0 },
        { 50, "D", 90, 1 },
    };

    static constexpr std::initializer_list<JobEntry> lowHigh2Test = {
        { 0, "a", 90, 0 },
        { 50, "A", 90, 1},
        { 10, "b", 90, 0 },
        { 10, "B", 90, 1 },
        { 10, "c", 90, 0 },
        { 10, "C", 90, 1 },
        { 10, "d", 90, 0 },
        { 10, "D", 90, 1 },
    };

    static constexpr std::initializer_list<JobEntry> lowHigh3Test = {
        { 0, "a", 90, 0 },
        { 50, "A", 90, 1},
        { 10, "b", 90, 0 },
    };

    static constexpr std::initializer_list<JobEntry> dripFeedTest = {
        { 0, "a", 10, 0 },
        { 100, "b", 10, 0},
        { 100, "c", 10, 0},
        { 100, "d", 10, 0},
        { 100, "e", 10, 0},
        { 100, "f", 10, 0},
        { 100, "g", 10, 0},
        { 100, "h", 10, 0},
        { 100, "i", 10, 0},
        { 100, "j", 10, 0},
    };

    static constexpr std::initializer_list<JobEntry> drip2FeedTest = {
        {  0, "a", 60, 0 },
        { 50, "b", 60, 0},
        { 50, "c", 60, 0},
        { 50, "d", 60, 0},
        { 50, "e", 60, 0},
        { 50, "f", 60, 0},
        { 50, "g", 60, 0},
        { 50, "h", 60, 0},
        { 50, "i", 60, 0},
        { 50, "j", 60, 0},
        { 50, "k", 60, 0},
        { 50, "l", 60, 0},
        { 50, "m", 60, 0},
        { 50, "n", 60, 0},
        { 50, "o", 60, 0},
    };

    static constexpr std::initializer_list<JobEntry> Cpp1Test = {
        {  0, "a", 50, 0 },
        { 60, "b", 50, 0},
        { 60, "c", 50, 0},
        { 60, "d", 50, 0},
        { 60, "e", 50, 0},
        { 10, "f", 80, 0},
        { 50, "g", 50, 0},
        { 40, "h", 50, 0},
        { 60, "i", 50, 0},
        { 60, "j", 50, 0},
        { 200, nullptr, 0, 0 },
    };

    static constexpr std::initializer_list<JobEntry> Cpp2Test = {
        {  0, "a", 50, 0 },
        { 60, "b", 50, 0},
        { 10, "c", 50, 0},
        { 10, "d", 50, 0},
        { 10, "e", 50, 0},
        { 10, "f", 50, 0},
        { 70, "g", 50, 0},
        { 20, "h", 50, 0},
        { 60, "i", 50, 0},
        { 200, nullptr, 0, 0 },
    };

    static constexpr std::initializer_list<JobEntry> Cpp3Test = {
        {  0, "a", 50, 0 },
        { 10, "b", 50, 0},
        { 40, "c", 50, 0},
        { 20, "d", 50, 0},
        { 70, "e", 50, 0},
        { 60, "f", 50, 0},
        { 60, "g", 50, 0},
        { 10, "h", 50, 0},
        { 10, "i", 50, 0},
        { 10, "j", 50, 0},
        { 100, "k", 50, 0},
        { 200, nullptr, 0, 0 },
    };


    //MODEL The way that cpp server should work - an agent that listens to the main queue, and jobs that listen to the main queue and a child queue
    //Timings are very temperamental - and if there are a choice of threads to restart processing then it isn't well defined which one will pick it up
    void testCppServer()
    {
        runTestCase("single, 1 c++", singleWuTest, { CppServerProcessor }, { "abcd" });
        runTestCase("drip, 1 c++", dripSingleTest, { CppServerProcessor }, { "a*b*c*d" });
        runTestCase("single, 2 c++", singleWuTest, { CppServerProcessor, CppServerProcessor }, { "abcd", "" });
        runTestCase("cpp, 2 c++", Cpp1Test, { CppServerProcessor, CppServerProcessor }, { "abcdeg*", "fhij*" });
        runTestCase("cpp, 3 c++", Cpp1Test, { CppServerProcessor, CppServerProcessor, CppServerProcessor }, { "abcdeg*", "fhij*", "" });
        runTestCase("cpp2, 3 c++", Cpp2Test, { CppServerProcessor, CppServerProcessor, CppServerProcessor }, { "abeg*", "cfhi*", "d*" });
        runTestCase("cpp3, 2 c++", Cpp3Test, { CppServerProcessor, CppServerProcessor }, { "ac*hjk*", "bdefgi*" });
    }
    void testSingle()
    {
        runTestCase("1 wu, 1 standard", singleWuTest, { StandardProcessor }, { "abcd" });
        runTestCase("2 wu, 1 standard", twoWuTest, { StandardProcessor }, { "aAbBcCdD" });
        runTestCase("lo hi wu, 1 standard", lowHighTest, { StandardProcessor }, { "aABCDbcd" });
        runTestCase("lo hi2 wu, 1 standard", lowHigh2Test, { StandardProcessor }, { "aABCDbcd" });
        runTestCase("lo hi2 wu, 1 thor", lowHigh2Test, { ThorProcessor }, {});
        runTestCase("lo hi2 wu, 1 newthor", lowHigh2Test, { NewThorProcessor }, {});
        runTestCase("drip wu, 1 std", dripFeedTest, { StandardProcessor }, { "abcdefghij" });
        runTestCase("drip wu, 1 std", dripFeedTest, { PriorityProcessor }, { "abcdefghij" });
    }

    void testDouble()
    {
        runTestCase("2 wu, 2 standard", twoWuTest, { StandardProcessor, StandardProcessor }, { "abcd", "ABCD" });
        runTestCase("lo hi wu, 2 standard", lowHighTest, { StandardProcessor, StandardProcessor }, { "abcd", "ABCD" });
        runTestCase("lo hi2  wu, 2 standard", lowHigh2Test, { StandardProcessor, StandardProcessor }, { "aBDc", "ACbd" });
        runTestCase("lo hi2  wu, 2 thor", lowHigh2Test, { ThorProcessor, ThorProcessor }, { "aBDc", "ACbd" });
        runTestCase("lo hi2  wu, 2 newthor", lowHigh2Test, { NewThorProcessor, NewThorProcessor }, {});

        runTestCase("lo hi3  wu, 2 thor", lowHigh3Test, { ThorProcessor, ThorProcessor }, {});
        runTestCase("lo hi3  wu, 2 newthor", lowHigh3Test, { NewThorProcessor, NewThorProcessor }, {});
        runTestCase("lo hi3  wu, 2 prio", lowHigh3Test, { PriorityProcessor, PriorityProcessor }, {});
        runTestCase("drip wu, 2 std", dripFeedTest, { StandardProcessor, StandardProcessor }, {});
        runTestCase("drip wu, 2 newthor", dripFeedTest, { NewThorProcessor, NewThorProcessor }, {});
        runTestCase("drip wu, 2 prio", dripFeedTest, { PriorityProcessor, PriorityProcessor }, { "abcdefghij", "" });
    }

    void testMany()
    {
        runTestCase("drip wu, 3 std", dripFeedTest, { StandardProcessor, StandardProcessor, StandardProcessor }, {});
        runTestCase("drip2 wu, 3 std", drip2FeedTest, { StandardProcessor, StandardProcessor, StandardProcessor }, {});
        runTestCase("drip wu, 3 prio", dripFeedTest, { PriorityProcessor, PriorityProcessor, PriorityProcessor }, { "abcdefghij", "", "" });
        runTestCase("drip2 wu, 3 prio", drip2FeedTest, { PriorityProcessor, PriorityProcessor, PriorityProcessor }, { "acegikmo", "bdfhjln", ""});
    }

    //MORE Tests:
    //Many requests at a time in waves
    //Priority 1,2,3 fixed - not dynamic
    //Stopping listening after N to check priorities removed correctly
    //Mix standard and priority
    //Priority with expiring and gaps to ensure the correct client picks up the items.
};

CPPUNIT_TEST_SUITE_REGISTRATION( DaliJobQueueTester );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( DaliJobQueueTester, "DaliJobQueueTester" );

//---------------------------------------------------------------------------------------------------------------------

class GlobalMetricDumper : implements IGlobalMetricRecorder
{
public:
    //MORE: Need to pass the start and end time.
    virtual void processGlobalStatistics(const char * category, const MetricsDimensionList & dimensions, const char * startTime, const char * endTime, const GlobalStatisticsList & stats) override
    {
        //Only take account of metrics that are part of the unit test
        if (!startsWith(category, "testCategory"))
            return;

        out.append(category).append("[");
        if (dimensions.size())
        {
            for (const auto & dimension : dimensions)
            {
                out.append(dimension.first).append("=");
                out.append(dimension.second).append(",");
            }
            out.setLength(out.length()-1);
        }

        out.appendf("] (%s..%s) => {", startTime, endTime);

        if (stats.size())
        {
            for (const auto & stat : stats)
            {
                out.append(queryStatisticName(stat.first)).append("=");
                out.append(stat.second).append(",");
            }
            out.setLength(out.length()-1);
        }

        out.append("}").newline();
    }

public:
    StringBuffer out;
};



class DaliGlobalMetricsTester : public CppUnit::TestFixture
{
    /* Note: global messages will be written for dates between 2000-02-04 and 2000-02-05 */
    /* Note: All global messages with time stamp before 2000-03-31 will be deleted */
    CPPUNIT_TEST_SUITE(DaliGlobalMetricsTester);
        CPPUNIT_TEST(doStart);
        CPPUNIT_TEST(testGlobalMetrics);
        CPPUNIT_TEST(testTimeslots);
        CPPUNIT_TEST(testMultiThread);
        CPPUNIT_TEST(doStop);
    CPPUNIT_TEST_SUITE_END();

public:
    void doStart()
    {
        daliClientInit();
    }
    void doStop()
    {
        daliClientEnd();
    }
    void verifyGlobalMetrics(const char * optCategory, const MetricsDimensionList & optDimensions, const CDateTime & startTime, const CDateTime * endTime, const char * expected)
    {
        CDateTime now;
        if (!endTime)
        {
            now.setNow();
            endTime = &now;
        }

        GlobalMetricDumper visitor;
        visitor.out.newline();  // start with a newline to allow cleaner definitions of the expected output
        gatherGlobalMetrics(optCategory, optDimensions, startTime, *endTime, visitor);
        if (expected)
            CPPUNIT_ASSERT_EQUAL_STR(expected, visitor.out.str());
        else
            DBGLOG("Results: %s", visitor.out.str());
    }

    void testGlobalMetrics()
    {
        CDateTime startTime;
        startTime.setString("1999-01-13T12:00:00", nullptr, false);

        setGlobalMetricNowTime("1999-01-13T12:00:00");
        try
        {
            resetGlobalMetrics("testCategory", MetricsDimensionList());
            resetGlobalMetrics("testCategory2", MetricsDimensionList());

            recordGlobalMetrics("testCategory", MetricsDimensionList{}, { StTimeBlocked }, { 10000 });

            verifyGlobalMetrics(nullptr, MetricsDimensionList{}, startTime, nullptr,
                                "\ntestCategory[] (1999011312..1999011312) => {TimeBlocked=10000}\n");

            recordGlobalMetrics("testCategory", MetricsDimensionList{}, { StTimeBlocked, StTimeLocalExecute }, { 10000, 12345 });
            recordGlobalMetrics("testCategory", MetricsDimensionList{{"user","gavin"},{"instance","thor400"}}, { StTimeLocalExecute, StCostExecute }, { 54321, 1290 });
            recordGlobalMetrics("testCategory", MetricsDimensionList{{"user","gavin"}}, { StTimeLocalExecute, StCostExecute }, { 11111, 999 });

            {
                constexpr const char * expected = R"!(
testCategory[] (1999011312..1999011312) => {TimeBlocked=20000,TimeLocalExecute=12345}
testCategory[user=gavin,instance=thor400] (1999011312..1999011312) => {TimeLocalExecute=54321,CostExecute=1290}
testCategory[user=gavin] (1999011312..1999011312) => {TimeLocalExecute=11111,CostExecute=999}
)!";
                verifyGlobalMetrics(nullptr, MetricsDimensionList{}, startTime, nullptr, expected);
            }

            recordGlobalMetrics("testCategory2", MetricsDimensionList{{"plane", "data1"},{"user","gavin"}}, { StCostFileAccess }, { 11111 });
            recordGlobalMetrics("testCategory2", MetricsDimensionList{{"plane", "data2"},{"user","gavin"}}, { StCostFileAccess }, { 22222 });
            recordGlobalMetrics("testCategory2", MetricsDimensionList{{"plane", "data2"},{"user","jim"}}, { StCostFileAccess }, { 33333 });
            recordGlobalMetrics("testCategory2", MetricsDimensionList{{"plane", "data3"},{"user","bob"}}, { StCostFileAccess }, { 9999 });
            recordGlobalMetrics("testCategory2", MetricsDimensionList{{"plane", "data1"},{"user","gavin"}}, { StCostFileAccess }, { 11111 });
            recordGlobalMetrics("testCategory2", MetricsDimensionList{{"plane", "data2"},{"user","gavin"}}, { StCostFileAccess }, { 22222 });
            recordGlobalMetrics("testCategory2", MetricsDimensionList{{"plane", "data2"},{"user","jim"}}, { StCostFileAccess }, { 33333 });
            recordGlobalMetrics("testCategory2", MetricsDimensionList{{"plane", "data2"},{"user","gavin"}}, { StCostFileAccess }, { 22222 });
            recordGlobalMetrics("testCategory2", MetricsDimensionList{{"plane", "data2"},{"user","jim"}}, { StCostFileAccess }, { 33333 });

            {
                constexpr const char * expected = R"!(
testCategory[] (1999011312..1999011312) => {TimeBlocked=20000,TimeLocalExecute=12345}
testCategory[user=gavin,instance=thor400] (1999011312..1999011312) => {TimeLocalExecute=54321,CostExecute=1290}
testCategory[user=gavin] (1999011312..1999011312) => {TimeLocalExecute=11111,CostExecute=999}
testCategory2[plane=data1,user=gavin] (1999011312..1999011312) => {CostFileAccess=22222}
testCategory2[plane=data2,user=gavin] (1999011312..1999011312) => {CostFileAccess=66666}
testCategory2[plane=data2,user=jim] (1999011312..1999011312) => {CostFileAccess=99999}
testCategory2[plane=data3,user=bob] (1999011312..1999011312) => {CostFileAccess=9999}
)!";
                verifyGlobalMetrics(nullptr, MetricsDimensionList{}, startTime, nullptr, expected);
            }
        }
        catch (IException * e)
        {
            StringBuffer msg;
            e->errorMessage(msg);
            e->Release();
            CPPUNIT_FAIL(msg.str());
        }
    }

    void testTimeslots()
    {
        const char * slot1 = "1999-01-13T12:00:00";
        const char * slot1End = "1999-01-13T12:59:99";
        const char * slot1b = "1999-01-13T12:58:00";
        const char * slot2 = "1999-01-13T13:00:00";
        const char * slot3 = "1999-01-14T13:00:00";
        const char * slot4 = "2000-01-14T13:00:00";

        CDateTime slot1Time;
        slot1Time.setString(slot1);
        CDateTime slot1EndTime;
        slot1EndTime.setString(slot1End);
        CDateTime slot2Time;
        slot2Time.setString(slot2);
        CDateTime slot3Time;
        slot3Time.setString(slot3);
        CDateTime slot4Time;
        slot4Time.setString(slot4);

        try
        {
            resetGlobalMetrics("testCategory", MetricsDimensionList());
            resetGlobalMetrics("testCategory2", MetricsDimensionList());

            setGlobalMetricNowTime(slot1);
            recordGlobalMetrics("testCategory", MetricsDimensionList{}, { StTimeLocalExecute }, { 10 });
            setGlobalMetricNowTime(slot1b);
            recordGlobalMetrics("testCategory", MetricsDimensionList{}, { StTimeLocalExecute }, { 20 });

            setGlobalMetricNowTime(slot2);
            recordGlobalMetrics("testCategory2", MetricsDimensionList{}, { StTimeLocalExecute }, { 40 });

            setGlobalMetricNowTime(slot3);
            recordGlobalMetrics("testCategory2", MetricsDimensionList{}, { StTimeLocalExecute }, { 80 });

            setGlobalMetricNowTime(slot4);
            recordGlobalMetrics("testCategory", MetricsDimensionList{}, { StTimeLocalExecute }, { 160 });

            {
                constexpr const char * expected = R"!(
testCategory[] (1999011312..1999011312) => {TimeLocalExecute=30}
testCategory[] (2000011413..2000011413) => {TimeLocalExecute=160}
testCategory2[] (1999011313..1999011313) => {TimeLocalExecute=40}
testCategory2[] (1999011413..1999011413) => {TimeLocalExecute=80}
)!";
                verifyGlobalMetrics(nullptr, MetricsDimensionList{}, slot1Time, nullptr, expected);
            }

            {
                constexpr const char * expected = R"!(
)!";
                verifyGlobalMetrics("unknown", MetricsDimensionList{}, slot1Time, nullptr, expected);
            }

            {
                constexpr const char * expected = R"!(
testCategory[] (1999011312..1999011312) => {TimeLocalExecute=30}
testCategory[] (2000011413..2000011413) => {TimeLocalExecute=160}
)!";
                verifyGlobalMetrics("testCategory", MetricsDimensionList{}, slot1Time, nullptr, expected);
            }

            {
                constexpr const char * expected = R"!(
testCategory[] (1999011312..1999011312) => {TimeLocalExecute=30}
)!";
                verifyGlobalMetrics(nullptr, MetricsDimensionList{}, slot1Time, &slot1Time, expected);
            }

            {
                constexpr const char * expected = R"!(
testCategory[] (1999011312..1999011312) => {TimeLocalExecute=30}
)!";
                verifyGlobalMetrics(nullptr, MetricsDimensionList{}, slot1Time, &slot1EndTime, expected);
            }

            {
                constexpr const char * expected = R"!(
testCategory[] (1999011312..1999011312) => {TimeLocalExecute=30}
testCategory2[] (1999011313..1999011313) => {TimeLocalExecute=40}
)!";
                verifyGlobalMetrics(nullptr, MetricsDimensionList{}, slot1Time, &slot2Time, expected);
            }

        }
        catch (IException * e)
        {
            StringBuffer msg;
            e->errorMessage(msg);
            e->Release();
            CPPUNIT_FAIL(msg.str());
        }
    }

    class GlobalMetricReporter : public Thread
    {
    public:
        GlobalMetricReporter(const char * _queueName, unsigned _numIterations) : queueName(_queueName), numIterations(_numIterations)
        {
        }

        virtual int run()
        {
            sem.wait();
            for (unsigned i = 0; i < numIterations; i++)
            {
                recordGlobalMetrics("testCategory", MetricsDimensionList{{"component","thor"},{"name",queueName.str()}}, { StTimeLocalExecute, StNumStarts }, { 1234, i });
                MilliSleep(0);
            }
            return 0;
        }

    public:
        StringAttr queueName;
        Semaphore sem;
        unsigned numIterations;
    };

    void testMultiThread()
    {
        const unsigned numQueues = 4;
        const unsigned numThreadsPerQueue = 20;
        const unsigned numIterations = 200;

        CDateTime startTime;
        startTime.setString("1999-01-13T12:00:00", nullptr, false);
        setGlobalMetricNowTime("1999-01-13T12:00:00");

        resetGlobalMetrics("testCategory", MetricsDimensionList());
        resetGlobalMetrics("testCategory2", MetricsDimensionList());

        CIArrayOf<GlobalMetricReporter> threads;
        for (unsigned queue=0; queue < numQueues; queue++)
        {
            VStringBuffer queueName("queue%u", queue);
            for (unsigned thread=0; thread < numThreadsPerQueue; thread++)
            {
                threads.append(*new GlobalMetricReporter(queueName, numIterations));
                threads.tos().start(true);
            }
        }

        CCycleTimer timer;
        ForEachItemIn(i, threads)
            threads.item(i).sem.signal();

        ForEachItemIn(i2, threads)
            threads.item(i2).join();

        __uint64 elapsedNs = timer.elapsedNs();
        unsigned numEvents = numQueues * numThreadsPerQueue * numIterations;
        DBGLOG("Processing %u events took %lluns (%llu per event)", numEvents, elapsedNs, elapsedNs / numEvents);
        {
            constexpr const char * expected = nullptr;
            DBGLOG("Expected TimeLocalExecute=%u NumStarts=%u", 1234 * numThreadsPerQueue * numIterations, numThreadsPerQueue * numIterations * (numIterations - 1) / 2);
            verifyGlobalMetrics(nullptr, MetricsDimensionList{}, startTime, nullptr, expected);
        }

    }
};

CPPUNIT_TEST_SUITE_REGISTRATION( DaliGlobalMetricsTester );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( DaliGlobalMetricsTester, "DaliGlobalMetricsTester" );

//---------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------

class DaliBinaryDataTester : public CppUnit::TestFixture
{
    /* Note: global messages will be written for dates between 2000-02-04 and 2000-02-05 */
    /* Note: All global messages with time stamp before 2000-03-31 will be deleted */
    CPPUNIT_TEST_SUITE(DaliBinaryDataTester);
        CPPUNIT_TEST(doStart);
        CPPUNIT_TEST(testSimpleBinary);
        CPPUNIT_TEST(testCompression);
        CPPUNIT_TEST(doCleanup);
        CPPUNIT_TEST(doStop);
    CPPUNIT_TEST_SUITE_END();

public:
    void doStart()
    {
        daliClientInit();
    }
    void doCleanup()
    {
        Owned<IRemoteConnection> conn = querySDS().connect("/testext", myProcessSession(), RTM_DELETE_ON_DISCONNECT, 2000);
    }
    void doStop()
    {
        daliClientEnd();
    }

    void testSimpleBinary(size32_t size, CompressionMethod compression)
    {
        DBGLOG("testBinary %u %s", size, translateFromCompMethod(compression));

        Owned<IRemoteConnection> conn = querySDS().connect("/testext", myProcessSession(), RTM_CREATE, 2000);
        IPropertyTree *root = conn->queryRoot();
        MemoryBuffer mb;
        // fill mem buffer with random data - 100k
        void *mem = mb.reserveTruncate(size);
        for (size32_t i = 0; i < size; i++)
            ((byte *)mem)[i] = (i & 256) ? rand() : i;      // ensure some data is not random
        root->setPropBin("bin", mb.length(), mb.toByteArray(), compression);

        MemoryBuffer result;
        root->getPropBin("bin", result);

        CPPUNIT_ASSERT_EQUAL(mb.length(), result.length());
        CPPUNIT_ASSERT(memcmp(mb.toByteArray(), result.toByteArray(), mb.length()) == 0);
    }

    void testDaliBinary(size32_t size, CompressionMethod compression)
    {
        DBGLOG("testDaliBinary %u %s", size, translateFromCompMethod(compression));

        Owned<IRemoteConnection> conn = querySDS().connect("/testext", myProcessSession(), RTM_CREATE, 2000);
        IPropertyTree *root = conn->queryRoot();
        MemoryBuffer mb;
        // fill mem buffer with random data - 100k
        void *mem = mb.reserveTruncate(size);
        for (size32_t i = 0; i < size; i++)
            ((byte *)mem)[i] = (i & 256) ? rand() : i;      // ensure some data is not random
        root->setPropBin("bin", mb.length(), mb.toByteArray(), compression);
        conn->commit();
        conn.clear();

        conn.setown(querySDS().connect("/testext", myProcessSession(), RTM_LOCK_READ, 2000));
        root = conn->queryRoot();
        Owned<IPropertyTree> clonedRoot = createPTreeFromIPT(root);

        MemoryBuffer result;
        clonedRoot->getPropBin("bin", result);

        CPPUNIT_ASSERT_EQUAL(mb.length(), result.length());
        CPPUNIT_ASSERT(memcmp(mb.toByteArray(), result.toByteArray(), mb.length()) == 0);
    }

    void testSimpleBinary()
    {
        for (size32_t size=0x100; size < 0x1000000; size *= 2)
        {
            testSimpleBinary(size, COMPRESS_METHOD_NONE);
            testDaliBinary(size, COMPRESS_METHOD_NONE);
        }
    }

    void testCompression()
    {
        auto options = { COMPRESS_METHOD_LZW, COMPRESS_METHOD_FASTLZ, COMPRESS_METHOD_LZ4, COMPRESS_METHOD_LZ4HC };
        for (auto compression : options)
        {
            testSimpleBinary(0x100000, compression);
            testDaliBinary(0x100000, compression);
        }
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION( DaliBinaryDataTester );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( DaliBinaryDataTester, "DaliBinaryDataTester" );




#endif // _USE_CPPUNIT
