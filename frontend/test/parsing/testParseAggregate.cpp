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

#include "test-parsing.h"

#include "chpl/parsing/Parser.h"
#include "chpl/framework/Context.h"
#include "chpl/uast/Class.h"
#include "chpl/uast/Comment.h"
#include "chpl/uast/Function.h"
#include "chpl/uast/Identifier.h"
#include "chpl/uast/Module.h"
#include "chpl/uast/Record.h"
#include "chpl/uast/StringLiteral.h"
#include "chpl/uast/Union.h"
#include "chpl/uast/Variable.h"

static BuilderResult parseAggregate(Parser* parser,
                                    const AggregateDecl*& agg,
                                    const char* testname,
                                    const char* prog) {
  ErrorGuard guard(parser->context());
  auto parseResult = parseStringAndReportErrors(parser, testname, prog);
  assert(!guard.realizeErrors());
  auto mod = parseResult.singleModule();
  assert(mod);

  // find the first AggregateDecl statement
  const AggregateDecl* decl = nullptr;
  for (auto stmt : mod->stmts()) {
    if (stmt->isAggregateDecl()) {
      decl = stmt->toAggregateDecl();
      break;
    }
  }

  assert(decl);
  agg = decl;
  return parseResult;
}

static void checkThisFormal(const Function* fn,
                            const char* receiverType,
                            Formal::Intent expectIntent,
                            bool isPrimary) {
  assert(fn->thisFormal() != nullptr);
  assert(fn->thisFormal() == fn->formal(0));
  auto formal = fn->thisFormal();
  if (isPrimary) {
    assert(formal->typeExpression() == nullptr);
  } else {
    assert(formal->typeExpression()->isIdentifier());
    assert(formal->typeExpression()->toIdentifier()->name() == receiverType);
  }
  assert(formal->initExpression() == nullptr);
  assert(formal->name() == "this");
  assert(formal->intent() == expectIntent);
}

static void test0(Parser* parser) {
  const AggregateDecl* agg = nullptr;
  auto res = parseAggregate(parser, agg, "test0.chpl",
                            "class C { }");
  auto cls = agg->toClass();
  assert(cls);
  assert(cls->name() == "C");
  assert(cls->inheritExpr(0) == nullptr);
  assert(cls->numDeclOrComments() == 0);
  assert(cls->linkage() == Decl::DEFAULT_LINKAGE);
  assert(!cls->linkageName());
}

static void test1(Parser* parser) {
  const AggregateDecl* agg = nullptr;
  auto res = parseAggregate(parser, agg, "test1.chpl",
                            "class C : P { }");
  auto cls = agg->toClass();
  assert(cls);
  assert(cls->name() == "C");
  assert(cls->inheritExpr(0));
  auto parentId = cls->inheritExpr(0)->toIdentifier();
  assert(parentId);
  assert(parentId->name() == "P");
  assert(cls->numDeclOrComments() == 0);
  assert(cls->linkage() == Decl::DEFAULT_LINKAGE);
  assert(!cls->linkageName());
}

static void test2(Parser* parser) {
  const AggregateDecl* agg = nullptr;
  auto res = parseAggregate(parser, agg, "test2.chpl",
                            "class C { var x; }");
  auto cls = agg->toClass();
  assert(cls);
  assert(cls->name() == "C");
  assert(cls->inheritExpr(0) == nullptr);
  assert(cls->numDeclOrComments() == 1);
  assert(cls->linkage() == Decl::DEFAULT_LINKAGE);
  assert(!cls->linkageName());
  auto var = cls->declOrComment(0)->toVariable();
  assert(var);
  assert(var->name() == "x");
  assert(var->isField());
}

static void test3(Parser* parser) {
  const AggregateDecl* agg = nullptr;
  auto res = parseAggregate(parser, agg, "test3.chpl",
                            "class C { proc method() { } }");
  auto cls = agg->toClass();
  assert(cls);
  assert(cls->name() == "C");
  assert(cls->inheritExpr(0) == nullptr);
  assert(cls->numDeclOrComments() == 1);
  assert(cls->linkage() == Decl::DEFAULT_LINKAGE);
  assert(!cls->linkageName());
  auto mtd = cls->declOrComment(0)->toFunction();
  assert(mtd);
  assert(mtd->name() == "method");
  assert(mtd->thisFormal());
  assert(mtd->isPrimaryMethod());
  checkThisFormal(mtd, "C", Formal::DEFAULT_INTENT, /* isPrimary */ true);
}

