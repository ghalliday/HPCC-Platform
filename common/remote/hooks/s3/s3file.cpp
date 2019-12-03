/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2019 HPCC Systems®.

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

#include "platform.h"

#include "jlib.hpp"
#include "jio.hpp"

#include "jmutex.hpp"
#include "jfile.hpp"
#include "jregexp.hpp"
#include "jstring.hpp"
#include "jlog.hpp"

#include "s3file.hpp"

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>
#include <aws/s3/model/DeleteObjectRequest.h>

/*
 * S3 questions:
 *
 * Where would we store access id/secrets?
 * What is the latency on file access?  ~200ms.
 * What is the cost of accessing the data?  $0.4/million GET requests (including requesting meta)
 * How to you efficiently page results from a S3 bucket?  You can perform a range get, but you'll need to pay for each call.
 * You can get the length of an object using HeadObject..getContentLength, but you will get charged - so for small files it is better to just get it.
 *     Probably best to request the first 10Mb, and if that returns 10Mb then request the size information.
 * S3 does not support partial writes, so we would need to create a file locally and then write it in one go.  Not sure how you do that if it doesn't fit into memory...
 *
 */

#define TRACE_S3
#define TEST_S3_PAGING

constexpr const char * s3FilePrefix = "s3://";
#ifdef TEST_S3_PAGING
constexpr offset_t awsReadRequestSize = 50;
#else
constexpr offset_t awsReadRequestSize = 0x400000;  // Default to requesting 4Mb each time
#endif

constexpr const char * myAccessKeyId = "<myId>";
constexpr const char * myAccessKeySecret = "<mySecret>";

static Aws::SDKOptions options;
MODULE_INIT(INIT_PRIORITY_HQLINTERNAL)
{
    Aws::InitAPI(options);
    return true;
}
MODULE_EXIT()
{
    Aws::ShutdownAPI(options);
}

//---------------------------------------------------------------------------------------------------------------------

class FileIOStats
{
public:
    ~FileIOStats()
    {
//        printf("Reads: %u  Bytes: %u  TimeMs: %u\n", (unsigned)ioReads, (unsigned)ioReadBytes, (unsigned)cycle_to_millisec(ioReadCycles));
    }
    unsigned __int64 getStatistic(StatisticKind kind);

public:
    RelaxedAtomic<cycle_t> ioReadCycles{0};
    RelaxedAtomic<cycle_t> ioWriteCycles{0};
    RelaxedAtomic<__uint64> ioReadBytes{0};
    RelaxedAtomic<__uint64> ioWriteBytes{0};
    RelaxedAtomic<__uint64> ioReads{0};
    RelaxedAtomic<__uint64> ioWrites{0};
};

class S3File;
class S3FileIO : implements CInterfaceOf<IFileIO>
{
public:
    S3FileIO(S3File * _file, Aws::S3::Model::GetObjectOutcome & firstRead, FileIOStats & _stats);

    virtual size32_t read(offset_t pos, size32_t len, void * data) override;
    virtual offset_t size() override;
    virtual void close() override
    {
    }

    // Write methods not implemented - this is a read-only file
    virtual size32_t write(offset_t pos, size32_t len, const void * data) override
    {
        throwUnexpected();
        return 0;
    }
    virtual offset_t appendFile(IFile *file,offset_t pos=0,offset_t len=(offset_t)-1) override
    {
        throwUnexpected();
        return 0;
    }
    virtual void setSize(offset_t size) override
    {
        throwUnexpected();
    }
    virtual void flush() override
    {
        //Could implement if we use the async version of the putObject call.
    }
    unsigned __int64 getStatistic(StatisticKind kind) override;

protected:
    size_t extractDataFromResult(size_t offset, size_t length, void * target);

protected:
    Linked<S3File> file;
    CriticalSection cs;
    offset_t startResultOffset = 0;
    offset_t endResultOffset = 0;
    Aws::S3::Model::GetObjectOutcome readResult;
    FileIOStats stats;
};

