#include "time_diagnostics.h"
#include "time_integration.h"

namespace mt2mp::game::time_diagnostics {
bool read(shared_time::Presentation& result, std::string& error) {
    return time_integration::instance().read_presentation(result,error);
}
}
