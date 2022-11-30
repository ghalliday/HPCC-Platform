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

#ifndef CTFILE_HPP
#define CTFILE_HPP

#include "jiface.hpp"
#include "jhutil.hpp"
#include "hlzw.h"
#include "jcrc.hpp"
#include "jio.hpp"
#include "jfile.hpp"

#define NODESIZE 8192

//The following constants are used for the keyFlags field (the bottom 8 bits are also stored in the
//oldktype field for backward compatibility.
#define TRAILING_HEADER_ONLY  0x01 // Leading header not updated - use trailing one
#define HTREE_TOPLEVEL_KEY  0x02
#define COL_PREFIX          0x04
#define HTREE_QUICK_COMPRESSED 0x08 // See QUICK_COMPRESSED_KEY below
#define HTREE_VARSIZE       0x10
#define HTREE_FULLSORT_KEY  0x20
#define HTREE_COMPRESSED_KEY 0x40
#define HTREE_QUICK_COMPRESSED_KEY 0x48
#define USE_TRAILING_HEADER  0x80 // Real index header node located at end of file
#define INPLACE_COMPRESS_BRANCH 0x100 // Compress branch nodes that can be processed as-is
#define INPLACE_COMPRESS_LEAF 0x200 // Compress leaf nodes that can be processed as-is

#define KEYBUILD_VERSION 1 // unsigned short. NB: This should upped if a change would make existing keys incompatible with current build.
#define KEYBUILD_MAXLENGTH 0x7FFF

// structure to be read into - NO VIRTUALS.
// This header layout corresponds to FairCom cTree layout for compatibility with old systems ...

struct __declspec(novtable) jhtree_decl KeyHdr
{

    __int64 phyrec; /* last byte offset of file     00x */
    __int64 delstk; /* top of delete stack: fixed len data  08x */
    __int64 numrec; /* last byte offset written     10x */
    __int64 reshdr; /* resource header          18x */
    __int64 lstmbr; /* last super file member/position  20x */
    __int64 sernum; /* serial number            28x */
    __int64 nument; /* active entries           30x */
    __int64 root;   /* B-Tree root              38x */
    __int64 fileid; /* unique file id           40x */
    __int64 servid; /* unique server id         48x */
    short   verson; /* configuration options at create  50x */
    unsigned short  nodeSize;   /* node record size         52x */
    unsigned short  reclen; /* data record length           54x */
    unsigned short  extsiz; /* extend file (chunk) size     56x */
    unsigned short  flmode; /* file mode (virtual, etc)     58x */
    unsigned short  logtyp; /* permanent components of file mode    5ax */
    unsigned short  maxkbl; /* maximum key bytes leaf-var       5cx */
    unsigned short  maxkbn; /* maximum key bytes non leaf-var   5ex */
    char    updflg; /* update (corrupt) flag        60x */
    char    oldktype;  /* file type flag           61x */
    char    autodup;/* duplicate flag           62x */
    char    deltyp; /* flag for type of idx delete      63x */
    unsigned char   keypad; /* padding byte             64x */
    unsigned char   flflvr; /* file flavor              65x */
    unsigned char   flalgn; /* file alignment           66x */
    unsigned char   flpntr; /* file pointer size            67x */
    unsigned short  clstyp; /* flag for file type           68x */
    unsigned short  length; /* key length               6ax */
    short   nmem;   /* number of members            6cx */
    short   kmem;   /* member number            6ex */
    __int64 lanchr; /* left most leaf anchor        70x */
    __int64 supid;  /* super file member #          78x */
    __int64 hdrpos; /* header position          80x */
    __int64 sihdr;  /* superfile header index hdr position  88x */
    __int64 timeid; /* time id#             90x */
    unsigned short  suptyp; /* super file type          98x */
    unsigned short  maxmrk; /* maximum exc mark entries per leaf    9ax */
    unsigned short  namlen; /* MAX_NAME at creation         9cx */
    unsigned short  xflmod; /* extended file mode info      9ex */
    __int64 defrel; /* file def release mask        a0x */
    __int64 hghtrn; /* tran# high water mark for idx    a8x */
    __int64 hdrseq; /* wrthdr sequence #            b0x */
    __int64 tstamp; /* update time stamp            b8x */
    __int64 rs3[3]; /* future use               c0x */
    __int64 fposOffset; /* amount by which file positions are biased        d8x */
    __int64 fileSize; /* fileSize - was once used in the bias calculation e0x */
    short nodeKeyLength; /* key length in intermediate level nodes e8x */
    unsigned short version; /* build version - to be updated if key format changes    eax*/
    unsigned keyFlags; /* extended version of ktype ecx */
    __int64 blobHead; /* fpos of first blob node f0x */
    __int64 metadataHead; /* fpos of first metadata node f8x */
    __int64 bloomHead; /* fpos of bloom table data, if present 100x */
    __uint64 partitionFieldMask; /* Bitmap indicating partition keyed fields 108x */
    __int64 firstLeaf; /* fpos of first leaf node 110x */
};

