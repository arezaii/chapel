extern proc chpl_cache_print();
config const verbose=false;

proc doit(a:locale, b:locale, c:locale)
{
  use CTypes;
  extern proc printf(fmt: c_ptrConst(c_char), vals...?numvals): int;

  on a {
    if verbose then printf("on %d\n", here.id:c_int);
    var x = 17;
    on b {
      var myx = x;
      if verbose then printf("on %d x = %d\n", here.id:c_int, myx:c_int);
      assert(myx == 17);
      x = 24;
      on c {
        var myx = x;
        if verbose then printf("on %d x = %d\n", here.id:c_int, myx:c_int);
        assert(myx == 24);
        x = 99;
        on b {
          var myx = x;
          if verbose then printf("on %d x = %d\n", here.id:c_int, myx:c_int);
          assert(myx == 99);
          x = 53;
          on a {
            var myx = x;
            if verbose then printf("on %d x = %d\n", here.id:c_int, myx:c_int);
            assert(myx == 53);
            x = 12;
          }
          assert(x == 12);
        }
        assert(x == 12);
      }
      assert(x == 12);
    }
    assert(x == 12);
  }
}

doit(Locales[1], Locales[0], Locales[2]);
doit(Locales[1], Locales[2], Locales[0]);
doit(Locales[0], Locales[1], Locales[2]);
doit(Locales[0], Locales[2], Locales[1]);
doit(Locales[2], Locales[0], Locales[1]);
doit(Locales[2], Locales[1], Locales[0]);

