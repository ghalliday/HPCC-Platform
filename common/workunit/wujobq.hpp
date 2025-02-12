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

#ifndef WUJOBQ_HPP
#define WUJOBQ_HPP


#include "jsocket.hpp"
#include "dasess.hpp"

/*
 * The job queues have the following semantics.
 *
 * Items are queued with a given priority, at a given offset
 * If no position is given, then the insertion position in the queue is determined by finding the first item with a lower priority
 * If a position is given, then the priority may be adjusted to ensure it is consistent with the items before it and after it in the queue
 *
 * When an item is dequeued, the head of the queue is removed.
 *
 * There is an option in acceptConversation(), currently used in thor, to wait for up to 30 seconds if the priority at the header of the queue is lower
 * than the last item dequeued from that queue.  I think this is to ensure that high priority workunits get precedence even if there
 * are short pauses between graphs.  However, this will prevent all thor instances for the same queue from dequeuing for that period.
 *
 * NOTE: If this logic is included I think this should really be the priority of the last item THIS thor dequeued.
 *
 *
 * We want to add the following semantics:
 *
 * When a server requests to dequeue an item it can pass a worker priority.
 * - If there is an item on the queue, then dequeue it
 * - Otherwise record the worker priority in the Client information.  (If there are multiple threads do they have to have the same priority??)
 * - When an item is received, the WAITING worker with the highest priority gets to process it.
 *
 * Problems:
 *   Multiple threads from a single client
 *   Ensuring there are no race conditions
 *
 */
interface IJobQueueItem: extends serializable
{
    virtual const char *queryWUID()=0;
    virtual const char *queryOwner()=0;
    virtual int getPriority()=0;
    virtual SessionId getSessionId()=0;
    virtual SocketEndpoint &queryEndpoint()=0;      // dali client ep

    virtual unsigned getPort()=0;                   // conversation port (not used for DFU server)
    virtual bool equals(IJobQueueItem *other)=0;

    virtual void setPriority(int priority)=0;
    virtual void setOwner(const char *owner)=0;
    virtual void setEndpoint(const SocketEndpoint &ep)=0;  
    virtual void setPort(unsigned)=0;            
    virtual void setSessionId(SessionId id)=0;
    virtual bool isValidSession()=0;                // used before conversation started

    virtual CDateTime &queryEnqueuedTime()=0;          // when was enqueued
    virtual void setEnqueuedTime(const CDateTime &dt)=0;
};

typedef IIteratorOf<IJobQueueItem> IJobQueueIterator;

#ifdef WORKUNIT_EXPORTS
    #define WORKUNIT_API DECL_EXPORT
#else
    #define WORKUNIT_API DECL_IMPORT
#endif

class WORKUNIT_API CJobQueueContents: public IArrayOf<IJobQueueItem>
{  // used as a 'snapshot' of queue items
public:
    IJobQueueIterator *getIterator(); // only valid during lifetime of CJobQueueContents    
};

interface IJobQueueConst: extends IInterface
{
    virtual unsigned ordinality()=0;            // number of items on queue
    virtual unsigned waiting()=0;               // number currently waiting on dequeue
    virtual IJobQueueItem *getItem(unsigned idx)=0;
    virtual IJobQueueItem *getHead()=0;
    virtual IJobQueueItem *getTail()=0;
    virtual IJobQueueItem *find(const char *wuid)=0;
    virtual unsigned findRank(const char *wuid)=0;
    virtual unsigned copyItems(CJobQueueContents &dest)=0;  // takes a snapshot copy of the entire queue (returns number copied)
    virtual bool getLastDequeuedInfo(StringAttr &wuid, CDateTime &enqueuedt, int &priority)=0;
    virtual void copyItemsAndState(CJobQueueContents& contents, StringBuffer& state, StringBuffer& stateDetails)=0;
    virtual void getState(StringBuffer& state, StringBuffer& stateDetails)=0;
    virtual bool paused()=0;    // true if paused
    virtual bool paused(StringBuffer& info)=0;    // true if paused
    virtual bool stopped()=0;   // true if stopped
    virtual bool stopped(StringBuffer& info)=0;   // true if stopped
};

