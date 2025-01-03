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

#ifndef __THORREAD_HPP_
#define __THORREAD_HPP_

#ifdef THORHELPER_EXPORTS
 #define THORHELPER_API DECL_EXPORT
#else
 #define THORHELPER_API DECL_IMPORT
#endif

#include "jfile.hpp"
#include "jrowstream.hpp"
#include "jptree.hpp"
#include "rtlkey.hpp"
#include "rtldynfield.hpp"

//---------------------------------------------------------------------------------
// byte stream related classes (copied from jstream.hpp)

interface ISerialInputStream : extends IInterface
{
    virtual size32_t read(size32_t len, void * ptr) = 0;            // returns size read, result < len does NOT imply end of file
    virtual void skip(size32_t sz) = 0;
    virtual void get(size32_t len, void * ptr) = 0;                 // exception if no data available
    virtual bool eos() = 0;                                         // no more data
    virtual void reset(offset_t _offset, offset_t _flen) = 0;       // input stream has changed - restart reading
    virtual offset_t tell() const = 0;                              // used to implement beginNested
};

interface IBufferedSerialInputStream : extends ISerialInputStream
{
    virtual const void * peek(size32_t wanted, size32_t &got) = 0;   // try and ensure wanted bytes are available.
                                                                    // if got<wanted then approaching eof
                                                                    // if got>wanted then got is size available in buffer
    virtual void replaceInput(ISerialInputStream * newInput) = 0;
};



interface ISerialOutputStream : extends IInterface
{
    virtual void put(size32_t len, const void * ptr) = 0;       // throws an error if cannot write the full size.
    virtual void flush() = 0;
    virtual offset_t tell() const = 0;                          // used to implement beginNested
    virtual void finished() = 0;                                // called just before the output stream is destroyed. [new function]
};

interface IBufferedSerialOutputStream : extends ISerialOutputStream
{
    virtual byte * reserve(size32_t wanted, size32_t & got) = 0;    // get a pointer to a contiguous block of memory to write to.
    virtual void commit(size32_t written) = 0 ;      // commit the data written to the block returned by reserve
    virtual void suspend(size32_t wanted) = 0;   // Reserve some bytes and prevent data being flushed to the next stage until resume() is called.  May nest.
    virtual void resume(size32_t len, const void * ptr) = 0;  // update the data allocated by suspend and allow flushing.
    virtual void replaceOutput(ISerialOutputStream * newOutput) = 0;
};


interface IFile
{
    //Two new functions added to IFile.
    //the default implementations create a wrapper around an IFileIO, but implementations can
    //create specialist implementations, and not implement open() if only streaming operations are allowed.
    virtual ISerialInputStream * createReadStream(...);
    virtual ISerialOutputStream * createWriteStream(...);
};


//This interface is used to create IFiles, and by extension input and output streams.
interface IRemoteFileCreateHook: extends IInterface
{
    virtual IFile * createIFile(const RemoteFilename & filename)=0;
    virtual IFile * createIFile(const char * provider, const char * filename)=0;
};


//--- Classes and interfaces for reading instances of files
//The following is constant for the life of a disk read activity

// IWriteFormatMapping interface represents the mapping when outputting a stream to a destination.
//
//  @expectedMeta - the format that rows have in memory (rename?)
//  @projectedMeta - the format that should be written to disk.
//  @formatOptions - which options are applied to the format
//
// if expectedMeta->querySerializedMeta() != projectedMeta then the transformation will lose
// fields from the dataset as it is written.  Reordering may be supported later, but fields
// will never be added.
interface IWriteFormatMapping : public IInterface
{
public:
    virtual unsigned getExpectedCrc() const = 0;
    virtual unsigned getProjectedCrc() const = 0;
    virtual IOutputMetaData * queryExpectedMeta() const = 0;
    virtual IOutputMetaData * queryProjectedMeta() const = 0;
    virtual RecordTranslationMode queryTranslationMode() const = 0;
    virtual IPropertyTree * queryFormatOptions() const = 0;
    virtual bool matches(const IWriteFormatMapping * other) const = 0;
};
THORHELPER_API IWriteFormatMapping * createDiskWriteMapping(RecordTranslationMode mode, unsigned expectedCrc, IOutputMetaData & expected, unsigned projectedCrc, IOutputMetaData & projected);

// IReadFormatMapping interface represents the mapping when reading a stream from an external source.
//
//  @actualMeta - the format obtained from the meta infromation (e.g. dali)
//  @expectedMeta - the format that is specified in the ECL
//  @projectedMeta - the format of the rows to be streamed out.
//  @formatOptions - what options are applied to the format (e.g. csv separator)
//
// if expectedMeta->querySerializedMeta() != projectedMeta then the transformation will lose
// fields from the dataset as it is written.

