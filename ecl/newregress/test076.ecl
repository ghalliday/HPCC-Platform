r1 := RECORD
    UNSIGNED i;
END;
r := {
    UNSIGNED id;
    UNSIGNED cnt;
    DATASET(r1, COUNT(SELF.cnt)) ds;
};
DATASET([TRANSFORM(r, SELF := [])]);
