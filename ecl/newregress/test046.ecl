r := RECORD
    UNSIGNED i;
    STRING j;
END;
ROW(TRANSFORM(r, SELF := []));