interface IReadFormatMapping : public IInterface
{
public:
    // Accessor functions to provide the basic information from the disk read
    virtual const char * queryFormat() const = 0;
    virtual unsigned getActualCrc() const = 0;
    virtual unsigned getExpectedCrc() const = 0;
    virtual unsigned getProjectedCrc() const = 0;
    virtual IOutputMetaData * queryActualMeta() const = 0;
    virtual IOutputMetaData * queryExpectedMeta() const = 0;
    virtual IOutputMetaData * queryProjectedMeta() const = 0;
    virtual const IPropertyTree * queryFormatOptions() const = 0;
    virtual RecordTranslationMode queryTranslationMode() const = 0;

    virtual bool matches(const IReadFormatMapping * other) const = 0;
    virtual bool expectedMatchesProjected() const = 0;

    virtual const IDynamicTransform * queryTranslator() const = 0; // translates from actual to projected - null if no translation needed
    virtual const IKeyTranslator *queryKeyedTranslator() const = 0; // translates from expected to actual
};

THORHELPER_API IReadFormatMapping * createDiskReadMapping(RecordTranslationMode mode, const char * format, unsigned actualCrc, IOutputMetaData & actual, unsigned expectedCrc, IOutputMetaData & expected, unsigned projectedCrc, IOutputMetaData & projected, const IPropertyTree * fileOptions);


typedef IConstArrayOf<IFieldFilter> FieldFilterArray;

interface IRowReaderSource : extends IInterface
{
    virtual IDiskRowStream * createRowStream(IEngineRowAllocator * _outputAllocator, const FieldFilterArray & expectedFilter) = 0;
};

interface IRowReader : extends IInterface
{
public:
    // get the interface for reading streams of row.  outputAllocator can be null if allocating next is not used.
    virtual bool matches(const char * format, IReadFormatMapping * mapping) = 0;  // not sure if this is needed....

    //NOTE: createSource may need to merge the inputformat options with the ecl options, and create a derived formatter based on that.
    //There are no problems with recreating the input buffering though - so the overhead is likely to be minimal. 
    virtual IRowReaderSource * createSource(IBufferedSerialInputStream * optStream, const char * logicalFilename, unsigned partNumber, offset_t baseOffset, const IPropertyTree * optInputFormatOptions) = 0;
    virtual IRowReaderSource * createSource(const RemoteFilename & filename, const char * logicalFilename, unsigned partNumber, offset_t baseOffset, const IPropertyTree * inputFormatOptions) = 0;
};

interface IRowFormatter : extends IInterface
{
    virtual bool supportsStreamInput() const = 0;
    virtual bool supportsStreamOutput() const = 0;
    virtual IRowReader * createReader(IReadFormatMapping * mapping) = 0;
    virtual IRowWriter * createWriter(IWriteFormatMapping * mapping) = 0;
};
IRowFormatter * queryRowFormatter(const char * format);

interface IUnifiedProvider
{
    virtual const char * queryName() const = 0;
    virtual ISerialInputStream * createReadStream() = 0; // returns null if formatting is always implicit
    virtual ISerialOutputStream * createWriteStream() = 0; // returns null if formatting is always implicit
    virtual IRowReaderSource * createSource(IReadFormatMapping * mapping) = 0;
    virtual IRowWriterStream * createTarget(IWriteFormatMapping * mapping) = 0;
};

IUnifiedProvider * createUnifiedProvider(const char * name, IPropertyTree * options, unsigned whichNode, unsigned curNode, bool global);


//A helper class for managing the reuse of the input buffers and decompressors to avoid having to recreate them
//when processing multiple file parts from a single activity.   Most significant for hthor/roxie, and reading
//multi-part spill files from thor.

class InputBufferAndDecompressor
{
    virtual IBufferedSerialInputStream * queryOutputStream() { return outputStream; }
    virtual void selectInput(ISerialInputStream * stream, const char * plane, bool isCompressed, unsigned compressedBufferSize??)
    {
        unsigned bufferSize = getBlockedFileIOSize(plane);
        if (isCompressed && bufferSize < 0x10000)
            bufferSize = 0x10000;

        if (readSize < bufferSize)
        {
            inputStream.setown(createBufferedIOStream(stream, bufferSize));
            readSize = bufferSize;
        }
        else
            inputStream->replaceInput(stream);

        if (isCompressed)
        {
            if (!decompressStream)
                decompressStream.setown(createDecompressor(inputStream, compressedSize));
            else
                decompressStream->replaceInput(inputStream); // MORE: this function is needed.
            
            if (decompressSize < compressedBufferSize)
            {
                decompressBufferStream.setown(createBufferedIOStream(decompressStream, compressedBufferSize));
                decompressSize = compressedBufferSize;
            }
            outputStream.set(decompressBufferStream);
        }
        else
            outputStream.set(inputStream);
    }

private:
    size32_t readSize = 0;
    size32_t decompressSize = 0;
    Owned<IBufferedSerialInputStream> inputStream;
    Owned<ISerialInputStream> decompressStream;
    Owned<IBufferedSerialInputStream> decompressBufferStream;
    Owned<IBufferedSerialInputStream> outputStream;
};