class S3File : implements CInterfaceOf<IFile>
{
    friend class S3FileIO;
public:
    S3File(const char *_s3FileName);
    virtual bool exists()
    {
        ensureMetaData();
        return fileExists;
    }
    virtual bool getTime(CDateTime * createTime, CDateTime * modifiedTime, CDateTime * accessedTime);
    virtual fileBool isDirectory()
    {
        ensureMetaData();
        if (!fileExists)
            return notFound;
        return isDir ? foundYes : foundNo;
    }
    virtual fileBool isFile()
    {
        ensureMetaData();
        if (!fileExists)
            return notFound;
        return !isDir ? foundYes : foundNo;
    }
    virtual fileBool isReadOnly()
    {
        ensureMetaData();
        if (!fileExists)
            return notFound;
        return foundYes;
    }
    virtual IFileIO * open(IFOmode mode, IFEflags extraFlags=IFEnone)
    {
        assertex(mode==IFOread && fileExists);
        return createFileIO();
    }
    virtual IFileAsyncIO * openAsync(IFOmode mode)
    {
        UNIMPLEMENTED;
    }
    virtual IFileIO * openShared(IFOmode mode, IFSHmode shmode, IFEflags extraFlags=IFEnone)
    {
        assertex(mode==IFOread && fileExists);
        return createFileIO();
    }
    virtual const char * queryFilename()
    {
        return fullName.str();
    }
    virtual offset_t size()
    {
        ensureMetaData();
        return fileSize;
    }

// Directory functions
    virtual IDirectoryIterator *directoryFiles(const char *mask, bool sub, bool includeDirs)
    {
        UNIMPLEMENTED;
        return createNullDirectoryIterator();
    }
    virtual bool getInfo(bool &isdir,offset_t &size,CDateTime &modtime)
    {
        ensureMetaData();
        isdir = isDir;
        size = fileSize;
        modtime.clear();
        return true;
    }

    // Not going to be implemented - this IFile interface is too big..
    virtual bool setTime(const CDateTime * createTime, const CDateTime * modifiedTime, const CDateTime * accessedTime) { UNIMPLEMENTED; }
    virtual bool remove();
    virtual void rename(const char *newTail) { UNIMPLEMENTED; }
    virtual void move(const char *newName) { UNIMPLEMENTED; }
    virtual void setReadOnly(bool ro) { UNIMPLEMENTED; }
    virtual void setFilePermissions(unsigned fPerms) { UNIMPLEMENTED; }
    virtual bool setCompression(bool set) { UNIMPLEMENTED; }
    virtual offset_t compressedSize() { UNIMPLEMENTED; }
    virtual unsigned getCRC() { UNIMPLEMENTED; }
    virtual void setCreateFlags(unsigned short cflags) { UNIMPLEMENTED; }
    virtual void setShareMode(IFSHmode shmode) { UNIMPLEMENTED; }
    virtual bool createDirectory() { UNIMPLEMENTED; }
    virtual IDirectoryDifferenceIterator *monitorDirectory(
                                  IDirectoryIterator *prev=NULL,    // in (NULL means use current as baseline)
                                  const char *mask=NULL,
                                  bool sub=false,
                                  bool includedirs=false,
                                  unsigned checkinterval=60*1000,
                                  unsigned timeout=(unsigned)-1,
                                  Semaphore *abortsem=NULL)  { UNIMPLEMENTED; }
    virtual void copySection(const RemoteFilename &dest, offset_t toOfs=(offset_t)-1, offset_t fromOfs=0, offset_t size=(offset_t)-1, ICopyFileProgress *progress=NULL, CFflags copyFlags=CFnone) { UNIMPLEMENTED; }
    virtual void copyTo(IFile *dest, size32_t buffersize=DEFAULT_COPY_BLKSIZE, ICopyFileProgress *progress=NULL, bool usetmp=false, CFflags copyFlags=CFnone) { UNIMPLEMENTED; }
    virtual IMemoryMappedFile *openMemoryMapped(offset_t ofs=0, memsize_t len=(memsize_t)-1, bool write=false)  { UNIMPLEMENTED; }

protected:
    void readBlob(Aws::S3::Model::GetObjectOutcome & readResult, FileIOStats & stats, offset_t from = 0, offset_t length = unknownFileSize);
    void ensureMetaData();
    void gatherMetaData();
    IFileIO * createFileIO();

protected:
    StringBuffer fullName;
    StringBuffer bucketName;
    StringBuffer objectName;
    offset_t fileSize = unknownFileSize;
    bool haveMeta = false;
    bool isDir = false;
    bool fileExists = false;
    int64_t modifiedMsTime = 0;
    SpinLock lock;
};

