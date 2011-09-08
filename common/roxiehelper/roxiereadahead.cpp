/*##############################################################################

    Copyright (C) 2011 HPCC Systems.

    All rights reserved. This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
############################################################################## */

#include "jexcept.hpp"
#include "roxiehelper.hpp"
#include "roxielmj.hpp"

#include "jmisc.hpp"
#include "jfile.hpp"
#include "mpbase.hpp"
#include "dafdesc.hpp"
#include "dadfs.hpp"

#include "roxiereadahead.ipp"

//This enumeration combines eog and eof handling into a single byte
enum { RAstart, RAstarted, RArow, RAeog, RAeof };
typedef byte ReadAheadState;

class ReadAheadBuffer
{
public:
    ReadAheadBuffer()
    {
        cached = NULL;
        readahead = 0;
        available = 0;
        nextToRead = 0;
    }

    bool fill(IInputBase * input, ReadAheadState & inputState)
    {
        //Use a local variable to reduce memory access
        ReadAheadState state = inputState;
        if (state == RAeof)
            return false;

        unsigned count;
        try
        {
            for (count=0; count < readahead; )
            {
                const void * next = input->nextInGroup();
                //Always include null entries in the buffer, so make it simpler to process end of file.
                cached[count++] = next;
                if (!next)
                {
                    if (state == RAeog)
                    {
                        state = RAeof;
                        break;
                    }
                    else
                        state = RAeog;
                }
                else
                    state = RArow;
            }
        }
        catch (IException * e)
        {
            //Saved the exception away in the buffer (and add a null row), read to be thrown when that row is requested.
            assertex(count != readahead);
            cached[count++] = NULL;
            savedException.setown(e);
            state = RAeof;
        }
        inputState = state;
        nextToRead = 0;
        available = count;
        assertex(count != 0);
        return (count != 0);
    }

    void onCreate(unsigned _readahead)
    {
        cached = new const void * [readahead];
        available = 0;
        nextToRead = 0;
    }

    void start()
    {
        available = 0;
        nextToRead = 0;
        savedException.clear();
    }

    void reset()
    {
        for (;(nextToRead != available); nextToRead++)
            ReleaseRoxieRow(cached[nextToRead]);
    }

    void onDestroy()
    {
        available = 0;
        nextToRead = 0;
        readahead = 0;
        delete [] cached;
        cached = NULL;
    }

    inline bool hasRowAvailable() const { return nextToRead < available; }

    inline const void * next()
    {
        const void * row = cached[nextToRead++];
        if (row || !savedException || nextToRead != available)
            return row;
        //MORE: Should this only throw this once.  I.e. throw savedException.getClear():
        nextToRead--;
        throw LINK(savedException);
    }

protected:
    Owned<IException> savedException;
    const void * * cached;
    unsigned readahead;
    unsigned available;
    unsigned nextToRead;
};


// Sequential blocked read ahead class
// - supports max readahead.
// - supports grouping.
// Could combine eof and eog into a single state, but it is then hard to implement abort safely
class SequentialReadahead : public CInterface, implements IInputBase
{
public:
    SequentialReadahead() : input(NULL)
    {
        inputState = RAstart;
        forceAbort = true;
    }

    void onCreate(IInputBase * _input, unsigned _readahead)
    {
        input = _input;
        buffer.onCreate(_readahead);
    }

    void start()
    {
        buffer.start();
        inputState = RAstart;
        forceAbort = false;
    }

    void reset()
    {
        buffer.reset();
    }

    void onDestroy()
    {
        buffer.onDestroy();
    }

    void abort()
    {
        forceAbort = true;
    }

    //interface IInputBase
    virtual const void * nextInGroup()
    {
        if (forceAbort)
            return NULL;
        if (!buffer.hasRowAvailable())
        {
            if (!buffer.fill(input, inputState) || forceAbort)
                return NULL;
        }
        return buffer.next();
    }

    virtual IOutputMetaData * queryOutputMeta() const { return input->queryOutputMeta(); }


protected:
//    Linked<IInputBase> input;
    IInputBase * input;
    ReadAheadBuffer buffer;
    ReadAheadState inputState;
    bool forceAbort;
};


// Parallel blocked read ahead class
// - supports max readeahead.
// - supports block size for signaling, and restarting the reading thead.
// - supports grouping.
// Read and write to separate blocks of records to avoid needing to lock on each access.
// I assume there is one reader and one writer?

