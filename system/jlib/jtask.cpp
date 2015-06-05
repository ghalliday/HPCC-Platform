/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2015 HPCC Systems.

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
#include <string.h>
#include <limits.h>
#include "jdebug.hpp"
#include "jtask.hpp"
#include "jqueue.tpp"
#include "jexcept.hpp"
#include "jthread.hpp"
#include "jset.hpp"

static __thread ATask * currentTask;
extern jlib_decl ATask * queryActiveTask() { return currentTask; }


/*
 Linking rules:
 - task passed to schedule must be linked before calling.
 - value returned from execute() must be linked.
 - task is release when execution has completed
 */

inline ATask * executeTask(ATask & task)
{
    assertex(!currentTask);
    ATask * next = task.execute();
    currentTask = NULL;
    task.onComplete();
    task.Release();
    return next;
}

//--------------------------------------------------------------------------------------------------------------------

class ATaskManager : public ITaskManager
{
public:
    virtual ~ATaskManager() {}
};

//Primarily useful for debugging tasks
class SingleThreadedTaskManager : public ATaskManager
{
public:
    virtual unsigned getNumParallel() const { return 1; }
    virtual unsigned getLog2Parallel() const { return 1; }
    virtual void schedule(ATask & task, bool isLocal)
    {
        ATask * cur = &task;
        do
        {
            cur = executeTask(*cur);
        } while (cur);
    }
    virtual void setNumParallel(unsigned _numCpus) {}
};

//--------------------------------------------------------------------------------------------------------------------

class TaskManager : public ATaskManager
{
private:
    friend class TaskExecutionThread;
    class TaskExecutionThread : public Thread, implements ITaskExecutor
    {
    public:
        TaskExecutionThread(TaskManager * _parent)
            : Thread("TaskExecutionThread")
        {
            parent = _parent;
        }

        int run()
        {
            loop
            {
                ATask * task = parent->nextTask();
                if (!task)
                    break;
                do
                {
                    task = executeTask(*task);
                } while (task);
            }
            parent->noteThreadStopped(*this);
            return 0;
        }

        virtual void schedule(ATask & task, bool isLocal)
        {
            //MORE? Add local queues?
            parent->schedule(task, isLocal);
        }

        TaskManager * parent;
    };

public:
    TaskManager(unsigned _numCpus) : numCpus(0), ln2Cpus(1)
    {
        atomic_set(&threadsToKill, 0);
        setNumParallel(_numCpus);
    }

    ~TaskManager()
    {
        setNumParallel(0);
    }

    virtual unsigned getNumParallel() const { return numCpus; }
    virtual unsigned getLog2Parallel() const { return ln2Cpus; }

    virtual void setNumParallel(unsigned newNumCpus)
    {
        unsigned numToWaitFor = 0;
        {
            CriticalBlock block2(threadCs);  // Protect threads variable
            if (newNumCpus > numCpus)
            {
                for (unsigned i = numCpus; i < newNumCpus; i++)
                {
                    TaskExecutionThread * thread = new TaskExecutionThread(this);
                    threads.append(*thread);
                    thread->start();
                }
            }
            else
            {
                numToWaitFor = numCpus - newNumCpus;
                atomic_add(&threadsToKill, numToWaitFor);
                taskAvailable.signal(numToWaitFor);
            }
            numCpus = newNumCpus;
            //ln2Cpus defined so that 2^(ln2Cpus-1) <= numCpus <= 2^ln2Cpus
            if (numCpus <= 1)
                ln2Cpus = 0;
            else
                ln2Cpus = getMostSignificantBit(numCpus-1);
            assertex(numCpus <= (1U << ln2Cpus));

            SpinBlock block1(taskCs); // Protect task
            tasks.ensure(20 * numCpus);
        }

        for (unsigned i=0; i < numToWaitFor; i++)
            threadsStopped.wait();
    }

    virtual ATask * nextTask()
    {
        taskAvailable.wait();
        //Check for closing some threads down - fast path is num = 0.  Don't block the spin block while checking.
        if (atomic_read(&threadsToKill))
        {
            loop
            {
                unsigned numToKill = atomic_read(&threadsToKill);
                if (numToKill == 0)
                    break;
                if (atomic_cas(&threadsToKill, numToKill-1, numToKill))
                    return NULL;
            }
        }
        SpinBlock block(taskCs);
        return tasks.dequeueTail();
    }

    virtual void schedule(ATask & task, bool isLocal)
    {
        {
            SpinBlock block(taskCs);
            if (isLocal)
                tasks.enqueue(&task);
            else
                tasks.enqueueHead(&task);
        }
        taskAvailable.signal();
    }

protected:
    void noteThreadStopped(TaskExecutionThread & thread)
    {
        {
            CriticalBlock block(threadCs);
            threads.zap(thread);
        }
        threadsStopped.signal();
    }

protected:
    unsigned numCpus;
    unsigned ln2Cpus;
    atomic_t threadsToKill;
    Semaphore alive;
    Semaphore taskAvailable;
    Semaphore threadsStopped;
    CIArrayOf<TaskExecutionThread> threads;
    CriticalSection threadCs;
    SpinLock taskCs;
    QueueOf<ATask,false> tasks;
};

//--------------------------------------------------------------------------------------------------------------------

static CriticalSection cs;
static ATaskManager * volatile taskManager;
ITaskManager & queryTaskManager()
{
    if (taskManager)
        return *taskManager;
    CriticalBlock block(cs);
    ATaskManager * ret = taskManager;
    if (!ret)
        taskManager = ret = new TaskManager(getAffinityCpus());
    return *ret;
}

void initTaskManager(unsigned numCpus)
{
    CriticalBlock block(cs);
    delete taskManager;
    taskManager = new TaskManager(getAffinityCpus());
}

void killTaskManager()
{
    CriticalBlock block(cs);
    delete taskManager;
    taskManager = NULL;
}

//--------------------------------------------------------------------------------------------------------------------

void scheduleTask(ATask & task, bool isLocal)
{
    queryTaskManager().schedule(task, isLocal);
}

MODULE_INIT(INIT_PRIORITY_JLIB_DEPENDENT)
{
//    taskManager = new SingleThreadedTaskManager;
    return true;
}
MODULE_EXIT()
{
    killTaskManager();
}
