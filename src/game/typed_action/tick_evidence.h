#pragma once
#include "tick_execution.h"
#include "mt2mp/core/hash.h"

namespace mt2mp::action_session::tick_commands {
inline bool canonical_evidence(const Context& context,const Assigned& command,const ExecutionReceipt& receipt,std::string& digest,std::string& error){
    digest.clear();
    if(command.input.session!=context.session||command.input.snapshot_uuid!=context.snapshot_uuid||
       !requires_evidence(command.input)||receipt.command!=command||receipt.outcome!=Outcome::Confirmed||!receipt.evidence){
        error="Canonical evidence requires the exact confirmed command execution";return false;
    }
    auto message=std::make_unique<Message>();message->kind=Kind::HostResult;
    message->session=context.session;message->revision=command.order;message->required_mask=context.peer_mask;
    message->request=std::get<Request>(command.input.command);message->outcome=Outcome::Confirmed;message->evidence=*receipt.evidence;
    if(message->request.action==Action::EditCharacterCostume){const auto& e=message->evidence;
        if(!ordered_costume_edit(message->request.costume_edit)||!costume_edit_transition(message->request.costume_edit,e.costume_edit_before,e.costume_edit_after)||e.before!=e.after){error="Ordered Costume canonical original weapon proof/Ledger differs";return false;}}
    if(message->request.action==Action::EditCharacterAbility){const auto& e=message->evidence;
        if(!ordered_ability_edit(message->request.ability_edit)||!ability_edit_transition(message->request.ability_edit,e.ability_edit_before,e.ability_edit_after)||e.before!=e.after){error="Ordered Ability canonical original Apply/adjustment proof/Ledger differs";return false;}}
    if(message->request.action==Action::GeneralProject){const auto& e=message->evidence;std::string cause;
        if(!receipt.native_attempted||!general_project_valid(message->request.general_project)||!e.general_project_before||!e.general_project_after||
           !general_project_valid(*e.general_project_before)||!general_project_valid(*e.general_project_after)||
           !game::general_project::effect(message->request.general_project,*e.general_project_before,*e.general_project_after,cause)||
           (message->request.general_project.command!=game::general_project::Command::Start&&e.before!=e.after)||
           !e.retirement_confirmed||e.target_retired||e.undo_confirmed||e.selection_confirmed){error="Ordered GeneralProject canonical complete source/proof/Ledger differs"+(cause.empty()?std::string{}:": "+cause);return false;}}
    if(message->request.action==Action::GmPolicy){const auto& e=message->evidence;if(!ordered_gm_policy(message->request.gm_policy)||!e.gm_policy_before||!e.gm_policy_after||!game::gm_policy::ordered(*e.gm_policy_before)||!game::gm_policy::ordered(*e.gm_policy_after)||!game::gm_policy::effect(message->request.gm_policy,*e.gm_policy_before,*e.gm_policy_after)||e.before!=e.after){error="Ordered GM policy canonical cascade/protected RNG evidence absent or differs";return false;}}
    if(message->request.action==Action::GameRules){const auto& e=message->evidence;if(!ordered_game_rules(message->request.game_rules)||!e.game_rules_before||!e.game_rules_after||!game::game_rules::digest(e.game_rules_before->protected_sha256)||!game::game_rules::effect(message->request.game_rules,*e.game_rules_before,*e.game_rules_after)){error="Ordered Design canonical protected rules/RNG evidence absent or differs";return false;}}
    if(message->request.action==Action::ManagementCommand){Ledger expected;const auto& e=message->evidence;if(!e.management_before||!e.management_after||!ordered_management_ledger(message->request.management,*e.management_before,e.before,expected)||expected!=e.after||!ordered_campaign_effect(message->request.management,*e.management_before,*e.management_after,e.before)){error="Ordered Management canonical Ledger or Campaign differs";return false;}}
    if(message->request.action==Action::ManagementCommand&&message->request.management.op==ManagementOp::SetServicePrice){const auto& e=message->evidence;
        if(!e.management_before->services||!e.management_after->services||!e.management_before->services->price||!e.management_after->services->price){error="Ordered ServicePrice canonical graph/RNG evidence absent";return false;}}
    std::vector<uint8_t> bytes;
    if(!action_session::encode(*message,bytes,error))return false;
    // The existing strict codec owns canonical field order and domain checks.
    // Its round trip must retain the entire evidence, including unused fields.
    auto decoded=std::make_unique<Message>();
    if(!action_session::decode(bytes.data(),bytes.size(),*decoded,error))return false;
    if(*decoded!=*message){error="Canonical evidence codec omitted or changed a field";return false;}
    if(message->request.action==Action::MoveMonster||message->request.action==Action::RemoveMonster){
        // The codec above checks the complete local transition. The loaded
        // actor's unpersisted Spawn serial is not shared identity.
        message->evidence.monster_before.actor.tag=0;
        if(message->request.action==Action::MoveMonster)message->evidence.monster_after.actor.tag=0;
        if(!action_session::encode(*message,bytes,error))return false;
    }
    try{digest=mt2mp::sha256(bytes);}catch(const std::exception& cause){error=cause.what();return false;}
    if(!evidence_digest(digest)){error="Canonical evidence SHA256 invalid";return false;}
    error.clear();return true;
}
}
