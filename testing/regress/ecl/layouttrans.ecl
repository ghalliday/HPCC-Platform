/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems.

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

//MORE: Thor could support it
//NoThor
//class=index
#option ('layoutTranslationEnabled', true);

import $.Setup;
Files := Setup.Files('hthor', false);

DG_FetchIndex1Name := Files.DG_FetchIndex1Name;
DG_FetchIndex1 := Files.DG_FetchIndex1;
DG_FetchFile := Files.DG_FetchFile;

//Swap the key fields.
DG_FetchIndex1Alt1 := INDEX(DG_FetchFile,{Fname,Lname},{state, STRING blobfield {blob}:= fname, STRING tfn := TRIM(Fname), __filepos},DG_FetchIndex1Name);
//Swap key fields, and some fields missing.
DG_FetchIndex1Alt2 := INDEX(DG_FetchFile,{Fname,Lname},{ STRING blobfield {blob}:= fname, __filepos},DG_FetchIndex1Name);

ds := DATASET([{'Anderson'}, {'Doe'}], {STRING25 Lname});

SEQUENTIAL(
    OUTPUT(SORT(DG_FetchIndex1(Lname = 'Smith'), record), {Fname, Lname}),
    OUTPUT(SORT(DG_FetchIndex1Alt1(Lname = 'Smith'), record), {Fname, Lname}),
    OUTPUT(SORT(DG_FetchIndex1Alt2(Lname = 'Smith'), record), {Fname, Lname}),
    OUTPUT(SORT(DG_FetchIndex1((Lname = 'Smith') AND (Fname >= 'Z')), record), {Fname, Lname}),
    OUTPUT(SORT(DG_FetchIndex1Alt1((Lname = 'Smith') AND (Fname >= 'Z')), record), {Fname, Lname}),
    OUTPUT(SORT(DG_FetchIndex1Alt2((Lname = 'Smith') AND (Fname >= 'Z')), record), {Fname, Lname}),
    OUTPUT(SORT(JOIN(ds, DG_FetchIndex1, LEFT.Lname = RIGHT.Lname), record), {Fname, Lname}),
    OUTPUT(SORT(JOIN(ds, DG_FetchIndex1Alt1, LEFT.Lname = RIGHT.Lname), record), {Fname, Lname}),
    OUTPUT(SORT(JOIN(ds, DG_FetchIndex1Alt2, LEFT.Lname = RIGHT.Lname), record), {Fname, Lname}),

    OUTPUT(COUNT(SORT(DG_FetchIndex1(Lname = 'Smith'), record))),
    OUTPUT(COUNT(SORT(DG_FetchIndex1Alt1(Lname = 'Smith'), record))),
    OUTPUT(COUNT(SORT(DG_FetchIndex1Alt2(Lname = 'Smith'), record))),
    OUTPUT(COUNT(SORT(DG_FetchIndex1((Lname = 'Smith') AND (Fname >= 'Z')), record))),
    OUTPUT(COUNT(SORT(DG_FetchIndex1Alt1((Lname = 'Smith') AND (Fname >= 'Z')), record))),
    OUTPUT(COUNT(SORT(DG_FetchIndex1Alt2((Lname = 'Smith') AND (Fname >= 'Z')), record))),
    OUTPUT(COUNT(SORT(JOIN(ds, DG_FetchIndex1, LEFT.Lname = RIGHT.Lname), record))),
    OUTPUT(COUNT(SORT(JOIN(ds, DG_FetchIndex1Alt1, LEFT.Lname = RIGHT.Lname), record))),
    OUTPUT(COUNT(SORT(JOIN(ds, DG_FetchIndex1Alt2, LEFT.Lname = RIGHT.Lname), record))),

    OUTPUT(TABLE(SORT(DG_FetchIndex1(Lname = 'Smith'), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))})),
    OUTPUT(TABLE(SORT(DG_FetchIndex1Alt1(Lname = 'Smith'), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))})),
    OUTPUT(TABLE(SORT(DG_FetchIndex1Alt2(Lname = 'Smith'), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))})),
    OUTPUT(TABLE(SORT(DG_FetchIndex1((Lname = 'Smith') AND (Fname >= 'Z')), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))})),
    OUTPUT(TABLE(SORT(DG_FetchIndex1Alt1((Lname = 'Smith') AND (Fname >= 'Z')), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))})),
    OUTPUT(TABLE(SORT(DG_FetchIndex1Alt2((Lname = 'Smith') AND (Fname >= 'Z')), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))})),
    OUTPUT(TABLE(SORT(JOIN(ds, DG_FetchIndex1, LEFT.Lname = RIGHT.Lname), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))})),
    OUTPUT(TABLE(SORT(JOIN(ds, DG_FetchIndex1Alt1, LEFT.Lname = RIGHT.Lname), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))})),
    OUTPUT(TABLE(SORT(JOIN(ds, DG_FetchIndex1Alt2, LEFT.Lname = RIGHT.Lname), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))})),

    OUTPUT(SORT(TABLE(SORT(DG_FetchIndex1(Lname = 'Smith'), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))}, Fname),s)),
    OUTPUT(SORT(TABLE(SORT(DG_FetchIndex1Alt1(Lname = 'Smith'), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))}, Fname),s)),
    OUTPUT(SORT(TABLE(SORT(DG_FetchIndex1Alt2(Lname = 'Smith'), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))}, Fname),s)),
    OUTPUT(SORT(TABLE(SORT(DG_FetchIndex1((Lname = 'Smith') AND (Fname >= 'Z')), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))}, Fname),s)),
    OUTPUT(SORT(TABLE(SORT(DG_FetchIndex1Alt1((Lname = 'Smith') AND (Fname >= 'Z')), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))}, Fname),s)),
    OUTPUT(SORT(TABLE(SORT(DG_FetchIndex1Alt2((Lname = 'Smith') AND (Fname >= 'Z')), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))}, Fname),s)),
    OUTPUT(SORT(TABLE(SORT(JOIN(ds, DG_FetchIndex1, LEFT.Lname = RIGHT.Lname), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))}, Fname),s)),
    OUTPUT(SORT(TABLE(SORT(JOIN(ds, DG_FetchIndex1Alt1, LEFT.Lname = RIGHT.Lname), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))}, Fname),s)),
    OUTPUT(SORT(TABLE(SORT(JOIN(ds, DG_FetchIndex1Alt2, LEFT.Lname = RIGHT.Lname), record), {s := SUM(GROUP, LENGTH(TRIM(Fname)))}, Fname),s))

);