static void test4(Parser* parser) {
  const AggregateDecl* agg = nullptr;
  auto res = parseAggregate(parser, agg, "test4.chpl",
                            "record R { }");
  auto rec = agg->toRecord();
  assert(rec);
  assert(rec->name() == "R");
  assert(rec->numDeclOrComments() == 0);
  assert(rec->linkage() == Decl::DEFAULT_LINKAGE);
  assert(!rec->linkageName());
}

static void test5(Parser* parser) {
  const AggregateDecl* agg = nullptr;
  auto res = parseAggregate(parser, agg, "test5.chpl",
                            "union U { }");
  auto uni = agg->toUnion();
  assert(uni);
  assert(uni->name() == "U");
  assert(uni->numDeclOrComments() == 0);
  assert(uni->linkage() == Decl::DEFAULT_LINKAGE);
  assert(!uni->linkageName());
}

static void test6(Parser* parser) {
  const AggregateDecl* agg = nullptr;
  auto res = parseAggregate(parser, agg, "test6.chpl",
                            "record R { var x; proc method() { } }");
  auto rec = agg->toRecord();
  assert(rec);
  assert(rec->name() == "R");
  assert(rec->numDeclOrComments() == 2);
  assert(rec->linkage() == Decl::DEFAULT_LINKAGE);
  assert(!rec->linkageName());
  auto var = rec->declOrComment(0)->toVariable();
  assert(var);
  assert(var->name() == "x");
  assert(var->isField());
  auto mtd = rec->declOrComment(1)->toFunction();
  assert(mtd);
  assert(mtd->name() == "method");
  assert(mtd->thisFormal());
  assert(mtd->isPrimaryMethod());
  checkThisFormal(mtd, "R", Formal::DEFAULT_INTENT, /* isPrimary */ true);
}

static void test7(Parser* parser) {
  const AggregateDecl* agg = nullptr;
  auto res = parseAggregate(parser, agg, "test7.chpl",
                            "union U { var x; proc method() { } }");
  auto uni = agg->toUnion();
  assert(uni);
  assert(uni->name() == "U");
  assert(uni->numDeclOrComments() == 2);
  assert(uni->linkage() == Decl::DEFAULT_LINKAGE);
  assert(!uni->linkageName());
  auto var = uni->declOrComment(0)->toVariable();
  assert(var);
  assert(var->name() == "x");
  assert(var->isField());
  auto mtd = uni->declOrComment(1)->toFunction();
  assert(mtd);
  assert(mtd->name() == "method");
  assert(mtd->thisFormal());
  assert(mtd->isPrimaryMethod());
  checkThisFormal(mtd, "U", Formal::DEFAULT_INTENT, /* isPrimary */ true);
}

