



IMPORT Std.Uni;

EXPORT TestExcludeNthWord := MODULE

  EXPORT TestConst := MODULE
    //Check action on a string with no entries.
    EXPORT Test01 := ASSERT(Uni.ExcludeNthWord(U'',0)+U'!' = U'!', CONST);
    EXPORT Test02 := ASSERT(Uni.ExcludeNthWord(U'',1)+U'!' = U'!', CONST);
    EXPORT Test03 := ASSERT(Uni.ExcludeNthWord(U'',-1)+U'!' = U'!', CONST);
    EXPORT Test04 := ASSERT(Uni.ExcludeNthWord(U'             ',0)+U'!' = U'!', CONST);
    EXPORT Test05 := ASSERT(Uni.ExcludeNthWord(U'             ',1)+U'!' = U'!', CONST);
    EXPORT Test06 := ASSERT(Uni.ExcludeNthWord(U'             ',-1)+U'!' = U'!', CONST);
    //Check action on a string containing a single word - with various whitespace
    EXPORT Test07 := ASSERT(Uni.ExcludeNthWord(U'x',0)+U'!' = U'x!');
    EXPORT Test08 := ASSERT(Uni.ExcludeNthWord(U'x',1)+U'!' = U'!');
    EXPORT Test09 := ASSERT(Uni.ExcludeNthWord(U'x',2)+U'!' = U'x!');
    EXPORT Test10 := ASSERT(Uni.ExcludeNthWord(U'x',3)+U'!' = U'x!');
    EXPORT Test11 := ASSERT(Uni.ExcludeNthWord(U' x',1)+U'!' = U'!');
    EXPORT Test12 := ASSERT(Uni.ExcludeNthWord(U'x ',1)+U'!' = U'!');
    EXPORT Test13 := ASSERT(Uni.ExcludeNthWord(U' x',2)+U'!' = U' x!');
    EXPORT Test14 := ASSERT(Uni.ExcludeNthWord(U' x ',1)+U'!' = U'!');
    //Check action on a string containg multiple words - with various whitespace combinations.
    EXPORT Test15 := ASSERT(Uni.ExcludeNthWord(U' abc def ', 1)+U'!' = U'def !');
    EXPORT Test16 := ASSERT(Uni.ExcludeNthWord(U' abc def ', 2)+U'!' = U' abc !');
    EXPORT Test17 := ASSERT(Uni.ExcludeNthWord(U'  a b c   def    ',0)+U'!' = U'  a b c   def    !');
    EXPORT Test18 := ASSERT(Uni.ExcludeNthWord(U'  a b c   def    ',1)+U'!' = U'b c   def    !');
    EXPORT Test19 := ASSERT(Uni.ExcludeNthWord(U'  a b c   def    ',2)+U'!' = U'  a c   def    !');
    EXPORT Test20 := ASSERT(Uni.ExcludeNthWord(U'  a b c   def    ',3)+U'!' = U'  a b def    !');
    EXPORT Test21 := ASSERT(Uni.ExcludeNthWord(U'  a b c   def    ',4)+U'!' = U'  a b c   !');
    EXPORT Test22 := ASSERT(Uni.ExcludeNthWord(U'  a b c   def    ',5)+U'!' = U'  a b c   def    !');
    EXPORT Test23 := ASSERT(Uni.ExcludeNthWord(U' ,,,, ',1)+U'!' = U'!');
    //Test other space characters (< 0x20)
    EXPORT Test24 := ASSERT(Uni.ExcludeNthWord(U'  a b\nc \t  def    ',2)+U'!' = U'  a c \t  def    !');
    EXPORT Test25 := ASSERT(Uni.ExcludeNthWord(U'  a b\nc \t  def    ',3)+U'!' = U'  a b\ndef    !');
  END;

END;
