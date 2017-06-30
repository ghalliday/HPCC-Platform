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

//class=index
//version multiPart=false
//version multiPart=true
//version multiPart=true,useLocal=true
//version multiPart=true,useTranslation=true,nothor

import ^ as root;
multiPart := #IFDEFINED(root.multiPart, true);
useLocal := #IFDEFINED(root.useLocal, false);
useTranslation := #IFDEFINED(root.useTranslation, false);

//--- end of version configuration ---

#option ('layoutTranslationEnabled', useTranslation);
#onwarning (5402, ignore);

import $.setup;
Files := setup.Files(multiPart, useLocal, useTranslation);

IMPORT Std;

set of string3 LnameValues := ['An','Jo'];
FnameValues := ['J','F'];

SavedLnameValues := LnameValues : independent;
SavedFnameValues := FnameValues : independent;
unsigned FNameLen := 1 : independent;
unsigned LNameLen := 2 : independent;
// try it with just one limit

sequential(
    OUTPUT(LIMIT(Files.DG_FetchIndex(KEYED(Lname[1..LNameLen] in LnameValues) AND (fname[1..1] IN FNameValues)),999999), {Lname,Fname});
    OUTPUT(LIMIT(Files.DG_FetchIndex(KEYED(Lname[1..LNameLen] in LnameValues) AND (fname[1..1] IN SavedFnameValues)),999999), {Lname,Fname});
    OUTPUT(LIMIT(Files.DG_FetchIndex(KEYED(Lname[1..LNameLen] in SavedLnameValues) AND (fname[1..FNameLen] IN SavedFnameValues)),999999), {Lname,Fname});
);