static void test8(Parser* parser) {
  const AggregateDecl* agg = nullptr;
  auto res = parseAggregate(parser, agg, "test8.chpl",
                            "/*1*/\n"
                            "class /*2*/ C /*3*/ : /*4*/ P /*5*/ {\n"
                            "  /*6*/ var /*7*/ x /*8*/;\n"
                            "  /*9*/ proc /*A*/ foo /*B*/ (/*C*/) /*D*/ {\n"
                            "    /*E*/"
                            "  }\n"
                            "  /*F*/\n"
                            "}\n"
                            "/*G*/\n");
  auto mod = res.singleModule();
  assert(mod);
  assert(mod->numStmts() == 3);
  assert(mod->stmt(0)->isComment());
  assert(mod->stmt(1)->isClass());
  assert(mod->stmt(2)->isComment());

  auto cls = agg->toClass();
  assert(cls);
  assert(cls->name() == "C");
  auto parentId = cls->inheritExpr(0)->toIdentifier();
  assert(parentId);
  assert(parentId->name() == "P");
  assert(cls->numDeclOrComments() == 5);
  assert(cls->declOrComment(0)->isComment());
  assert(cls->declOrComment(1)->isVariable());
  assert(cls->declOrComment(2)->isComment());
  assert(cls->declOrComment(3)->isFunction());
  assert(cls->declOrComment(4)->isComment());
  assert(cls->linkage() == Decl::DEFAULT_LINKAGE);
  assert(!cls->linkageName());

  auto var = cls->declOrComment(1)->toVariable();
  assert(var);
  assert(var->name() == "x");
  assert(var->isField());
  auto mtd = cls->declOrComment(3)->toFunction();
  assert(mtd);
  assert(mtd->name() == "foo");
  assert(mtd->thisFormal());
  assert(mtd->isPrimaryMethod());
  checkThisFormal(mtd, "C", Formal::DEFAULT_INTENT, /* isPrimary */ true);
}