//---------------------------------------------------------------------------------------------------------------------

S3FileIO::S3FileIO(S3File * _file, Aws::S3::Model::GetObjectOutcome & firstRead, FileIOStats & _firstStats)
: file(_file), readResult(std::move(firstRead)), stats(_firstStats)
{
    startResultOffset = 0;
    endResultOffset = readResult.GetResult().GetContentLength();
}

size32_t S3FileIO::read(offset_t pos, size32_t len, void * data)
{
    if (pos > file->fileSize)
        return 0;
    if (pos + len > file->fileSize)
        len = file->fileSize - pos;
    if (len == 0)
        return 0;

    size32_t sizeRead = 0;
    offset_t lastOffset = pos + len;

    // MORE: Do we ever read file IO from more than one thread?  I'm not convinced we do, and the critical blocks waste space and slow it down.
    //It might be worth revisiting (although I'm not sure what effect stranding has)
    CriticalBlock block(cs);
    for(;;)
    {
        //Check if part of the request can be fulfilled from the current read block
        if (pos >= startResultOffset && pos < endResultOffset)
        {
            size_t copySize = ((lastOffset > endResultOffset) ? endResultOffset : lastOffset) - pos;
            size_t extractedSize = extractDataFromResult((pos - startResultOffset), copySize, data);
            assertex(copySize == extractedSize);
            pos += copySize;
            len -= copySize;
            data = (byte *)data + copySize;
            sizeRead += copySize;
            if (len == 0)
                return sizeRead;
        }

#ifdef TEST_S3_PAGING
        offset_t readSize = awsReadRequestSize;
#else
        offset_t readSize = (len > awsReadRequestSize) ? len : awsReadRequestSize;
#endif

        file->readBlob(readResult, stats, pos, readSize);
        if (!readResult.IsSuccess())
            return sizeRead;
        offset_t contentSize = readResult.GetResult().GetContentLength();
        //If the results are inconsistent then do not loop forever
        if (contentSize == 0)
            return sizeRead;

        startResultOffset = pos;
        endResultOffset = pos + contentSize;
    }
}

offset_t S3FileIO::size()
{
    return file->fileSize;
}

size_t S3FileIO::extractDataFromResult(size_t offset, size_t length, void * target)
{
    auto & contents = readResult.GetResultWithOwnership().GetBody();
    auto buffer = contents.rdbuf();
    buffer->pubseekoff(0, std::ios_base::beg, std::ios_base::in);
    return buffer->sgetn((char *)target, length);
}

unsigned __int64 S3FileIO::getStatistic(StatisticKind kind)
{
    return stats.getStatistic(kind);
}

unsigned __int64 FileIOStats::getStatistic(StatisticKind kind)
{
    switch (kind)
    {
    case StCycleDiskReadIOCycles:
        return ioReadCycles.load();
    case StCycleDiskWriteIOCycles:
        return ioWriteCycles.load();
    case StTimeDiskReadIO:
        return cycle_to_nanosec(ioReadCycles.load());
    case StTimeDiskWriteIO:
        return cycle_to_nanosec(ioWriteCycles.load());
    case StSizeDiskRead:
        return ioReadBytes.load();
    case StSizeDiskWrite:
        return ioWriteBytes.load();
    case StNumDiskReads:
        return ioReads.load();
    case StNumDiskWrites:
        return ioWrites.load();
    }
    return 0;
}