enum NodeType : char
{
    NodeBranch = 0,
    NodeLeaf = 1,
    NodeBlob = 2,
    NodeMeta = 3,
    NodeBloom = 4,
//The following is never stored and only used in code as a value that does not match any of the above.
    NodeNone = 127,
};


//#pragma pack(1)
#pragma pack(push,1)
struct jhtree_decl NodeHdr
{
    __int64    rightSib;
    __int64    leftSib;
    unsigned short   numKeys;
    unsigned short   keyBytes;
    unsigned    crc32;
    char    unusedMemNumber;
    char    leafFlag;

    bool isValid(unsigned nodeSize)
    {
        return 
            (rightSib % nodeSize == 0) &&
            (leftSib % nodeSize == 0) &&
            (unusedMemNumber==0) &&
            (keyBytes < nodeSize);
    }
};
//#pragma pack(4)
#pragma pack(pop)

class CWritableKeyNode : public CInterface
{
public:
    virtual void write(IFileIOStream *, CRC32 *crc) = 0;
};

class jhtree_decl CKeyHdr : public CWritableKeyNode
{
private:
    KeyHdr hdr;
public:
    CKeyHdr();

    void load(KeyHdr &_hdr);
    virtual void write(IFileIOStream *, CRC32 *crc) override;

    unsigned int getMaxKeyLength();  // MORE - is this correctly named? Is it the max record length?
    void setMaxKeyLength(uint32_t max) { hdr.length = max; };
    bool isVariable();
    inline unsigned int getNodeKeyLength() 
    {
        return hdr.nodeKeyLength != -1 ? hdr.nodeKeyLength : getMaxKeyLength(); 
    }
    inline bool hasPayload()
    {
        return (hdr.nodeKeyLength != -1);
    }
    inline unsigned getKeyType() { return hdr.keyFlags; }
    inline offset_t getRootFPos() { return hdr.root; }
    inline unsigned short getMaxNodeBytes() { return hdr.maxkbl; }
    inline KeyHdr *getHdrStruct() { return &hdr; }
    inline static size32_t getSize() { return sizeof(KeyHdr); }
    inline __int64 getNumRecords() { return hdr.nument; }
    inline unsigned getNodeSize() { return hdr.nodeSize; }
    inline offset_t getFirstLeafPos() const { return (offset_t)hdr.firstLeaf; }
    inline bool hasSpecialFileposition() const { return true; }
    inline bool isRowCompressed() const { return (hdr.keyFlags & (HTREE_QUICK_COMPRESSED_KEY|HTREE_VARSIZE)) == HTREE_QUICK_COMPRESSED_KEY; }
    inline bool isInplaceCompressedBranch() const { return (hdr.keyFlags & INPLACE_COMPRESS_BRANCH) != 0; }
    inline bool isInplaceCompressedLeaf() const { return (hdr.keyFlags & INPLACE_COMPRESS_LEAF) != 0; }
    __uint64 getPartitionFieldMask()
    {
        if (hdr.partitionFieldMask == (__uint64) -1)
            return 0;
        else
            return hdr.partitionFieldMask;
    }
    unsigned numPartitions()
    {
        if (hdr.keyFlags & HTREE_TOPLEVEL_KEY)
            return (unsigned) hdr.nument-1;
        else
            return 0;
    }
    void setPhyRec(offset_t pos)
    {
        hdr.phyrec = hdr.numrec = pos;
    }
};

class jhtree_decl CNodeBase : public CWritableKeyNode
{
protected:
    NodeHdr hdr;
    CKeyHdr *keyHdr;
    unsigned keyType;
    bool isVariable;
    std::atomic<bool> ready{false}; // is this node ready for use?  Can be checked outside a critsec, but only set within one.

private:
    offset_t fpos;

public:
    virtual void write(IFileIOStream *, CRC32 *crc) { throwUnexpected(); }
    inline offset_t getFpos() const { assertex(fpos); return fpos; }
    inline size32_t getNumKeys() const { return hdr.numKeys; }
    inline bool isBlob() const { return hdr.leafFlag == NodeBlob; }
    inline bool isMetadata() const { return hdr.leafFlag == NodeMeta; }
    inline bool isBloom() const { return hdr.leafFlag == NodeBloom; }
    inline bool isLeaf() const { return hdr.leafFlag != NodeBranch; }       // actually is-non-branch.  Use should be reviewed.
    inline bool isBranch() const { return hdr.leafFlag == NodeBranch; }
    inline NodeType getNodeType() const { return (NodeType)hdr.leafFlag; }
    const char * getNodeTypeName() const;

