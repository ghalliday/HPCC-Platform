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
#include "jset.hpp"

static __thread ATask * currentTask;
extern jlib_decl ATask * queryActiveTask() { return currentTask; }


/*
 Linking rules:
 - task passed to schedule must be linked before calling.
 - value returned from execute() must be linked.
 - task is release when execution has completed
 */

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
    virtual void schedule(ATask & task)
    {
        assertex(!currentTask);
        ATask * next = task.execute();
        currentTask = NULL;
        task.onComplete();
        task.Release();
        if (next)
            schedule(*next);
    }
    virtual void setNumParallel(unsigned _numCpus) {}
};


class TaskManager : public ATaskManager
{
public:
    TaskManager(unsigned _numCpus);
    ~TaskManager()
    {
        setNumParallel(0);
    }

    virtual unsigned getNumParallel() const { return numCpus; }
    virtual unsigned getLog2Parallel() const { return ln2Cpus; }

    virtual void setNumParallel(unsigned _numCpus);

    virtual void schedule(ATask & task)
    {
        task.execute();
    }

protected:
    unsigned numCpus;
    unsigned ln2Cpus;
};

TaskManager::TaskManager(unsigned _numCpus) : numCpus(0)
{
    setNumParallel(_numCpus);
}

void TaskManager::setNumParallel(unsigned _numCpus)
{
    numCpus = _numCpus;
    if (numCpus == 0)
        ln2Cpus = 1;
    else
        ln2Cpus = getMostSignificantBit(numCpus);
    throwUnexpected();//trace this
}

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

extern jlib_decl void scheduleTask(ATask & task)
{
    queryTaskManager().schedule(task);
}

MODULE_INIT(INIT_PRIORITY_JLIB_DEPENDENT)
{
    taskManager = new SingleThreadedTaskManager;
    return true;
}
MODULE_EXIT()
{
    killTaskManager();
}