//---------------------------------------------------------------------------------------------------------------------

S3File::S3File(const char *_s3FileName) : fullName(_s3FileName)
{
    const char * filename = fullName + strlen(s3FilePrefix);
    const char * slash = strchr(filename, '/');
    assertex(slash);

    bucketName.append(slash-filename, filename);
    objectName.set(slash+1);
}

bool S3File::getTime(CDateTime * createTime, CDateTime * modifiedTime, CDateTime * accessedTime)
{
    ensureMetaData();
    if (createTime)
        createTime->clear();
    if (modifiedTime)
    {
        modifiedTime->clear();
        modifiedTime->set((time_t)(modifiedMsTime / 1000));
    }
    if (accessedTime)
        accessedTime->clear();
    return false;
}

void S3File::readBlob(Aws::S3::Model::GetObjectOutcome & readResult, FileIOStats & stats, offset_t from, offset_t length)
{
    // Set up the request
    Aws::Client::ClientConfiguration configuration;
    configuration.region = "eu-west-2";
    auto credentials = std::make_shared<Aws::Auth::SimpleAWSCredentialsProvider>(Aws::String(myAccessKeyId), Aws::String(myAccessKeySecret));
    Aws::S3::S3Client s3_client(credentials, configuration);

    Aws::S3::Model::GetObjectRequest object_request;
    object_request.SetBucket(bucketName);
    object_request.SetKey(objectName);
    if ((from != 0) || (length != unknownFileSize))
    {
        StringBuffer range;
        range.append("bytes=").append(from).append("-");
        if (length != unknownFileSize)
            range.append(from + length - 1);
        object_request.SetRange(Aws::String(range));
    }

    // Get the object
    CCycleTimer timer;
    readResult = s3_client.GetObject(object_request);
    stats.ioReads++;
    stats.ioReadCycles += timer.elapsedCycles();
    stats.ioReadBytes += readResult.GetResult().GetContentLength();

#ifdef TRACE_S3
    if (!readResult.IsSuccess())
    {
        auto error = readResult.GetError();
        DBGLOG("ERROR: %s: %s", error.GetExceptionName().c_str(), error.GetMessage().c_str());
    }
#endif
}

IFileIO * S3File::createFileIO()
{
    //Read the first chunk of the file.  If it is the full file then fill in the meta information, otherwise
    //ensure the meta information is calculated before creating the file IO object
    Aws::S3::Model::GetObjectOutcome readResult;
    FileIOStats readStats;

    SpinBlock block(lock);
    readBlob(readResult, readStats, 0, awsReadRequestSize);
    if (!readResult.IsSuccess())
        return nullptr;

    if (!haveMeta)
    {
        offset_t readSize = readResult.GetResult().GetContentLength();

        //If we read the entire file then we don't need to gather the meta to discover the file size.
        if (readSize < awsReadRequestSize)
        {
            haveMeta = true;
            fileExists = true;
            fileSize = readResult.GetResult().GetContentLength();
            modifiedMsTime = readResult.GetResult().GetLastModified().Millis();
        }
        else
        {
            gatherMetaData();
            if (!fileExists)
            {
                DBGLOG("Internal consistency - read succeeded but head failed.");
                return nullptr;
            }
        }
    }

    return new S3FileIO(this, readResult, readStats);
}

void S3File::ensureMetaData()
{
    SpinBlock block(lock);
    if (haveMeta)
        return;

    gatherMetaData();
}