class ParallelReadahead : public CInterface, implements IInputBase, implements IThreaded
{
public:
    ParallelReadahead() : input(NULL)
    {
        inputState = RAstart;
        readState = RAeof;
        forceAbort = true;
        buffers = NULL;
        nextReadBlock = 0;
        nextWriteBlock = 0;
        numBlocks = 0;
        blockSize = 0;
    }

    void onCreate(IInputBase * _input, unsigned _readahead, unsigned _numBlocks)
    {
        input = _input;
        setReadAhead(_readahead, _numBlocks);
        buffers = new ReadAheadBuffer[numBlocks];
        for (unsigned i=0; i < numBlocks; i++)
            buffers[i].onCreate(blockSize);
    }

    void start()
    {
        inputState = RAstart;
        readState = RAstart;
        forceAbort = false;
        nextReadBlock = 0;
        nextWriteBlock = 0;
        writeAvailable.reinit(numBlocks);
        readAvailable.reinit(0);
        for (unsigned i = 0; i < numBlocks; i++)
            buffers[i].start();
        readAheadThread.setown(new CThreaded("RoxieReadAheadThread", this));
        readAheadThread->start();
    }

    void reset()
    {
        readAheadThread->join();
        for (unsigned i = 0; i < numBlocks; i++)
            buffers[i].reset();
    }

    void onDestroy()
    {
        for (unsigned i = 0; i < numBlocks; i++)
            buffers[i].onDestroy();
        delete [] buffers;
        buffers = NULL;
        numBlocks = 0;
    }

    void abort()
    {
        forceAbort = true;
    }

    // interface IThreaded...
    virtual void main()  // called from readThread
    {
        while (!forceAbort || inputState != RAeof)
        {
            writeAvailable.wait();
            if (!forceAbort && buffers[nextWriteBlock].fill(input, inputState) && !forceAbort)
            {
                nextWriteBlock = nextBlock(nextWriteBlock);
                readAvailable.signal();
            }
        }
        if (forceAbort)
            readAvailable.signal();
    }

    // interface IInputBase
    virtual const void * nextInGroup()
    {
        if (!forceAbort && (readState == RAstart))
        {
            readAvailable.wait();
            readState = RAstarted;
        }

        loop
        {
            if (forceAbort || (readState == RAeof))
                return NULL;

            ReadAheadBuffer & activeBuffer = buffers[nextReadBlock];
            if (activeBuffer.hasRowAvailable())
            {
                const void * next = activeBuffer.next();
                if (!next)
                {
                    if (readState == RAeog)
                        readState = RAeof;
                    else
                        readState = RAeog;
                }
                else
                    readState = RArow;
                return next;
            }

            nextReadBlock = nextBlock(nextReadBlock);
            writeAvailable.signal();
            readAvailable.wait();
        }
    }

    virtual IOutputMetaData * queryOutputMeta() const { return input->queryOutputMeta(); }


private:
    //This should work with (1,1) - useful test case, but not recommended.
    void setReadAhead(unsigned readahead, unsigned _numBlocks)
    {
        numBlocks = _numBlocks ? _numBlocks : 2;
        blockSize = readahead / _numBlocks;
        assertex(blockSize != 0);
    }

    inline unsigned nextBlock(unsigned block) const
    {
        return (block == numBlocks-1) ? 0 : block+1;
    }

protected:
    IInputBase * input;
//    Linked<IInputBase> input;
    Owned<Thread> readAheadThread;
    ReadAheadBuffer * buffers;
    Semaphore writeAvailable;
    Semaphore readAvailable;
    unsigned nextReadBlock;         // only accessed from read code
    unsigned nextWriteBlock;        // only accessed from write code
    unsigned numBlocks;
    unsigned blockSize;
    ReadAheadState inputState;      // only accessed from write code
    ReadAheadState readState;       // only accessed from read code
    bool forceAbort;
};




//Allows an arbitrary activity to be execution in parallel on a dataset
//this class takes care of ensuring the stream of output records is in
//the correct order corresponding to the expected input records.
//End of group records are not returned to the processing nodes, but groups are preserved in the output stream.
//Each input record can generate a sequence of output records.

//Possible approaches:
//Each pull from the input adds a pending record.  When it is complete that record is marked.
//   Either needs dynamic records to hold the information, or the potential to block if large numbers skipped.
//
//