    inline bool isReady() const { return ready; }
    inline void noteReady() { ready = true; }
public:
    CNodeBase();
    void load(CKeyHdr *keyHdr, offset_t fpos);
    ~CNodeBase();
};

class jhtree_decl CJHTreeNode : public CNodeBase
{
protected:
    size32_t keyLen = 0;
    size32_t keyCompareLen = 0;
    size32_t keyRecLen;
    size32_t expandedSize;
    char * keyBuf;              // Used for expanded keys or blobs
    unsigned __int64 firstSequence;

    inline size32_t getKeyLen() const { return keyLen; }
    static void releaseMem(void *togo, size32_t size);
    static void *allocMem(size32_t size);

public:
    CJHTreeNode();
    ~CJHTreeNode();

    inline bool isKeyAt(unsigned int num) { return (num < hdr.numKeys); }

//These are the key functions that need to be implemented for a node that can be searched
    virtual void load(CKeyHdr *keyHdr, const void *rawData, offset_t pos, bool needCopy) = 0; // has an implementation
    virtual bool getKeyAt(unsigned int num, char *dest) const = 0;       // Retrieve keyed fields
    virtual bool fetchPayload(unsigned int num, char *dest) const = 0;   // Retrieve payload fields. Note destination is assumed to already contain keyed fields
    virtual size32_t getSizeAt(unsigned int num) const = 0;
    virtual offset_t getFPosAt(unsigned int num) const = 0;
    virtual int compareValueAt(const char *src, unsigned int index) const = 0;

    virtual int locateGE(const char * search, unsigned minIndex) const;
    virtual int locateGT(const char * search, unsigned minIndex) const;

// reading methods
    size32_t getMemSize() { return expandedSize; }
    offset_t prevNodeFpos() const;
    offset_t nextNodeFpos() const ;
    bool contains(const char *src) const;
    inline offset_t getRightSib() const { return hdr.rightSib; }
    inline offset_t getLeftSib() const { return hdr.leftSib; }
    unsigned __int64 getSequence(unsigned int num) const;
    size32_t getNodeSize() const;
    void dump(FILE *out, int length, unsigned rowCount, bool raw) const;
};


class jhtree_decl CJHExpandedTreeNode : public CJHTreeNode
{
protected:
    void unpack(const void *node, bool needCopy);

    static char *expandKeys(void *src,size32_t &retsize);

public:
    virtual void load(CKeyHdr *keyHdr, const void *rawData, offset_t pos, bool needCopy) override;
    virtual bool getKeyAt(unsigned int num, char *dest) const override;       // Retrieve keyed fields
    virtual bool fetchPayload(unsigned int num, char *dest) const override;   // Retrieve payload fields. Note destination is assumed to already contain keyed fields
    virtual size32_t getSizeAt(unsigned int num) const override;
    virtual offset_t getFPosAt(unsigned int num) const override;
    virtual int compareValueAt(const char *src, unsigned int index) const override;
};


class CJHVarTreeNode : public CJHExpandedTreeNode
{
    const char **recArray;

public:
    CJHVarTreeNode();
    ~CJHVarTreeNode();
    virtual void load(CKeyHdr *keyHdr, const void *rawData, offset_t pos, bool needCopy);
    virtual bool getKeyAt(unsigned int num, char *dest) const;         // Retrieve keyed fields
    virtual bool fetchPayload(unsigned int num, char *dest) const;       // Retrieve payload fields. Note destination is assumed to already contain keyed fields
    virtual size32_t getSizeAt(unsigned int num) const;
    virtual offset_t getFPosAt(unsigned int num) const;
    virtual int compareValueAt(const char *src, unsigned int index) const;
};

class CJHRowCompressedNode : public CJHExpandedTreeNode
{
    Owned<IRandRowExpander> rowexp;  // expander for rand rowdiff
    static IRandRowExpander *expandQuickKeys(void *src, bool needCopy);
public:
    virtual void load(CKeyHdr *keyHdr, const void *rawData, offset_t pos, bool needCopy);
    virtual bool getKeyAt(unsigned int num, char *dest) const;         // Retrieve keyed fields
    virtual bool fetchPayload(unsigned int num, char *dest) const;       // Retrieve payload fields. Note destination is assumed to already contain keyed fields
    virtual offset_t getFPosAt(unsigned int num) const;
    virtual int compareValueAt(const char *src, unsigned int index) const;
};