void S3File::gatherMetaData()
{
    // Set up the request
    Aws::Client::ClientConfiguration configuration;
    configuration.region = "eu-west-2";
    auto credentials = std::make_shared<Aws::Auth::SimpleAWSCredentialsProvider>(Aws::String(myAccessKeyId), Aws::String(myAccessKeySecret));
    Aws::S3::S3Client s3_client(credentials, configuration);

    Aws::S3::Model::HeadObjectRequest request;
    request.SetBucket(bucketName);
    request.SetKey(objectName);

    // Get the object
    Aws::S3::Model::HeadObjectOutcome headResult = s3_client.HeadObject(request);
    if (headResult.IsSuccess())
    {
        fileExists = true;
        fileSize = headResult.GetResult().GetContentLength();
        modifiedMsTime = headResult.GetResult().GetLastModified().Millis();
    }
    else
    {
#ifdef TRACE_S3
        auto error = headResult.GetError();
        DBGLOG("ERROR: %s: %s", error.GetExceptionName().c_str(), error.GetMessage().c_str());
#endif
    }
    haveMeta = true;
}

bool S3File::remove()
{
    // Set up the request
    Aws::Client::ClientConfiguration configuration;
    configuration.region = "eu-west-2";
    auto credentials = std::make_shared<Aws::Auth::SimpleAWSCredentialsProvider>(Aws::String(myAccessKeyId), Aws::String(myAccessKeySecret));
    Aws::S3::S3Client s3_client(credentials, configuration);

    Aws::S3::Model::DeleteObjectRequest object_request;
    object_request.SetBucket(bucketName);
    object_request.SetKey(objectName);

    // Get the object
    Aws::S3::Model::DeleteObjectOutcome result = s3_client.DeleteObject(object_request);
    if (result.IsSuccess())
    {
        SpinBlock block(lock);
        haveMeta = true;
        fileExists = false;
        fileSize = unknownFileSize;
        return true;
    }
    else
    {
#ifdef TRACE_S3
        auto error = result.GetError();
        DBGLOG("ERROR: S3 Delete Object %s: %s", error.GetExceptionName().c_str(), error.GetMessage().c_str());
#endif
        return false;
    }
}

//---------------------------------------------------------------------------------------------------------------------

static IFile *createS3File(const char *s3FileName)
{
    return new S3File(s3FileName);
}


//---------------------------------------------------------------------------------------------------------------------

class S3FileHook : public CInterfaceOf<IContainedFileHook>
{
public:
    virtual IFile * createIFile(const char *fileName)
    {
        if (isS3FileName(fileName))
            return createS3File(fileName);
        else
            return NULL;
    }

protected:
    static bool isS3FileName(const char *fileName)
    {
        if (!startsWith(fileName, s3FilePrefix))
            return false;
        const char * filename = fileName + strlen(s3FilePrefix);
        const char * slash = strchr(filename, '/');
        if (!slash)
            return false;
        return true;
    }
} *s3FileHook;

static CriticalSection *cs;

extern S3FILE_API void installFileHook()
{
    CriticalBlock b(*cs); // Probably overkill!
    if (!s3FileHook)
    {
        s3FileHook = new S3FileHook;
        addContainedFileHook(s3FileHook);
    }
}

extern S3FILE_API void removeFileHook()
{
    if (cs)
    {
        CriticalBlock b(*cs); // Probably overkill!
        if (s3FileHook)
        {
            removeContainedFileHook(s3FileHook);
            delete s3FileHook;
            s3FileHook = NULL;
        }
    }
}

MODULE_INIT(INIT_PRIORITY_STANDARD)
{
    cs = new CriticalSection;
    s3FileHook = NULL;  // Not really needed, but you have to have a modinit to match a modexit
    return true;
}

MODULE_EXIT()
{
    if (s3FileHook)
    {
        removeContainedFileHook(s3FileHook);
        s3FileHook = NULL;
    }
    ::Release(s3FileHook);
    delete cs;
    cs = NULL;
}
