use IO;

config const filename = "stringCorrupterText.txt";

proc main {
  var infile = open(filename, ioMode.r);
  var reader = infile.reader(locking=false);
  var word: string;

  while reader.read(word) {
    writeln("returned from toAlpha: '", toAlpha(word), "'");
  }
}

proc toAlpha(s: string) {
  var stripchars: string;
  writeln("toAlpha starting argument: '", s, "'");

  for i in 0..<s.size {
    if !s[i].isAlpha() then
      stripchars += s[i];
  }

  writeln("toAlpha strip characters: '", stripchars, "'");
  writeln("toAlpha before strip: '", s, "'");
  var stripped = s.strip(stripchars);
  writeln("toAlpha after strip: '", stripped, "'");

  return stripped;
}