static void test9(Parser* parser) {
  const AggregateDecl* agg = nullptr;
  auto res = parseAggregate(parser, agg, "test9.chpl",
                            "record R {\n"
                            "  proc df(arg) { }\n"
                            "  proc const cnst(arg) const { }\n"
                            "  proc const ref cnstrf(arg) const ref { }\n"
                            "  proc ref rf(arg) ref { }\n"
                            "  proc param prm(arg) param { }\n"
                            "  proc type tp(arg) type { }\n"
                            "  proc ou(arg) out { }\n"
                            "}\n"
                            "proc R.df2(arg) { }\n"
                            "proc const R.cnst2(arg) const { }\n"
                            "proc const ref R.cnstrf2(arg) const ref { }\n"
                            "proc ref R.rf2(arg) ref { }\n"
                            "proc param R.prm2(arg) param { }\n"
                            "proc type R.tp2(arg) type { }\n"
                            "proc R.ou2(arg) out { }\n"
                            );

  auto rec = agg->toRecord();
  assert(rec);
  assert(rec->name() == "R");
  assert(rec->numDeclOrComments() == 7);
  assert(rec->linkage() == Decl::DEFAULT_LINKAGE);
  assert(!rec->linkageName());
  assert(rec->declOrComment(0)->isFunction());
  assert(rec->declOrComment(1)->isFunction());
  assert(rec->declOrComment(2)->isFunction());
  assert(rec->declOrComment(3)->isFunction());
  assert(rec->declOrComment(4)->isFunction());
  assert(rec->declOrComment(5)->isFunction());
  assert(rec->declOrComment(6)->isFunction());

  {
    auto df = rec->declOrComment(0)->toFunction();
    auto cnst = rec->declOrComment(1)->toFunction();
    auto cnstrf = rec->declOrComment(2)->toFunction();
    auto rf = rec->declOrComment(3)->toFunction();
    auto prm = rec->declOrComment(4)->toFunction();
    auto tp = rec->declOrComment(5)->toFunction();
    auto ou = rec->declOrComment(6)->toFunction();

    assert(df->name() == "df");
    assert(df->returnIntent() == Function::DEFAULT_RETURN_INTENT);
    assert(df->thisFormal());
    assert(df->isPrimaryMethod());
    checkThisFormal(df, "R", Formal::DEFAULT_INTENT, /* isPrimary */ true);
    assert(df->numFormals() == 2);

    assert(cnst->name() == "cnst");
    assert(cnst->returnIntent() == Function::CONST);
    assert(cnst->thisFormal());
    assert(cnst->isPrimaryMethod());
    checkThisFormal(cnst, "R", Formal::CONST, /* isPrimary */ true);
    assert(cnst->numFormals() == 2);

    assert(cnstrf->name() == "cnstrf");
    assert(cnstrf->returnIntent() == Function::CONST_REF);
    assert(cnstrf->thisFormal());
    assert(cnstrf->isPrimaryMethod());
    checkThisFormal(cnstrf, "R", Formal::CONST_REF, /* isPrimary */ true);
    assert(cnstrf->numFormals() == 2);

    assert(rf->name() == "rf");
    assert(rf->returnIntent() == Function::REF);
    assert(rf->thisFormal());
    assert(rf->isPrimaryMethod());
    checkThisFormal(rf, "R", Formal::REF, /* isPrimary */ true);
    assert(rf->numFormals() == 2);

    assert(prm->name() == "prm");
    assert(prm->returnIntent() == Function::PARAM);
    assert(prm->thisFormal());
    assert(prm->isPrimaryMethod());
    checkThisFormal(prm, "R", Formal::PARAM, /* isPrimary */ true);
    assert(prm->numFormals() == 2);

    assert(tp->name() == "tp");
    assert(tp->returnIntent() == Function::TYPE);
    assert(tp->thisFormal());
    assert(tp->isPrimaryMethod());
    checkThisFormal(tp, "R", Formal::TYPE, /* isPrimary */ true);
    assert(tp->numFormals() == 2);

    assert(ou->name() == "ou");
    assert(ou->returnIntent() == Function::OUT);
    assert(ou->thisFormal());
    assert(ou->isPrimaryMethod());
    checkThisFormal(ou, "R", Formal::DEFAULT_INTENT, /* isPrimary */ true);
    assert(ou->numFormals() == 2);
  }

  auto mod = res.singleModule();
  assert(mod);
  assert(mod->stmt(0)->isRecord());
  assert(mod->stmt(1)->isFunction());
  assert(mod->stmt(2)->isFunction());
  assert(mod->stmt(3)->isFunction());
  assert(mod->stmt(4)->isFunction());
  assert(mod->stmt(5)->isFunction());
  assert(mod->stmt(6)->isFunction());
  assert(mod->stmt(7)->isFunction());

  {
    auto df2 = mod->stmt(1)->toFunction();
    auto cnst2 = mod->stmt(2)->toFunction();
    auto cnstrf2 = mod->stmt(3)->toFunction();
    auto rf2 = mod->stmt(4)->toFunction();
    auto prm2 = mod->stmt(5)->toFunction();
    auto tp2 = mod->stmt(6)->toFunction();
    auto ou2 = mod->stmt(7)->toFunction();

    assert(df2->name() == "df2");
    assert(df2->returnIntent() == Function::DEFAULT_RETURN_INTENT);
    assert(df2->thisFormal());
    assert(df2->isPrimaryMethod() == false);
    checkThisFormal(df2, "R", Formal::DEFAULT_INTENT, /* isPrimary */ false);
    assert(df2->numFormals() == 2);

    assert(cnst2->name() == "cnst2");
    assert(cnst2->returnIntent() == Function::CONST);
    assert(cnst2->thisFormal());
    assert(cnst2->isPrimaryMethod() == false);
    checkThisFormal(cnst2, "R", Formal::CONST, /* isPrimary */ false);
    assert(cnst2->numFormals() == 2);

    assert(cnstrf2->name() == "cnstrf2");
    assert(cnstrf2->returnIntent() == Function::CONST_REF);
    assert(cnstrf2->thisFormal());
    assert(cnstrf2->isPrimaryMethod() == false);
    checkThisFormal(cnstrf2, "R", Formal::CONST_REF, /* isPrimary */ false);
    assert(cnstrf2->numFormals() == 2);

    assert(rf2->name() == "rf2");
    assert(rf2->returnIntent() == Function::REF);
    assert(rf2->thisFormal());
    assert(rf2->isPrimaryMethod() == false);
    checkThisFormal(rf2, "R", Formal::REF, /* isPrimary */ false);
    assert(rf2->numFormals() == 2);

    assert(prm2->name() == "prm2");
    assert(prm2->returnIntent() == Function::PARAM);
    assert(prm2->thisFormal());
    assert(prm2->isPrimaryMethod() == false);
    checkThisFormal(prm2, "R", Formal::PARAM, /* isPrimary */ false);
    assert(prm2->numFormals() == 2);

    assert(tp2->name() == "tp2");
    assert(tp2->returnIntent() == Function::TYPE);
    assert(tp2->thisFormal());
    assert(tp2->isPrimaryMethod() == false);
    checkThisFormal(tp2, "R", Formal::TYPE, /* isPrimary */ false);
    assert(tp2->numFormals() == 2);

    assert(ou2->name() == "ou2");
    assert(ou2->returnIntent() == Function::OUT);
    assert(ou2->thisFormal());
    assert(ou2->isPrimaryMethod() == false);
    checkThisFormal(ou2, "R", Formal::DEFAULT_INTENT, /* isPrimary */ false);
    assert(ou2->numFormals() == 2);
  }
}

