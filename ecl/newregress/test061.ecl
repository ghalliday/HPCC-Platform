JPEG(INTEGER len) := TYPE
            EXPORT DATA LOAD(DATA D) := D[1..len];
            EXPORT DATA STORE(DATA D) := D[1..len];
            EXPORT INTEGER PHYSICALLENGTH(DATA D) := len;
END;

r := RECORD
            STRING ID;
            UNSIGNED2 LEN;
            JPEG(SELF.LEN) PHOTO;
END;
ROW(TRANSFORM(r, SELF := []));
