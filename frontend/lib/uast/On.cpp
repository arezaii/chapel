/*
 * Copyright 2021-2025 Hewlett Packard Enterprise Development LP
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

#include "chpl/uast/On.h"

#include "chpl/uast/Builder.h"

namespace chpl {
namespace uast {


std::string On::dumpChildLabelInner(int i) const {
  if (i == destChildNum_) {
    return "dest";
  }
  return SimpleBlockLike::dumpChildLabelInner(i);
}

owned<On> On::build(Builder* builder, Location loc,
                    owned<AstNode> destination,
                    BlockStyle blockStyle,
                    AstList stmts) {
  CHPL_ASSERT(destination.get() != nullptr);

  AstList lst;

  lst.push_back(std::move(destination));

  const int bodyChildNum = lst.size();
  const int numBodyStmts = stmts.size();

  for (auto& stmt : stmts) {
    lst.push_back(std::move(stmt));
  }

  On* ret = new On(std::move(lst), blockStyle, bodyChildNum, numBodyStmts);
  builder->noteLocation(ret, loc);
  return toOwned(ret);
}


} // namespace uast
} // namespace chpl
