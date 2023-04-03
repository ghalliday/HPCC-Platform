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

#option ('globalFold', false);
ppersonRecord := RECORD
string10    surname ;
varstring10 forename;
unicode     uf1;
varunicode10 uf2;
decimal10_2  df;
integer4    nl;
  END;


pperson := DATASET([{'','',U'',U'',0,0}], ppersonRecord);

output(pperson, {hashmd5(surname),hashmd5(forename)});
output(pperson, {hashmd5(uf1),hashmd5(uf2)});
output(pperson, {hashmd5(df),hashmd5(nl)});
output(pperson, {data20 hashsurname := hashmd5(surname), data10 hashforename := hashmd5(forename)});


string compareMD5(data16 l, data16 r) :=
    if(l=r,'MD5 match', 'MD5 do not match "' + l + '","' + r);


compareMD5(x'00000000000000000000000000000001',     x'00000000000000000000000000000001');
compareMD5(HASHMD5(''),                             x'd41d8cd98f00b204e9800998ecf8427e');
compareMD5(HASHMD5('a'),                            x'0cc175b9c0f1b6a831c399e269772661');
compareMD5(HASHMD5('abc'),                          x'900150983cd24fb0d6963f7d28e17f72');
compareMD5(HASHMD5('message digest'),               x'f96b697d7cb7938d525a2f31aaf161d0');
compareMD5(HASHMD5('abcdefghijklmnopqrstuvwxyz'),   x'c3fcd3d76192e4007dfb496cca67e13b');
compareMD5(HASHMD5('ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789'), x'd174ab98d277d9f5a5611c2c9f419d9f');
compareMD5(HASHMD5('12345678901234567890123456789012345678901234567890123456789012345678901234567890'), x'57edf4a22be3c955ac49da2e2107b67a');


import $.setup;

multiPart := false;
sq := setup.sq(multiPart);

ds := sq.SimplePersonBookDs;


r0 := RECORDOF(ds);

r1 := RECORD(r0)
  DATA16 hash1a;
  DATA16 hash1b;
END;

r2 := RECORD(r1)
  DATA16 hash2a;
  DATA16 hash2b;
END;

r1 t1(r0 l) := TRANSFORM
  SELF.hash1a := HASHMD5(l.books);
  SELF.hash1b := HASHMD5(TRANSFER(l.books, DATA));
  SELF := l;
END;

r2 t2(r1 l) := TRANSFORM
  SELF.hash2a := HASHMD5(l.books);
  SELF.hash2b := HASHMD5(TRANSFER(l.books, DATA));
  SELF := l;
END;

p1 := PROJECT(ds, t1(LEFT));
p2 := PROJECT(NOCOMBINE(p1), t2(LEFT));

output(p2, { hash1a, hash1a = hash1b, hash1a = hash2a, hash1b = hash2b });