interface IJobQueue: extends IJobQueueConst
{

// enqueuing
    // the following enqueues all take ownership of qitem passed
    virtual void enqueue(IJobQueueItem *qitem)=0;
    virtual void enqueueHead(IJobQueueItem *qitem)=0;
    virtual void enqueueTail(IJobQueueItem *qitem)=0;
    virtual void enqueueBefore(IJobQueueItem *qitem,const char *nextwuid)=0;
    virtual void enqueueAfter(IJobQueueItem *qitem,const char *prevwuid)=0;

// dequeueing
    virtual void connect(bool validateitemsessions)=0;     // must be called before dequeueing
                                                                // validateitemsessions ensures that all queue items have running session
    virtual IJobQueueItem *dequeue(unsigned timeout=INFINITE)=0;
    virtual IJobQueueItem *dequeue(int minPrio, unsigned timeout, unsigned prioritytransitiondelay)=0;
    virtual IJobQueueItem *dequeuePriority(unsigned __int64 priority, unsigned timeout=INFINITE)=0;
    virtual void disconnect()=0;    // signal no longer wil be dequeing (optional - done automatically on release)
    virtual void getStats(unsigned &connected,unsigned &waiting, unsigned &enqueued)=0; // this not quick as validates clients still running
    virtual bool waitStatsChange(unsigned timeout)=0;
    virtual void cancelWaitStatsChange()=0;

//manipulation
    virtual IJobQueueItem *take(const char *wuid)=0; // finds and removes
    virtual bool moveBefore(const char *wuid,const char *nextwuid)=0;
    virtual bool moveAfter(const char *wuid,const char *prevwuid)=0;
    virtual bool moveToHead(const char *wuid)=0;
    virtual bool moveToTail(const char *wuid)=0;
    virtual bool remove(const char *wuid)=0;
    virtual bool changePriority(const char *wuid,int value)=0;
    virtual void clear()=0;                     // removes all items

// transactions (optional)
    virtual void lock()=0;          
    virtual void unlock(bool rollback=false)=0;

// control:
    virtual void pause()=0;     // marks queue as paused - and subsequent dequeues block until resumed
    virtual void pause(const char *info)=0;     // marks queue as paused - and subsequent dequeues block until resumed
    virtual void stop()=0;      // sets stopped flags - all current and subsequent dequeues return NULL
    virtual void stop(const char *info)=0;      // sets stopped flags - all current and subsequent dequeues return NULL
    virtual void resume()=0;    // removes paused or stopped flag
    virtual void resume(const char *info)=0;    // removes paused or stopped flag

// conversations:
    virtual IConversation *initiateConversation(IJobQueueItem *item)=0; // does enqueue - take ownership of item
    virtual IConversation *acceptConversation(IJobQueueItem *&item,unsigned prioritytransitiondelay=0)=0;
                                                                        // does dequeue - returns queue item dequeued
    virtual void cancelInitiateConversation()=0;                        // cancels initiateConversation in progress
    virtual bool cancelInitiateConversation(const char *wuid)=0;        // cancels remote initiate
    virtual void cancelAcceptConversation()=0;                          // cancels acceptConversation in progress

    virtual const char * queryActiveQueueName()=0;
    virtual void setActiveQueue(const char *name)=0;
    virtual const char *nextQueueName(const char *last)=0;

};

interface IJQSnapshot : extends IInterface
{
    virtual IJobQueueConst *getJobQueue(const char *name)=0;
};

extern WORKUNIT_API IJQSnapshot *createJQSnapshot();

extern WORKUNIT_API IJobQueueItem *createJobQueueItem(const char *wuid);
extern WORKUNIT_API IJobQueueItem *deserializeJobQueueItem(MemoryBuffer &mb); 

extern WORKUNIT_API IJobQueue *createJobQueue(const char *name);

extern WORKUNIT_API bool queueJobIfQueueWaiting(IJobQueue *queue, IJobQueueItem *item, unsigned maxTimeMs, unsigned intervalMs);

extern bool WORKUNIT_API runWorkUnit(const char *wuid, const char *queueName);
extern bool WORKUNIT_API runWorkUnit(const char *wuid);
extern WORKUNIT_API StringBuffer & getQueuesContainingWorkUnit(const char *wuid, StringBuffer &queueList);
extern WORKUNIT_API void removeWorkUnitFromAllQueues(const char *wuid);

extern bool WORKUNIT_API switchWorkUnitQueue(IWorkUnit* wu, const char *cluster);

#endif
