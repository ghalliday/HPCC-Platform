/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2017 HPCC Systems®.

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

r := record
  integer i;
end;

r t(integer c) := transform
  SELF.i := c;
end;

ds1 := NOCOMBINE(dataset(10000, t(COUNTER), distributed));


loopbody(dataset(r) infile) := JOIN(infile, infile   , LEFT.i*2 = RIGHT.i, transform(r, SELF.i := LEFT.i/2), LOOKUP);

l := LOOP(ds1, LEFT.i % 2 = 0, exists(rows(left)), loopbody(rows(left)));
output(count(l));
