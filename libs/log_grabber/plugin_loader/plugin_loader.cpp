#include "plugin_loader.h"

#include <boost/dll/import.hpp>  // for import_alias
#include <boost/filesystem.hpp>
#include <iostream>
using namespace rbths::log_grabber;
using namespace rbths::log_grabber::plugin_loader;

typedef const char*(LogIteratorNamePtr)();

LogIteratorGenerator try_load_iterator(const std::string& path_so,
                                       const std::string& name) {
  try {
    auto names =
        boost::dll::import_alias<LogIteratorNamePtr>(  // type of imported
                                                       // symbol must be
                                                       // explicitly specified
            path_so,                                   // path to library
            "name_logiterator",                        // symbol to import
            boost::dll::load_mode::append_decorations  // do append extensions
                                                       // and prefixes
        );
    if (names() == name) {
      return boost::dll::import_alias<
          LogIteratorGeneratorPtr>(  // type of imported symbol must be
                                     // explicitly specified
          path_so,                   // path to library
          "create_logiterator",      // symbol to import
          boost::dll::load_mode::append_decorations  // do append extensions and
                                                     // prefixes
      );
    }
  } catch (const std::exception& e) {
    std::cout << "Error loading " << path_so << ": " << e.what() << std::endl;
    return nullptr;
  }
}

LogIteratorGenerator rbths::log_grabber::plugin_loader::getLogIteratorGenerator(
    const std::string& name,
    const std::string& path) {
#ifndef RBTHS_RELEASE_BUILD
  try {
    for (auto& p : boost::filesystem::directory_iterator(
             "libs/log_grabber/plugins/log_iterators/")) {
      if (!boost::filesystem::is_directory(p.path()))
        continue;
      for (auto& p2 : boost::filesystem::directory_iterator(p.path())) {
        if (p2.path().filename().extension() == ".so") {
          auto ret = try_load_iterator(p2.path().string(), name);
          if (ret != nullptr) {
            return ret;
          }
        }
      }
    }
  } catch (const std::exception& e) {
    std::cout << "Error loading default path: " << e.what() << std::endl;
  }

#endif
  try {
    for (auto& p : boost::filesystem::directory_iterator(path)) {
      if (p.path().filename().extension() == ".so") {
        auto ret = try_load_iterator(p.path().string(), name);
        if (ret != nullptr) {
          return ret;
        }
      }
    }
  } catch (const std::exception& e) {
    std::cout << "Error loading " << path << ": " << e.what() << std::endl;
  }


  return nullptr;
}
