use CTypes;

extern proc fopen(name: c_ptrConst(c_char), mode: c_ptrConst(c_char)): c_ptr(c_FILE);
extern proc fread(ref data, size: int, n: int, f: c_ptr(c_FILE)): int;
extern proc fwrite(ref data, size: int, n: int, f: c_ptr(c_FILE)): int;
extern proc fclose(f: c_ptr(c_FILE));
extern proc sizeof(x): int;

var i = 1, j = 2.0;

writeln((i, j));

var f: c_ptr(c_FILE);
f = fopen("myfile.dat", "wb");
fwrite(i, sizeof(i), 1, f);
fwrite(j, sizeof(j), 1, f);
fclose(f);

var ii: int, jj: real;

f = fopen("myfile.dat", "rb");
fread(ii, sizeof(ii), 1, f);
fread(jj, sizeof(jj), 1, f);
fclose(f);

writeln((ii, jj));

var A: [1..5] int = (1, 2, 3, 4, 5);
writeln(A);

f = fopen("myfile.dat", "wb");
fwrite(A(1), sizeof(A(i)), 5, f);
fclose(f);

var AA: [1..5] int;

f = fopen("myfile.dat", "rb");
fread(AA(1), sizeof(A(i)), 5, f);
fclose(f);

writeln(AA);