class ParallelExecutor : public CInterface, implements IInputBase
{
    //This class is used to keep track of the results that are returned from a single input record.
    //Avoid signalling results for a new input are available until they can be consumed.
    class InputTrack
    {
    public:
        InputTrack() { readTrackAvailableSemaphore = NULL; }

        void start()
        {
            exception.clear();
            resultAvailable.reinit(0);
            state = RAstart;
            notifiedTrackAvailable = false;
        }
        void onCreate(Semaphore & _readTrackAvailableSemaphore)
        {
            readTrackAvailableSemaphore = &_readTrackAvailableSemaphore;
        }
        inline void beginProcessing()
        {
            state = RAstart;
            notifiedTrackAvailable = false;
        }
        void noteEof()
        {
            state = RAeof;
            signalResult();
        }
        void noteEog()
        {
            state = RAeog;
            signalResult();
        }
        void noteComplete()
        {
            if (!isEof() && !isEog())
                enqueueRow(NULL);
        }
        void enqueueRow(const void * row)
        {
            {
                CriticalBlock block(cs);
                rows.enqueue(row);
            }
            signalResult();
        }
        const void * dequeueRow()
        {
            CriticalBlock block(cs);
            if (rows.ordinality()==0 && exception)
                throw LINK(exception);
            return rows.dequeue();
        }
        inline void waitForResult()
        {
            resultAvailable.wait();
        }
        inline void setException(IException * e)
        {
            exception.set(e);
            signalResult();
        }
        inline void signalResult()
        {
            resultAvailable.signal();

            //Is this complication worth it??
            //Signal this semaphore after the result semaphore to reduce blocking.
            //Previous tracks may not have signalled yet, but if so the reader will block on
            //the result available semaphore.
            {
                CriticalBlock block(cs);
                if (notifiedTrackAvailable)
                    return;
                notifiedTrackAvailable = true;
            }

            readTrackAvailableSemaphore->signal();
        }

        inline bool isEof() const { return state == RAeof; }
        inline bool isEog() const { return state == RAeog; }

    private:
        Owned<IException> exception;
        Semaphore resultAvailable;
        Semaphore * readTrackAvailableSemaphore;
        CriticalSection cs;
        QueueOf<const void, true> rows;
        ReadAheadState state;
        bool notifiedTrackAvailable;
    };

    //MORE: Is it a problem restarting thread objects?
    //Should it create CThreaded objects in start() instead?
    class ParallelReader : implements IInputBase, implements IThreaded
    {
    public:
        ParallelReader(ParallelExecutor * _owner) : owner(_owner) {}

    //interface IThreaded
        virtual void main()
        {
            track = NULL;
            try
            {
                loop
                {
                    const void * next = processor->nextInGroup();
                    if (!next)
                        return;
                    assertex(track);        // if null implies a record was returned before reading input
                    track->enqueueRow(next);
                }
            }
            catch (IException * e)
            {
                if (track)
                    track->setException(e);
                else
                    owner->setException(e);
                e->Release();
            }
        }

    //interface IInputBase
        //When acting as an input that is passed to the processing activity
        virtual const void * nextInGroup()
        {
            return owner->readNextInput(*this);
        }

        virtual IOutputMetaData * queryOutputMeta() const
        {
            return owner->queryInputMeta();
        }

        inline void finishedInputRow()
        {
            if (track)
            {
                track->noteComplete();
                track = NULL;
            }
        }

        inline void finishedProcessing()
        {
            track = NULL;
        }

        inline void setTrack(InputTrack * _track)
        {
            track = _track;
        }

    public:
        ParallelExecutor * owner;
        IInputBase * processor;
        InputTrack * track;
    };


public:
    ParallelExecutor(IInputBase * _input, IOutputMetaData * _outputMeta) : input(_input), outputMeta(_outputMeta)
    {
        tracks = NULL;
        readers = NULL;
        threads = NULL;
    }

    virtual IOutputMetaData * queryOutputMeta() const
    {
        return outputMeta;
    }

    inline IOutputMetaData * queryInputMeta() const
    {
        return input->queryOutputMeta();
    }


