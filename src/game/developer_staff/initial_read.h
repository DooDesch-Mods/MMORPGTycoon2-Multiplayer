#pragma once
#include "read.h"
namespace mt2mp::game::developer_staff {
// Loaded construction is the sole process caller. All action reads remain strict.
bool read_process_loaded(const std::string&,const action_session::Binding&,Capture&,std::string&);
namespace initial_detail {
// Native Mode.Update establishes the employment prefix after the certified load.
// The process caller must hold the live zero-step Developer bootstrap agreement.
bool read(Port&,Capture&,std::string&);
}
}
