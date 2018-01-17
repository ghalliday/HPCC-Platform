/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2018 HPCC Systems®.

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

#ifndef __HQLCACHE_HPP_
#define __HQLCACHE_HPP_

/*
 * This interface represents cached information about an ECL definition.  If it is up to date then
 * the stored information can be used to optimize creating an archive, and parsing source code.
 */
interface IEclCachedDefinition : public IInterface
{
public:
    virtual timestamp_type getTimeStamp() const = 0;
    virtual bool isUpToDate() const = 0;
    virtual IFileContents * querySimplifiedEcl() const = 0;
    virtual void queryDependencies(StringArray & values) const = 0;
};

/*
 * This interface is used to locate a cached definition for a scoped reference.  There are at least two
 * implementations - a directory tree and a compound cache file (useful for regression testing)
 */
interface IEclCachedDefinitionCollection : public IInterface
{
    virtual IEclCachedDefinition * getDefinition(const char * path) = 0;
};


extern HQL_API IEclCachedDefinitionCollection * createEclXmlCachedDefinitionCollection(IEclRepository * repository, IPropertyTree * root);

extern HQL_API void convertSelectsToPath(StringBuffer & filename, const char * eclPath);


#endif
