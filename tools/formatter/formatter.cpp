// get some files from the command line
#include <iostream>
#include <fstream>

#include "chpl/uast/chpl-syntax-printer.h"
#include "chpl/parsing/Parser.h"
#include "chpl/uast/Builder.h"
#include "chpl/parsing/parsing-queries.h"
#include "chpl/framework/UniqueString.h"

using namespace chpl;
using namespace uast;
using namespace parsing;
using namespace std;

// some simple arguments
struct Args {
  bool writeToStdOut = false;
  std::string outputDir;
  std::vector<std::string> files;
};

// parse the command line arguments
// expect 1..n  *.chpl sources and some optional flags
static Args parseArgs(int argc, char** argv) {
  Args ret;
  for (int i = 1; i < argc; i++) {
    if  (std::strcmp("--stdout", argv[i]) == 0){
      ret.writeToStdOut = true;
    } else if (std::strcmp("--outdir", argv[i]) == 0) {
      assert(i < (argc - 1));
      ret.outputDir = argv[i + 1];
      i += 1;
    } else {
      ret.files.push_back(argv[i]);
    }
  }
  return ret;
}


/* The formatter reads in a list of files and outputs them to stdout
 * or to a directory. The output is the same as the input except
 * that the AST is printed out without any comments or empty lines.
 * Any discrepancies that affect the program output should be reported
 * at github.com/chapel-lang/chapel/issues.
 */
int main(int argc, char** argv) {
  Context context;
  Context* ctx = &context;
  auto parser = Parser::createForTopLevelModule(ctx);
  Parser* p = &parser;

  Args args = parseArgs(argc, argv);
  for (int i = 0; i < args.files.size(); i++) {
    // parse the file, use an empty parent as the last argument to parseFileToBuilderResult
    auto& builderResult = parseFileToBuilderResult(ctx,
                                                 UniqueString::get(ctx, args.files[i]),
                                                 UniqueString());

    // check for errors in the builderResult
    if (builderResult.numErrors() > 0) {
      bool fatal = false;
      for (auto e : builderResult.errors()) {
      // just display the error messages right now, see TODO above
        if (e->kind() == ErrorBase::Kind::ERROR ||
            e->kind() == ErrorBase::Kind::SYNTAX) {
              fatal = true;
        }
        context.report(e);
      }
      if (fatal) {
        return 1;
      }
    }

    // get the module that resulted from parsing the input file
    auto mod = builderResult.singleModule();
    assert(mod);

    std::cout << "parsed " << args.files[i] << std::endl;

    // write to stdout if that was requested
    if (args.writeToStdOut) {
      std::cout << "writing to stdout" << std::endl;
      mod->stringify(std::cout, CHPL_SYNTAX);
      std::cout << std::endl;
    } else { // write to a file
      ofstream myfile;
      std::string outFileName;
      std::string end = ".chpl";
      if (end.size() < args.files[i].size() &&
          std::equal(end.rbegin(), end.rend(), args.files[i].rbegin())) {
        // trim the .chpl off the filename
       outFileName = args.files[i].substr(0, args.files[i].length() - 5);
      } else {
        // the filename didn't end in .chpl?
        std::cout << "unexpected file name did not end in .chpl" << args.files[i] << std::endl;
        return 1;
      }
      // if we didn't get an output directory, use the same directory as the input file
      if (args.outputDir.empty()) {
        outFileName = std::string(outFileName + std::string(".clean.chpl"));
      } else {
        // try to make a filename from the module name and output directory
        outFileName = args.outputDir + "/" + mod->name().c_str() + ".clean.chpl";
      }
      std::cout << "writing to " << outFileName << std::endl;
      // write and close the file
      myfile.open(outFileName);
      mod->stringify(myfile, CHPL_SYNTAX);
      myfile.close();
    }
  }
  return 0;
}
