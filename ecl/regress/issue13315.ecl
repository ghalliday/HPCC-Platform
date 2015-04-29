#option ('pickBestEngine', true);

input := '0!1!2|3!4!5|5!6!7|8!>!9|';

Rec := RECORD
   INTEGER a;
   INTEGER b;
   REAL c;
END;

ds := PIPE('echo ' + input, Rec, CSV(SEPARATOR('!'),TERMINATOR('|')));

CHOOSEN(ds, 100);