class CJHTreeBlobNode : public CJHExpandedTreeNode
{
public:
    CJHTreeBlobNode ();
    ~CJHTreeBlobNode ();
    virtual offset_t getFPosAt(unsigned int num) const {throwUnexpected();}
    virtual size32_t getSizeAt(unsigned int num) const {throwUnexpected();}
    virtual int compareValueAt(const char *src, unsigned int index) const {throwUnexpected();}
    virtual void dump() {throwUnexpected();}

    size32_t getTotalBlobSize(unsigned offset);
    size32_t getBlobData(unsigned offset, void *dst);
};

class CJHTreeMetadataNode : public CJHExpandedTreeNode
{
public:
    virtual offset_t getFPosAt(unsigned int num) const {throwUnexpected();}
    virtual size32_t getSizeAt(unsigned int num) const {throwUnexpected();}
    virtual int compareValueAt(const char *src, unsigned int index) const {throwUnexpected();}
    virtual void dump() {throwUnexpected();}
    void get(StringBuffer & out);
};

class CJHTreeBloomTableNode : public CJHExpandedTreeNode
{
public:
    virtual offset_t getFPosAt(unsigned int num) const {throwUnexpected();}
    virtual size32_t getSizeAt(unsigned int num) const {throwUnexpected();}
    virtual int compareValueAt(const char *src, unsigned int index) const {throwUnexpected();}
    virtual void dump() {throwUnexpected();}
    void get(MemoryBuffer & out);
    __int64 get8();
    unsigned get4();
private:
    unsigned read = 0;
};

class jhtree_decl CWriteNodeBase : public CNodeBase
{
protected:
    char *nodeBuf;
    char *keyPtr;
    int maxBytes;
    KeyCompressor lzwcomp;
    void writeHdr();

public:
    CWriteNodeBase(offset_t fpos, CKeyHdr *keyHdr);
    ~CWriteNodeBase();

    virtual void write(IFileIOStream *, CRC32 *crc) override;
    void setLeftSib(offset_t leftSib) { hdr.leftSib = leftSib; }
    void setRightSib(offset_t rightSib) { hdr.rightSib = rightSib; }
};


class jhtree_decl CWriteNode : public CWriteNodeBase
{
protected:
    unsigned keyLen = 0;
    unsigned keyCompareLen = 0;
    char *lastKeyValue;
    unsigned __int64 lastSequence;

public:
    CWriteNode(offset_t fpos, CKeyHdr *keyHdr, bool isLeaf);
    ~CWriteNode();

    virtual bool add(offset_t pos, const void *data, size32_t size, unsigned __int64 sequence);

    const void *getLastKeyValue() const { return lastKeyValue; }
    unsigned __int64 getLastSequence() const { return lastSequence; }

protected:
    size32_t compressValue(const char *keyData, size32_t size, char *result);
    void saveLastKey(const void *indata, size32_t insize, unsigned __int64 sequence);
};

class jhtree_decl CLeafWriteNode : public CWriteNode
{
public:
    CLeafWriteNode(offset_t fpos, CKeyHdr *keyHdr);

    virtual void write(IFileIOStream *, CRC32 *crc) override;
};

class jhtree_decl CBranchWriteNode : public CWriteNode
{
public:
    CBranchWriteNode(offset_t fpos, CKeyHdr *keyHdr);
};

class jhtree_decl CBlobWriteNode : public CWriteNodeBase
{
    static unsigned __int64 makeBlobId(offset_t nodepos, unsigned offset);
public:
    CBlobWriteNode(offset_t _fpos, CKeyHdr *keyHdr);
    ~CBlobWriteNode();

    virtual void write(IFileIOStream *, CRC32 *crc) override;
    unsigned __int64 add(const char * &data, size32_t &size);
};

class jhtree_decl CMetadataWriteNode : public CWriteNodeBase
{
public:
    CMetadataWriteNode(offset_t _fpos, CKeyHdr *keyHdr);
    size32_t set(const char * &data, size32_t &size);
};

class jhtree_decl CBloomFilterWriteNode : public CWriteNodeBase
{
public:
    CBloomFilterWriteNode(offset_t _fpos, CKeyHdr *keyHdr);
    size32_t set(const byte * &data, size32_t &size);
    void put4(unsigned val);
    void put8(__int64 val);
};

enum KeyExceptionCodes
{
    KeyExcpt_IncompatVersion = 1,
};
interface jhtree_decl IKeyException : extends IException { };
IKeyException *MakeKeyException(int code, const char *format, ...) __attribute__((format(printf, 2, 3)));


//Following class can be passed to nodes when they are being created to help save context information
class KeyBuildContext
{
public:
    unsigned numKeyedDuplicates = 0;
    unsigned singleCounts[256] = { 0 };
    MemoryBuffer uncompressed;
    MemoryAttr compressed;
    const byte * nullRow = nullptr;
};

#endif