    void onCreate(unsigned _numThreads, unsigned _numTracks)
    {
        numThreads = numThreads;
        numTracks = _numTracks ? _numTracks : numThreads * 4;
        assertex(numThreads && numThreads >= numTracks);
        tracks = new InputTrack[numTracks];
        for (unsigned i1=0; i1 < numTracks; i1++)
            tracks[i1].onCreate(readTrackAvailable);
        readers = new ParallelReader * [numThreads];
        threads = new CThreaded * [numThreads];
        for (unsigned i2=0; i2 < numThreads; i2++)
        {
            readers[i2] = new ParallelReader(this);
            threads[i2] = NULL;
        }
    }

    void start()
    {
        inputException.clear();
        processException.clear();
        for (unsigned i1=0; i1 < numTracks; i1++)
            tracks[i1].start();
        for (unsigned i2=0; i2 < numThreads; i2++)
        {
            threads[i2] = new CThreaded("ParallelReaderThread", readers[i2]);
            threads[i2]->start();
        }
    }

    void reset()
    {
        for (unsigned i2=0; i2 < numTracks; i2++)
        {
            threads[i2]->join();
            threads[i2] = NULL;
        }
    }

    void onDestroy()
    {
        for (unsigned i1=0; i1 < numThreads; i1++)
            delete readers[i1];
        delete [] tracks;
        delete [] readers;
        delete [] threads;
    }

    IInputBase * queryInput(unsigned whichThread)
    {
        return readers[whichThread];
    }

    void setProcessor(unsigned whichThread, IInputBase * processor)
    {
        readers[whichThread]->processor = processor;
    }

    const void * readNextInput(ParallelReader & reader)
    {
        CriticalBlock block(inputCrit);
        reader.finishedInputRow();
        if (inputException)
            throw LINK(inputException);
        try
        {
            loop
            {
                if (inputState == RAeof)
                {
                    reader.finishedProcessing();
                    return NULL;
                }

                InputTrack * track = allocateTrack();
                reader.setTrack(track);
                const void * next = input->nextInGroup();
                if (next)
                {
                    inputState = RArow;
                    return next;
                }
                if (inputState == RAeog)
                {
                    track->noteEof();
                    inputState = RAeof;
                }
                else
                    track->noteEog();
                reader.finishedInputRow();
            }
        }
        catch (IException * e)
        {
            inputException.set(e);
            throw;
        }
    }

    InputTrack * allocateTrack()
    {
        writeTrackAvailable.wait();
        InputTrack * track = &tracks[trackToWrite];
        trackToWrite = nextTrack(trackToWrite);
        track->beginProcessing();
        return track;
    }

    void setException(IException * e)
    {
        processException.set(e);
        readTrackAvailable.signal();
    }

    virtual const void * nextInGroup()
    {
        loop
        {
            if (forceAbort || (readState == RAeof))
                return NULL;

            if ((readState == RAstart) || (readState == RAeog))
                readTrackAvailable.wait();

            if (processException)
                throw LINK(processException);

            InputTrack * curTrack = &tracks[trackToRead];
            curTrack->waitForResult();
            if (curTrack->isEof())
                readState = RAeof;
            else if (curTrack->isEog())
            {
                ReadAheadState prevState = readState;
                readState = RAeog;
                trackToRead = nextTrack(trackToRead);
                writeTrackAvailable.signal();
                //Make sure that groups that are filtered by the processor don't generate two eog markers.
                if (prevState == RArow)
                    return NULL;
            }
            else
            {
                const void * next = curTrack->dequeueRow();
                if (next)
                {
                    readState = RArow;
                    return next;
                }

                //Null marks the end of the generated rows a given input row.
                trackToRead = nextTrack(trackToRead);
                writeTrackAvailable.signal();
                readTrackAvailable.wait();
            }
        }
    }

    inline unsigned nextTrack(unsigned track) const
    {
        return (track == numTracks-1) ? 0 : track+1;
    }


protected:
    IInputBase * input;
    Owned<IException> inputException;
    Owned<IException> processException;
    IOutputMetaData * outputMeta;
    InputTrack * tracks;
    ParallelReader * * readers;
    CThreaded * * threads;
    Semaphore writeTrackAvailable;
    Semaphore readTrackAvailable;
    CriticalSection inputCrit;
    unsigned trackToRead;         // only accessed from read code
    unsigned trackToWrite;        // only accessed from write code
    unsigned numThreads;
    unsigned numTracks;
    ReadAheadState inputState;      // only accessed from write code
    ReadAheadState readState;       // only accessed from read code
    bool forceAbort;
};