// walkthrough of various test cases:
// a)	hthor, read multi part flat file from local disk 
void readMultiPartThorFromHthor()
{
    FieldFilterArray filter;            // created from helper
    Owned<IPropertyTree> formatOptions; // created from helper

    //MORE: Can this vary between subfiles in a superfile?  If so it needs creating for each logical file, same with the format.
    IReadFormatMapping * inputMapping = createReadFormatMapping(expected, actual, projected, formatOptions); // created from meta

    IRowFormatter * format = createRowFormatter("thor");
    bool streamed = format->supportsStreamInput();
    InputBufferAndDecompressor inputBuffer;
    IRowReader * reader = format->createReader(mapping);
    for (auto input & : inputs)
    {
        IRowReaderSource * source;
        IPropertyTree * inputOptions = input->getDaliOptions();
        if (streamed)
        {
            Owned<IFile> resolvedFile = createIFile(input->queryRemoteFilename());
            Owned<ISerialInputStream> inputStream = resolvedFile->createReadStream(inputOptions);
            inputBuffer.selectInput(inputStream, input->queryStoragePlane(), input->isCompressed(), compressedBufferSize);
            source = reader->createSource(inputBuffer.queryOutputStream(), input->getLogicalFilename(), input->getPartNumber(), input->getBaseOffset(), inputOptions);
        }
        else
            source = reader->createSource(input->queryPhysicalFilename(), input->queryLogicalFilename(), input->getPartNumber(), input->getBaseOffset(), inputOptions);

        Owned<IDiskRowStream> rowStream = source->createRowStream(outputAllocator, filter);
        ...process all the records...
        rowStream.clear(); //release row stream - and return it to the source's pool, just keep a list of all in the source and skip if shared. Unlikley to get long?  Careful with multithreading.
    }
}

// b)	hthor reading single part file via dafilesrv
// processing is identical to above - createIFile() will resolve to an external IFile

// c)	thor, read distributed csv from azure api.
//
// The storage plane is configured to use an azure provider.  by default that mangles the pyhsycal filename with a prefix of azureblob:<plane>//path
// that prefix is spotted in the createIFile() function and the a AzureFile object is returned.
// NOTE: colon before the first path separator char is already treated as an absolute path.
// The function to read is similar to the code above, except that inputs has been filtered by whether the operation 

// d)	hthor, read single xml file from a zip file.
//
// The .zip extension is spotted as a container provider, and a mapped IFile is returned.

// e)	Thor, read single/distributed parquet file stored on azure.
//
// The storage plane is configured to use an azure provider.
// The format is resolved as parquet format.  Provided formatter->supportsStreamInput() is true, then the code above works.
// If parquet->supportsStreamInput() is false then it is only supported from local parquet files.

// f)	Roxie, read from a cosmos db
//
// The ECL indicates that a custom comos provider is used.  The engine understands that no filename mapping occurs.

void readFromUnifiedProvider()
{
    FieldFilterArray filter;            // created from helper
    Owned<IPropertyTree> formatOptions; // created from helper

    IUnifiedProvider * provider = createUnifiedProvider(logicalFilename, providerOptions, whichNode, curNode, global);
    Owned<IRowReaderSource> source;
    IReadFormatMapping * inputMapping = createReadFormatMapping(expected, expected, projected, formatOptions); // created from meta
    Owned<IBufferedSerialInputStream> inputStream = provider->createReadStream();
    if (inputStream)
    {
        //Format is independent of the 
        if (!format) format = provider->queryDefaultFormat();??
        IRowFormatter * format = queryRowFormatter(format);
        assertex(format->supportsStreamInput());
        IRowReader * reader = format->createReader(inputMapping);
        source.setown(reader->createSource(inputStream, logicalFilename, 1, 0, nullptr));
    }
    else
        source.setown(provider->createSource(inputMapping))

    Owned<IDiskRowStream> rowStream = source->createRowStream(outputAllocator, filter);
    ...process all the records...
    rowStream.clear(); //release row stream - and return it to the source's pool, just keep a list of all in the sources and skip if shared. Unlikley to get long?  Careful with multithreading.
}


//
// g)	Hthor, Read a csv file from a zip file stored on aws s3.
//
// A variant on (d), but with an extra complication.  The providers need to be processed in the correct order/
// in this case the zip provider should be processed first.  It will remove the zip extension, and then resolve
// the remainder of the filename as a different IFile.  Therefore the hooks need a flag to indicate if they are
// indirect, and if they are they have a higher priority.

// h)	Thor, read a distributed csv file from an unified provider.
//
// Logic in thor is the same as the other unified providers.
// The logic with the unified provider is similar to the logic added to hthor to support container providers.


// Questions:
// Is there any scope for reusing CFile objects for the custom providers?  Possibly by internally caching instances, but unlikely to be worth it.

#endif
