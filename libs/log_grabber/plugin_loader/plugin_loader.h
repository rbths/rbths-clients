#include "log_grabber.h"
#include <string>
namespace rbths {
namespace log_grabber {
namespace plugin_loader {
LogIteratorGenerator getLogIteratorGenerator(const std::string& name, const std::string& path = "/usr/lib/rbths/log_grabber/plugins/log_iterators/");
}  // namespace plugin_loader
}  // namespace log_grabber
}  // namespace rbths
