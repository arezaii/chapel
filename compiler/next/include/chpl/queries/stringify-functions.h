/*
 * Copyright 2021 Hewlett Packard Enterprise Development LP
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

/*
  This file implements the generic chpl::stringify
  as well as specializations for some common types.

  stringify can be used to get a string representation of an object
  for debugging purposes
 */

#ifndef CHPL_QUERIES_STRINGIFY_FUNCTIONS_H
#define CHPL_QUERIES_STRINGIFY_FUNCTIONS_H

#include <cassert>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "chpl/util/memory.h"
#include <set>
#include <sstream>


namespace chpl {
class Context;

enum StringifyKind {
  DEBUG_SUMMARY,
  DEBUG_DETAIL,
  CHPL_SYNTAX
};


template<typename T> struct stringify {
  std::string operator()(StringifyKind stringKind, const T& stringMe) const = 0;
};


template<typename T> struct stringify<const T*> {
  std::string operator()(StringifyKind stringKind, const T* stringMe) const {
    stringify<T> stringifier;
    return stringifier(stringKind, *stringMe);
  }
};

template<typename T>
static inline std::string defaultStringify(StringifyKind stringKind,
                                           const T& stringMe) {

  return "HAVE NOT IMPLEMENTED STRINGIFY YET";

}

template<typename T>
static inline std::string defaultStringifyVec(StringifyKind stringKind,
                                              const std::vector<T>& stringVec)
{
  if (stringVec.empty()) {
   return "[ ]";
  } else {
   std::ostringstream ss;
   std::string separator;
   stringify<T> vecString;
   for (const auto &vecVal : stringVec ) {
     ss << separator << vecString(stringKind, vecVal);
     separator = ",";
   }
   return std::string("["+ss.str()+"]");
  }
}

template<typename K, typename V>
static inline std::string defaultStringifyMap(StringifyKind stringKind,
                                              const std::unordered_map<K,V>& stringMap)
{
  if (stringMap.size() == 0) {
    return "{ }";
  } else {
    std::ostringstream ss;
    std::string separator;
    stringify<K> keyString;
    stringify<V> valString;
    for (auto const& x : stringMap)
    {
      ss << separator
         << keyString(stringKind, x.first)
         << " : "
         << valString(stringKind, x.second);

      separator = ",";
    }
    return std::string("{"+ss.str()+"}");
  }
}

template<typename A, typename B>
static inline std::string defaultStringifySetPairs(StringifyKind stringKind,
                                                   const std::set<std::pair<A,B>>& stringSet)
{
  if (stringSet.size() == 0) {
    return "{ }";
  } else {
    std::ostringstream ss;
    std::string separator;
    stringify<A> aString;
    stringify<B> bString;

    for (auto const& x : stringSet)
    {
      ss << separator
         << "("
         << aString(stringKind, x.first)
         << ", "
         << bString(stringKind, x.second)
         << ")";
      separator = ",";
    }
    return std::string("{"+ss.str()+"}");
  }
}

template<typename TUP, size_t... I>
static inline std::string defaultStringifyTuple(StringifyKind stringKind,
                                                const TUP& tuple,
                                                std::index_sequence<I...>) {
  // lambda to convert
  auto convert = [](bool printsep, auto& elem) {
    // we don't know what the type of `elem` is going to be, so we use
    // std::decay_t<decltype(x)> to get the type, so we can instantiate
    // the proper stringify struct
    chpl::stringify<std::decay_t<decltype(elem)>> stringifier;
    std::string ret = stringifier(StringifyKind::DEBUG_DETAIL, elem);
    if (printsep) {
      ret = ", " + ret;
    }
    return ret;
  };

  std::ostringstream ss;
  auto dummy = {(ss << convert(I!=0, std::get<I>(tuple)),0)...};
  (void) dummy; // avoid unused variable warning
  return std::string("("+ss.str()+")");
}

template<typename A, typename B>
static inline std::string defaultStringifyPair(StringifyKind stringKind,
                                               const std::pair<A,B>& stringPair)
{
 stringify<A> stringA;
 stringify<B> stringB;

 return std::string("(" + stringA(stringKind, stringPair[0])
                    + ", " + stringB(stringKind, stringPair[1])+")");
}

/// \cond DO_NOT_DOCUMENT
template<> struct stringify<std::string> {
 std::string operator()(StringifyKind stringKind,
                        const std::string& val) const {
   return std::string("\""+val+"\"");
 }
};


template<> struct stringify<int> {
 std::string operator()(StringifyKind stringKind, const int val) const {
   return std::to_string(val);
 }
};

template<> struct stringify<long int> {
std::string operator()(StringifyKind stringKind, const long int val) const {
  return std::to_string(val);
}
};

template<> struct stringify<double> {
std::string operator()(StringifyKind stringKind, const double val) const {
  return std::to_string(val);
}
};

template<> struct stringify<long unsigned int> {
std::string operator()(StringifyKind stringKind,
                       const long unsigned int val) const {
  return std::to_string(val);
}
};

template<> struct stringify<bool> {
 std::string operator()(StringifyKind stringKind, const bool val) const {
   if (val) {
     return "true";
   }
   return "false";
 }
};

template<typename T> struct stringify<std::vector<T>> {
 std::string operator()(StringifyKind stringKind,
                        const std::vector<T>& stringMe) const {
   return defaultStringifyVec(stringKind, stringMe);
 }
};

template<typename K, typename V> struct stringify<std::unordered_map<K,V>> {
 std::string operator()(StringifyKind stringKind,
                        const std::unordered_map<K,V>& stringMe) const {
   return defaultStringifyMap(stringKind, stringMe);
 }
};

template<typename A, typename B> struct stringify<std::pair<A,B>> {
 std::string operator()(StringifyKind stringKind,
                 const std::pair<A,B>& stringMe) const {
   return defaultStringifyPair(stringKind, stringMe);
 }
};

template<typename A, typename B> struct stringify<std::set<std::pair<A,B>>> {
std::string operator()(StringifyKind stringKind,
                       const std::set<std::pair<A,B>>& stringMe) const {

  return defaultStringifySetPairs(stringKind, stringMe);
}
};


template<typename... ArgTs> struct stringify<std::tuple<ArgTs...>> {
  std::string operator()(StringifyKind stringKind,
                         const std::tuple<ArgTs...>& stringMe) const {
    return defaultStringifyTuple(stringKind, stringMe,
                                 std::index_sequence_for<ArgTs...>{});
  }
};
/// \


} // end namespace chpl

#endif