static void test10(Parser* parser) {
  auto parseResult = parseStringAndReportErrors(parser, "test10.chpl",
                                         "/*1*/ class C1 {\n"
                                         "  /*1a*/ var a;\n"
                                         "  /*1aa*/ var aa;\n"
                                         "}\n"
                                         "/*2*/ class C2 { }\n"
                                         "/*3*/ record R3 {\n"
                                         "  /*3b*/ var b;\n"
                                         "  /*3bb*/ var bb;\n"
                                         "}\n"
                                         "/*4*/ record R4 { }\n"
                                         "/*5*/ union U5 {\n"
                                         "  /*5c*/ var c;\n"
                                         "  /*5cc*/ var cc;\n"
                                         "}\n"
                                         "/*6*/ union U6 { }\n");
  auto mod = parseResult.singleModule();
  assert(mod);
  assert(mod->numStmts() == 12);
  assert(mod->stmt(0)->isComment());
  assert(mod->stmt(1)->isClass());
  assert(mod->stmt(2)->isComment());
  assert(mod->stmt(3)->isClass());
  assert(mod->stmt(4)->isComment());
  assert(mod->stmt(5)->isRecord());
  assert(mod->stmt(6)->isComment());
  assert(mod->stmt(7)->isRecord());
  assert(mod->stmt(8)->isComment());
  assert(mod->stmt(9)->isUnion());
  assert(mod->stmt(10)->isComment());
  assert(mod->stmt(11)->isUnion());


  auto c1 = mod->stmt(1)->toClass();
  assert(c1->numDeclOrComments() == 4);
  assert(c1->declOrComment(0)->isComment());
  assert(c1->declOrComment(1)->isVariable());
  assert(c1->declOrComment(2)->isComment());
  assert(c1->declOrComment(3)->isVariable());
  assert(c1->linkage() == Decl::DEFAULT_LINKAGE);
  assert(!c1->linkageName());

  auto c2 = mod->stmt(3)->toClass();
  assert(c2->numDeclOrComments() == 0);
  assert(c2->linkage() == Decl::DEFAULT_LINKAGE);
  assert(!c2->linkageName());

  auto r3 = mod->stmt(5)->toRecord();
  assert(r3->numDeclOrComments() == 4);
  assert(r3->declOrComment(0)->isComment());
  assert(r3->declOrComment(1)->isVariable());
  assert(r3->declOrComment(2)->isComment());
  assert(r3->declOrComment(3)->isVariable());
  assert(r3->linkage() == Decl::DEFAULT_LINKAGE);
  assert(!r3->linkageName());

  auto r4 = mod->stmt(7)->toRecord();
  assert(r4->numDeclOrComments() == 0);
  assert(r4->linkage() == Decl::DEFAULT_LINKAGE);
  assert(!r4->linkageName());

  auto u5 = mod->stmt(9)->toUnion();
  assert(u5->numDeclOrComments() == 4);
  assert(u5->declOrComment(0)->isComment());
  assert(u5->declOrComment(1)->isVariable());
  assert(u5->declOrComment(2)->isComment());
  assert(u5->declOrComment(3)->isVariable());
  assert(u5->linkage() == Decl::DEFAULT_LINKAGE);
  assert(!u5->linkageName());

  auto u6 = mod->stmt(11)->toUnion();
  assert(u6->numDeclOrComments() == 0);
  assert(u6->linkage() == Decl::DEFAULT_LINKAGE);
  assert(!u6->linkageName());
}

