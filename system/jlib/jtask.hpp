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



#ifndef JTASK_HPP
#define JTASK_HPP

#include "jiface.hpp"
#include "jsem.hpp"

//This is an abstract class
class ATask : public CSimpleInterface
{
//Functions that must be implemented by derived classes
public:
    inline ATask(unsigned _ancestors) { setNumAncestors(_ancestors); }

    virtual ATask * execute() = 0;
    virtual void onComplete() = 0;

//Public implemented functions
public:
    inline void setNumAncestors(unsigned num) { atomic_set(&ancestors, num); }
    inline bool noteAncestorComplete() { return atomic_dec_and_test(&ancestors); }

private:
    atomic_t ancestors;
};

//--------------------------------------------------------------------------------------------------------------------

extern jlib_decl void scheduleTask(ATask & task, bool isLocal);
inline void noteAncestorCompleteAndSchedule(ATask & task)
{
    if (task.noteAncestorComplete())
        scheduleTask(OLINK(task), true);
}

//--------------------------------------------------------------------------------------------------------------------

class NullTask : public ATask
{
public:
    inline NullTask(unsigned _ancestors) : ATask(_ancestors) {}

    virtual ATask * execute() { return NULL; }
    virtual void onComplete() {}
};

class SignalTask : public NullTask
{
public:
    SignalTask(Semaphore & _sem) : NullTask(1), sem(_sem) {}

    virtual ATask * execute() { sem.signal(); return NULL; }

protected:
    Semaphore & sem;
};

class AncestorTask : public ATask
{
public:
    AncestorTask(unsigned _ancestors, ATask * _next) : ATask(_ancestors), next(_next) {}
    virtual void onComplete()
    {
        if (next)
            noteAncestorCompleteAndSchedule(*next);
    }
    ATask * getNextTask()
    {
        if (next)
        {
            if (next->noteAncestorComplete())
                return next.getClear();
            next.clear();
        }
        return NULL;
    }
protected:
    Owned<ATask> next;
};

extern jlib_decl ATask * queryActiveTask();

//--------------------------------------------------------------------------------------------------------------------

interface ITaskManager
{
    virtual unsigned getNumParallel() const = 0;
    virtual unsigned getLog2Parallel() const = 0;
    virtual void schedule(ATask & task, bool isLocal) = 0;
    virtual void setNumParallel(unsigned _numCpus) = 0;
};

extern jlib_decl ITaskManager & queryTaskManager();
extern jlib_decl void initTaskManager(unsigned numCpus);

//--------------------------------------------------------------------------------------------------------------------

interface ITaskExecutor
{
    virtual void schedule(ATask & task, bool isLocal) = 0;
};

extern jlib_decl ITaskManager * queryTaskExecutor();

#endif
