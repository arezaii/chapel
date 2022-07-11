
#include <cstring>
#include <iostream>
#include <fstream>
#include <filesystem>

#include "chpl/parsing/parsing-queries.h"


using namespace chpl;
using namespace uast;
using namespace parsing;


int main(int argc, char** argv) {
  std::vector<std::string> files;
  Context context;
  Context* ctx = &context;

  for (int i = 1; i < argc; i++) {
    files.push_back(argv[i]);
  }

  for (auto cpath: files) {
    UniqueString path = UniqueString::get(ctx, cpath);
    UniqueString emptyParent;
    const BuilderResult& builderResult = parseFileToBuilderResult(ctx,
                                                                  path,
                                                                  emptyParent);
    if (builderResult.numErrors() > 0) {
      for (auto e: builderResult.errors()) {
        if (e.kind() == ErrorMessage::Kind::ERROR ||
            e.kind() == ErrorMessage::Kind::SYNTAX) {
          std::cerr << "Error parsing " << path << ": "
                    << builderResult.error(0).message() << "\n";
          return 1;
        }
      }
    }
    for (const auto& ast: builderResult.topLevelExpressions()) {
      std::ostringstream ss;
      auto outpath = cpath;

      std::ofstream ofs = std::ofstream(outpath, std::ios::out);
      ast->stringify(ofs, CHPL_SYNTAX);
    }
  }
  return 0;
}
