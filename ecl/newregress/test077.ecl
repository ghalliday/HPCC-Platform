r1 := RECORD
    UNSIGNED i;
END;
r := RECORD
    UNSIGNED id;
    r1 nested;
END;
DATASET([TRANSFORM(r, SELF := [])]);
