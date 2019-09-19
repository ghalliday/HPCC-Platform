import Std;

rRecord := record
    string f1;
    string f2;
end;

dRecord := dataset([{'a','b'},{'a','a'},{'e','e'},{'f','g'}],rRecord);

apply(dRecord,
          if (f1=f2,
               output(dataset([{f1 + ' matches'}],{string myfield}),,named('msgs'),extend),
               output(dataset([{f1 + ' mismatches ' + f2}],{string myfield}),,named('msgs'),extend)
          )
      );

