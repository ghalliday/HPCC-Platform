<Archive build="internal_7.0.3-closedown0"
         eclVersion="7.0.3"
         legacyImport="0"
         legacyWhen="0">
 <Query attributePath="query"/>
 <Module key="" name="">
  <Attribute key="query"
             name="query"
             sourcePath="/home/gavin/temp/example/query.ecl"
             ts="1543859905000000">
   import TA, TB, TC, TD;

output(TA.f1);
output(TB.TB.f1);
output(TC.TC.f1);
output(TC.TC.f2);
output(TC.TC.f3);
output(TD.x.TD.y.f1);
output(TD.x.TD.y.f2);&#10;&#10;
  </Attribute>
 </Module>
 <Module key="ta" name="TA">
  <Attribute key="f1"
             name="f1"
             sourcePath="/home/gavin/temp/example/./TA/f1.ecl"
             ts="1543853211000000">
   import TA;
export f1 := Ta.c1;&#10;&#10;
  </Attribute>
  <Attribute key="c1"
             name="c1"
             sourcePath="/home/gavin/temp/example/./TA/c1.ecl"
             ts="1543853101000000">
   export c1 := &apos;TA.c1&apos;;&#10;&#10;
  </Attribute>
 </Module>
 <Module key="tb" name="TB">
  <Attribute key="c1"
             name="c1"
             sourcePath="/home/gavin/temp/example/./TB/c1.ecl"
             ts="1543853345000000">
   export c1 := &apos;TB.c1&apos;;&#10;&#10;
  </Attribute>
 </Module>
 <Module key="tc" name="TC">
  <Attribute key="c1"
             name="c1"
             sourcePath="/home/gavin/temp/example/./TC/c1.ecl"
             ts="1543853266000000">
   export c1 := &apos;TC.c1&apos;;&#10;&#10;
  </Attribute>
 </Module>
 <Module key="td" name="TD">
  <Attribute key="c1"
             name="c1"
             sourcePath="/home/gavin/temp/example/./TD/c1.ecl"
             ts="1543854354000000">
   export c1 := &apos;TD.c1&apos;;&#10;&#10;
  </Attribute>
 </Module>
 <Module key="tb.tb" name="TB.TB">
  <Attribute key="f1"
             name="f1"
             sourcePath="/home/gavin/temp/example/./TB/TB/f1.ecl"
             ts="1543853325000000">
   import TB;
export f1 := TB.c1;&#10;&#10;&#10;
  </Attribute>
 </Module>
 <Module key="tc.tc" name="TC.TC">
  <Attribute key="f1"
             name="f1"
             sourcePath="/home/gavin/temp/example/./TC/TC/f1.ecl"
             ts="1543854235000000">
   import $.TC;
export f1 := TC.c1;&#10;&#10;
  </Attribute>
  <Attribute key="f2"
             name="f2"
             sourcePath="/home/gavin/temp/example/./TC/TC/f2.ecl"
             ts="1543854243000000">
   import ^.TC;
export f2 := TC.c1;&#10;&#10;
  </Attribute>
  <Attribute key="f3"
             name="f3"
             sourcePath="/home/gavin/temp/example/./TC/TC/f3.ecl"
             ts="1543854310000000">
   import $.^.TC;
export f3 := TC.c1;&#10;&#10;
  </Attribute>
  <Attribute key="c1"
             name="c1"
             sourcePath="/home/gavin/temp/example/./TC/TC/c1.ecl"
             ts="1543853153000000">
   export c1 := &apos;TC.TC.c1&apos;;&#10;&#10;
  </Attribute>
 </Module>
 <Module key="tc.tc.tc" name="TC.TC.TC">
  <Attribute key="c1"
             name="c1"
             sourcePath="/home/gavin/temp/example/./TC/c1.ecl"
             ts="1543853266000000">
   export c1 := &apos;TC.c1&apos;;&#10;&#10;
  </Attribute>
 </Module>
 <Module key="td.x" name="TD.x"/>
 <Module key="td.x.td" name="TD.x.TD"/>
 <Module key="td.x.td.y" name="TD.x.TD.y">
  <Attribute key="f1"
             name="f1"
             sourcePath="/home/gavin/temp/example/./TD/x/TD/y/f1.ecl"
             ts="1543853448000000">
   import TD;
export f1 := TD.c1;&#10;&#10;
  </Attribute>
  <Attribute key="f2"
             name="f2"
             sourcePath="/home/gavin/temp/example/./TD/x/TD/y/f2.ecl"
             ts="1543859944000000">
   import ^.TD;
export f2 := TD.c1;&#10;&#10;
  </Attribute>
 </Module>
</Archive>
