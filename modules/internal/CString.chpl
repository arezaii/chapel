/*
 * Copyright 2020-2023 Hewlett Packard Enterprise Development LP
 * Copyright 2004-2019 Cray Inc.
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// C strings
// extern type c_string; is a built-in primitive type
//
// In terms of how they are used, c_strings are a "close to the metal"
// representation, being in essence the common NUL-terminated C string.
module CString {
  private use ChapelStandard, CTypes;

  //inline proc c_string.c_str() return this;

  pragma "init copy fn"
  inline proc chpl__initCopy(x: c_ptrConst(c_uchar), definedConst: bool) : c_ptrConst(c_uchar) {
    return x;
  }

  pragma "auto copy fn"
  inline proc chpl__autoCopy(x: c_ptrConst(c_uchar), definedConst: bool) : c_ptrConst(c_uchar) {
    return x;
  }

  inline operator c_ptrConstUint8.==(s0: c_ptrConst(c_uchar), s1: c_ptrConst(c_uchar)) {
    return __primitive("string_compare", s0, s1) == 0;
  }

//  inline operator c_string.==(s0: string, s1: c_string) {
//    return __primitive("string_compare", s0.c_str(), s1) == 0;
//  }
//
//  inline operator ==(s0: c_string, s1: string) {
//    return __primitive("string_compare", s0, s1.c_str()) == 0;
//  }

  inline operator c_ptrConstUint8.!=(s0: c_ptrConst(c_uchar), s1: c_ptrConst(c_uchar)) {
    return __primitive("string_compare", s0, s1) != 0;
  }

//  inline operator c_string.!=(s0: string, s1: c_string) {
//    return __primitive("string_compare", s0.c_str(), s1) != 0;
//  }
//
//  inline operator !=(s0: c_string, s1: string) {
//    return __primitive("string_compare", s0, s1.c_str()) != 0;
//  }

  inline operator c_ptrConstUint8.<=(a: c_ptrConst(c_uchar), b: c_ptrConst(c_uchar)) {
    return (__primitive("string_compare", a, b) <= 0);
  }

  inline operator c_ptrConstUint8.>=(a: c_ptrConst(c_uchar), b: c_ptrConst(c_uchar)) {
    return (__primitive("string_compare", a, b) >= 0);
  }

  inline operator c_ptrConstUint8.<(a: c_ptrConst(c_uchar), b: c_ptrConst(c_uchar)) {
    return (__primitive("string_compare", a, b) < 0);
  }

  inline operator c_ptrConstUint8.>(a: c_ptrConst(c_uchar), b: c_ptrConst(c_uchar)) {
    return (__primitive("string_compare", a, b) > 0);
  }

  inline operator c_ptrConstUint8.=(ref a: c_ptrConst(c_uchar), b: c_ptrConst(c_uchar)) {
    __primitive("=", a, b);
  }

  // let us set c_strings to NULL
  inline operator c_ptrConstUint8.=(ref a:c_ptrConst(c_uchar), b:_nilType) { a = nil:c_ptrConst(c_uchar); }

  // for a to be a valid c_string after this function it must be on the same
  // locale as b
  inline operator c_ptrConstUint8.=(ref a: c_ptrConst(c_uchar), b: string) {
    __primitive("=", a, c_ptrToConst_helper(b));
  }

  //
  // casts from nil to c_string
  //
  inline operator :(x: _nilType, type t:c_ptrConst(c_uchar)) {
    return __primitive("cast", t, x);
  }

  //
  // casts from c_string to c_ptr(void)
  //
  inline operator :(x: c_ptrConst(c_uchar), type t:c_ptr(void)) {
    return __primitive("cast", t, x);
  }
  //
  // casts from c_ptr(void) to c_string
  //
  inline operator :(x: c_ptr(void), type t:c_ptrConst(c_uchar)) {
    return __primitive("cast", t, x);
  }

  //
  // casts from c_string to c_ptr(c_char/int(8)/uint(8))
  //
  inline operator :(x: c_ptrConst(c_uchar), type t:c_ptr(?eltType))
    where eltType == c_char || eltType == int(8) || eltType == uint(8)
  {
    return __primitive("cast", t, x);
  }
  //
  // casts from c_string to c_ptrConst(c_char/int(8)/uint(8))
  //
  inline operator :(x: c_ptrConst(c_uchar), type t:c_ptrConst(?eltType))
    where eltType == c_char || eltType == int(8) || eltType == uint(8)
  {
    return __primitive("cast", t, x);
  }
  //
  // casts from c_ptr(c_char/int(8)/uint(8)) to c_string
  //
  inline operator :(x: c_ptr(?eltType), type t:c_ptrConst(c_uchar))
    where eltType == c_char || eltType == int(8) || eltType == uint(8)
  {
    return __primitive("cast", t, x);
  }
  //
  // casts from c_ptrConst(c_char/int(8)/uint(8)) to c_string
  //
  inline operator :(x: c_ptrConst(?eltType), type t:c_ptrConst(c_uchar))
    where eltType == c_char || eltType == int(8) || eltType == uint(8)
  {
    return __primitive("cast", t, x);
  }

  //
  // casts from c_string to bool types
  //
  inline operator :(x:c_ptrConst(c_uchar), type t:chpl_anybool) throws {
    var chplString: string;
    try! {
      chplString = string.createCopyingBuffer(x);
    }
    return try (chplString.strip()): t;
  }

  //
  // casts from c_string to integer types
  //
  inline operator :(x:c_ptrConst(c_uchar), type t:integral) throws {
    var chplString: string;
    try! {
      chplString = string.createCopyingBuffer(x);
    }
    return try (chplString.strip()): t;
  }

  //
  // casts from c_string to real/imag types
  //
  inline operator :(x:c_ptrConst(c_uchar), type t:chpl_anyreal)  throws {
    var chplString: string;
    try! {
      chplString = string.createCopyingBuffer(x);
    }
    return try (chplString.strip()): t;
  }

  inline operator :(x:c_ptrConst(c_uchar), type t:chpl_anyimag) throws {
    var chplString: string;
    try! {
      chplString = string.createCopyingBuffer(x);
    }
    return try (chplString.strip()): t;
  }

  //
  // casts from c_string to complex types
  //
  inline operator :(x:c_ptrConst(c_uchar), type t:chpl_anycomplex)  throws {
    var chplString: string;
    try! {
      chplString = string.createCopyingBuffer(x);
    }
    return try (chplString.strip()): t;
  }

  //
  // primitive c_string functions and methods
  //

  inline proc c_ptrConstUint8.size do return __primitive("string_length_bytes", this);

  inline proc c_ptrConstUint8.substring(i: int) do
    return __primitive("string_index", this, i);

  inline proc c_ptrConstUint8.substring(r: range(?)) {
    var r2 = r[1..this.size];  // This may warn about ambiguously aligned ranges.
    var lo:int = r2.low, hi:int = r2.high;
    return __primitive("string_select", this, lo, hi, r2.stride);
  }

  pragma "last resort" // avoids param string to c_string coercion
  inline proc param c_ptrConstUint8.size param {
    return __primitive("string_length_bytes", this);
  }

  // TODO: Remove this if we don't really need it as c_string is being deprecated
  // and we don't have a param replacement for it
  // pragma "last resort" // avoids param string to c_string coercion
  // inline proc _string_contains(param a: c_string, param b: c_string) param do
  //   return __primitive("string_contains", a, b);

  /* Returns the index of the first occurrence of a substring within a string,
     or 0 if the substring is not in the string.
  */
  inline proc c_ptrConstUint8.indexOf(substring:c_ptrConst(c_uchar)):int do
    return string_index_of(this, substring);

  pragma "fn synchronization free"
  extern proc string_index_of(haystack:c_ptrConst(c_uchar), needle:c_ptrConst(c_uchar)):int;

  // Use with care.  Not for the weak.
  inline proc chpl_free_c_string(ref cs: c_ptrConst(c_uchar)) {
    pragma "fn synchronization free"
    pragma "insert line file info"
    extern proc chpl_rt_free_c_string(ref cs: c_ptrConst(c_uchar));
    if (cs != nil:c_ptrConst(c_uchar)) then chpl_rt_free_c_string(cs);
    // cs = nil;
  }

  proc c_ptrConstUint8.writeThis(x) throws {
    compilerError("Cannot write a c_ptrConstUint8, cast to a string first.");
  }
  proc c_ptrConstUint8.serialize(writer, ref serializer) throws {
    writeThis(writer);
  }

  proc c_ptrConstUint8.readThis(x) throws {
    compilerError("Cannot read a c_ptrConstUint8, use string.");
  }

}

