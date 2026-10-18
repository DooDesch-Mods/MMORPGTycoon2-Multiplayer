#pragma once
#include "lobby_presentation.h"
namespace mt2mp::game::lobby_progress {
void tick(bool member,bool host,lobby_presentation::Phase phase);
lobby_presentation::Phase remote_phase();
}