static void test11(Parser* parser) {
  ErrorGuard guard(parser->context());
  auto parseResult = parseStringAndReportErrors(parser, "test11.chpl",
        "extern record foo {}\n"
        "extern \"struct bar\" record bar {}\n"
        "export record dog { var x = 0; }\n"
        "export \"meow\" record cat { var x = 0; }\n"
        "extern union baz {}\n"
        "extern \"union thing\" union thing {}\n");

  assert(!guard.realizeErrors());
  auto mod = parseResult.singleModule();
  assert(mod);
  assert(mod->numStmts() == 6);

  auto rec1 = mod->stmt(0)->toRecord();
  assert(rec1);
  assert(rec1->linkage() == Decl::Linkage::EXTERN);
  assert(!rec1->linkageName());
  assert(rec1->numDeclOrComments() == 0);

  auto rec2 = mod->stmt(1)->toRecord();
  assert(rec2);
  assert(rec2->linkage() == Decl::Linkage::EXTERN);
  assert(rec2->linkageName());
  assert(rec2->linkageName()->isStringLiteral());
  assert(rec2->linkageName()->toStringLiteral()->value() == "struct bar");
  assert(rec2->numDeclOrComments() == 0);

  auto rec3 = mod->stmt(2)->toRecord();
  assert(rec3);
  assert(rec3->linkage() == Decl::Linkage::EXPORT);
  assert(!rec3->linkageName());
  assert(rec3->numDeclOrComments() == 1);

  auto rec4 = mod->stmt(3)->toRecord();
  assert(rec4);
  assert(rec4->linkage() == Decl::Linkage::EXPORT);
  assert(rec4->linkageName());
  assert(rec4->linkageName()->isStringLiteral());
  assert(rec4->linkageName()->toStringLiteral()->value() == "meow");
  assert(rec4->numDeclOrComments() == 1);

  auto uni5 = mod->stmt(4)->toUnion();
  assert(uni5);
  assert(uni5->linkage() == Decl::Linkage::EXTERN);
  assert(!uni5->linkageName());
  assert(uni5->numDeclOrComments() == 0);

  auto uni6 = mod->stmt(5)->toUnion();
  assert(uni6);
  assert(uni6->linkage() == Decl::Linkage::EXTERN);
  assert(uni6->linkageName());
  assert(uni6->linkageName()->isStringLiteral());
  assert(uni6->linkageName()->toStringLiteral()->value() == "union thing");
  assert(uni6->numDeclOrComments() == 0);
}

// Test failure for exporting or externing a class.
// Test failure for exporting a union.
static void test12(Parser* parser) {
  ErrorGuard guard(parser->context());
  auto parseResult = parseStringAndReportErrors(parser, "test12.chpl",
        "extern class foo {};\n"
        "extern \"foo\" class foo {};\n"
        "export class bar { var x = 0; }\n"
        "export \"bar\" class bar { var x = 0; }\n"
        "export union baz {}\n"
        "export \"baz\" union baz {}\n");

  assert(guard.realizeErrors() == 6);
  auto mod = parseResult.singleModule();
  assert(mod);
}

static void test13(Parser* parser) {
  ErrorGuard guard(parser->context());
  std::string base = " foo /*comment*/ {}\n";
  for (auto keyword : {"class", "record", "union"}) {
    auto parseResult = parseStringAndReportErrors(parser, "test13.chpl",
          (std::string(keyword) + base).c_str());
    assert(guard.realizeErrors() == 0);
    auto mod = parseResult.singleModule();
    assert(mod);
    assert(mod->numStmts() == 1);
    assert(mod->stmt(0)->isAggregateDecl());
    assert(mod->stmt(0)->toAggregateDecl()->numDeclOrComments() == 1);
  }
}

int main() {
  Context context;
  Context* ctx = &context;

  auto parser = Parser::createForTopLevelModule(ctx);
  Parser* p = &parser;

  test0(p);
  test1(p);
  test2(p);
  test3(p);
  test4(p);
  test5(p);
  test6(p);
  test7(p);
  test8(p);
  test9(p);
  test10(p);
  test11(p);
  test12(p);
  test13(p);

  return 0;
}
