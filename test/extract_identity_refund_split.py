from pathlib import Path
import argparse
import hashlib

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
source = (args.root / 'src/game/remove_diagnostics.cpp').read_text(encoding='utf-8')

def body(anchor):
    start = source.index(anchor)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

# Reverse only this split, so every existing guard and public consumer stays bound.
original = source
identity_wrapper = body('bool identity_bindings(') + '\n'
assert original.count(identity_wrapper) == 1
original = original.replace(identity_wrapper, '', 1)
# Character retirement is an independent include-only extension. Keep the
# original identity/refund guards bound to their exact prior source.
retirement_headers = ('#include "character_boss_retirement_read.h"\n'
                      '#include "typed_action/action_session.h"\n'
                      '#include "typed_action_runtime.h"\n'
                      '#include "move_contract.h"\n'
                      '#include "dungeon_scene_identity_runtime.h"\n'
                      '#include "region_saved_identity.h"\n')
retirement_runtime = '#include "character_boss_retirement_runtime.inc"\n\n'
assert original.count(retirement_headers) == 1, 'Character retirement include boundary differs'
assert original.count(retirement_runtime) == 1, 'Character retirement runtime boundary differs'
original = original.replace(retirement_headers, '', 1).replace(retirement_runtime, '', 1)
# Independent identity readers do not participate in this collector/refund split.
for anchor, suffix in (('bool resolve_current_building_key(', '\n\n'),
                       ('bool resolve_current_building_identity(', '\n'),
                       ('bool identity_targets(', '\n\n')):
    addition = body(anchor) + suffix
    assert original.count(addition) == 1, 'Scene identity lookup boundary differs'
    original = original.replace(addition, '', 1)
include = '#include "service_building_census.h"\n'
assert original.count(include) == 1, 'Service census include boundary differs'
original = original.replace(include, '', 1)
helper = ('namespace {\nbool resolve_local_target_impl(uintptr_t manager,uintptr_t object,LocalTarget& out,std::string& error,\n'
          '                               const host::opened::CurrentWorld* admitted) {')
assert original.count(helper) == 1
original = original.replace(helper, 'bool resolve_local_target(uintptr_t manager,uintptr_t object,LocalTarget& out,std::string& error) {', 1)
admission = ('    if(admitted) {\n'
             '        if(!host::same_world(*admitted,opened)){error="native UI admitted world changed before target observation";return false;}\n'
             '    } else {\n'
             '        json sources;if(!collect_bindings(sources,error,BindingDetails::Identity))return false;\n'
             '    }')
assert original.count(admission) == 1
original = original.replace(admission, '    json sources;if(!collect_bindings(sources,error,BindingDetails::Identity))return false;', 1)
local_wrapper = ('}\nbool resolve_local_target(uintptr_t manager,uintptr_t object,LocalTarget& out,std::string& error) {\n'
                 '    return resolve_local_target_impl(manager,object,out,error,nullptr);\n}\n')
assert original.count(local_wrapper) == 1
original = original.replace(local_wrapper, '', 1)
record_call = 'resolve_local_target_impl(world.selection,uintptr_t(object),out,error,&world.origin)'
assert original.count(record_call) == 1
original = original.replace(record_call, 'resolve_local_target(world.selection,uintptr_t(object),out,error)', 1)
assert hashlib.sha256(original.encode()).hexdigest() == '5929bfadd2acd723ad6cf07f199837bdaf52a4ffa9b1ea2fc1519de36010ff1c', 'Same-call admission changed another guard or consumer'
old = 'namespace {\nenum class BindingDetails {Identity,Refund};\nbool collect_bindings(json& out,std::string& error,BindingDetails details) {'
assert original.count(old) == 1
original = original.replace(old, 'bool bindings(json& out,std::string& error) {', 1)
for rows in ('retained', 'list'):
    new = f'if(details==BindingDetails::Refund&&!attach_refunds(world,{rows},error))'
    assert original.count(new) == 1
    original = original.replace(new, f'if(!attach_refunds(world,{rows},error))', 1)
wrapper = '}\nbool bindings(json& out,std::string& error) {\n    return collect_bindings(out,error,BindingDetails::Refund);\n}\n'
assert original.count(wrapper) == 1
original = original.replace(wrapper, '', 1)
edge = 'collect_bindings(sources,error,BindingDetails::Identity)'
assert original.count(edge) == 2
for anchor in ('bool resolve_local_target_impl(', 'bool resolve_record_target('):
    assert body(anchor).count(edge) == 1
original = original.replace(edge, 'bindings(sources,error)')
assert hashlib.sha256(original.encode()).hexdigest() == '6aa33a7d60b77f636002b1f0eb3086f71c7f1c94994559acf70b8a61a0d2fde2', 'Identity/refund split changed an unrelated guard or consumer'

pieces = [body('std::string binding_id('), body('std::string registry_id('),
          body('template<class Binding>bool import_bindings('),
          body('enum class BindingDetails ') + ';', body('bool collect_bindings('),
          body('bool bindings(json& out,'), body('bool identity_bindings('), body('bool attach_refunds(const host::opened::CurrentWorld& origin,'),
          body('bool resolve_local_target_impl('), body('bool resolve_local_target('), body('bool resolve_record_target(')]
args.output.mkdir(parents=True, exist_ok=True)
(args.output / 'identity_refund_split_type.inc').write_text(body('struct Bound ') + ';\n', encoding='utf-8')
(args.output / 'identity_refund_split_bodies.inc').write_text('\n'.join(pieces) + '\n', encoding='utf-8')
print('Extracted complete collector, public report, refund enrichment, both resolvers and registry import; full original source guard passed.')
