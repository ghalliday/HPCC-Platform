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

layout := RECORD
                unsigned1 field1;
                unsigned1           field2;
                unsigned1 field3;
                string mydata;
END;

fieldStr := 'field1, field3';

fieldExp := MACRO 
                #expand('field1, field3')
ENDMACRO;

fieldlist := MACRO
   field1, field3
ENDMACRO;

ds := DATASET([{1,1,1, 'some data'},
            {1,1,2, 'some more data'},
            {1,2,4, 'even more data'},
            {2,1,1, 'blah blah'},
            {1,10,3, 'more'},
            {1,100,2, 'dup data'}], layout);

output(ds, named('original'));

//--------------------------------------------------------------------------------------------
// Approach 1 – won’t compile

ds_sorted1 := SORT(ds, fieldlist());

ds_deduped1 := DEDUP(ds_sorted1, fieldlist());

output(ds_sorted1, named('sorted1'));
output(ds_deduped1, named('deduped1'));

//--------------------------------------------------------------------------------------------
// Approach 2 – works but is more cumbersome than it feels like it should be

ds_sorted2 := SORT(ds, #expand(fieldStr));

ds_deduped2 := DEDUP(ds_sorted2, #expand(fieldStr));


output(ds_sorted2, named('sorted2'));
output(ds_deduped2, named('deduped2'));


//----------------------------------------------------------------------------------------------
// Approach 3 – feels like a reasonable compromise, but can’t get it to compile either

ds_sorted3 := SORT(ds, fieldExp());

ds_deduped3 := DEDUP(ds_sorted3,fieldExp());


output(ds_sorted3, named('sorted3'));
output(ds_deduped3, named('deduped3'));

