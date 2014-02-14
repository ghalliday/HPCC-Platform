myType := decimal8_2;
r := RECORD
    myType i;
END;
ROW(TRANSFORM(r, SELF := []));
