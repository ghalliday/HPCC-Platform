/*##############################################################################

    Copyright (C) 2025 HPCC Systems®.

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

#include "eventoperation.h"
#include "eventfilter.h"

bool CEventConsumingOp::ready() const
{
    return !inputPath.isEmpty() && out.get();
}

void CEventConsumingOp::setInputPath(const char* path)
{
    inputPath.set(path);
}

void CEventConsumingOp::setOutput(IBufferedSerialOutputStream& _out)
{
    out.set(&_out);
}

bool CEventConsumingOp::acceptEvents(const char* eventNames)
{
    return ensureFilter()->acceptEvents(eventNames);
}

bool CEventConsumingOp::acceptAttribute(EventAttr attr, const char* values)
{
    return ensureFilter()->acceptAttribute(attr, values);
}

IEventFilter* CEventConsumingOp::ensureFilter()
{
    if (!filter)
        filter.setown(createEventFilter());
    return filter;
}

bool CEventConsumingOp::traverseEvents(const char* path, IEventAttributeVisitor& visitor)
{
    if (filter)
    {
        filter->setTarget(visitor);
        return readEvents(path, *filter);
    }
    return readEvents(path, visitor);
}
