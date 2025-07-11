/*##############################################################################
    HPCC SYSTEMS software Copyright (C) 2023 HPCC Systems®.
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

#pragma once

#include <list>
#include <string>
#include <utility>
#include <vector>


#include "platform.h"
#include "jscm.hpp"

namespace k8s {

// returns a vector of {pod-name, node-name} vectors,
jlib_decl std::vector<std::vector<std::string>> getPodNodes(const char *selector);
jlib_decl const char *queryMyPodName();
jlib_decl const char *queryMyContainerName();
jlib_decl const char *queryMyJobName();
jlib_decl const char *queryPodSuffix();

enum class KeepJobs { none, podfailures, all };
jlib_decl KeepJobs translateKeepJobs(const char *keepJobs);
constexpr unsigned defaultDeleteJobGracePeriod = 120; // this is time during deleteResource. Time for pod containers to stop and capture any info.

jlib_decl bool isActiveService(const char *serviceName);
jlib_decl void deleteResource(const char *componentName, const char *job, const char *resource);
jlib_decl void waitJob(const char *componentName, const char *resourceType, const char *job, unsigned pendingTimeoutSecs, unsigned totalWaitTimeSecs, KeepJobs keepJob);
jlib_decl bool applyYaml(const char *componentName, const char *wuid, const char *job, const char *resourceType, const std::list<std::pair<std::string, std::string>> &extraParams, bool optional, bool autoCleanup);
jlib_decl void runJob(const char *componentName, const char *wuid, const char *job, const std::list<std::pair<std::string, std::string>> &extraParams={});

constexpr unsigned defaultKubectlRetrySecs = 60;
extern jlib_decl void runKubectlCommand(const char *title, const char *cmd, const char *input, StringBuffer *output, unsigned retrySecs=defaultKubectlRetrySecs);

// return the k8s external host and port for serviceName
extern jlib_decl std::pair<std::string, unsigned> getExternalService(const char *serviceName);

extern jlib_decl std::pair<std::string, unsigned> getDafileServiceFromConfig(const char *application, bool secure, bool errorIfMissing);

extern jlib_decl StringBuffer &addInstanceContextPaths(StringBuffer &dst);

}
