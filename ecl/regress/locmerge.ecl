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

unsigned numrecs := 1000 : stored('numrecs');

trec := record
         unsigned4 seq;
         unsigned4 strm;
         unsigned4 nodenum;
         unsigned4 key;
       end;

seed1 := dataset([{0, 1, 0, 0}], trec);
seed2 := dataset([{0, 2, 0, 0}], trec);

trec addNodeNum(trec L, unsigned4 c) := transform
    SELF.nodenum := c-1;
    SELF := L;
  END;

dataset one_per_node(dataset(trec) seed) := distribute(normalize(seed, CLUSTERSIZE, addNodeNum(LEFT, COUNTER)), nodenum);

trec generatePseudoRandom(trec L, unsigned4 c) := transform
    SELF.seq := c;
    SELF.key := (RANDOM()%900);
    SELF := L;
  END;

dataset bigstream(dataset(trec) seed) := NORMALIZE(one_per_node(seed), numrecs, generatePseudoRandom(LEFT, counter));
dataset sortedrecs(dataset(trec) seed) := sort(bigstream(seed), key, local);
mergedrecs :=      merge(sortedrecs(seed1),sortedrecs(seed2),local);

output(mergedrecs);