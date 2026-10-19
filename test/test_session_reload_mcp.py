from pathlib import Path
import sys
from unittest.mock import Mock

sys.dont_write_bytecode = True
CANDIDATE = Path(__file__).resolve().parents[1]
ROOT = next(path for path in Path(__file__).resolve().parents if (path / 'tools/mcp/error_details.py').is_file())
sys.path.insert(0, str(ROOT / 'tools/mcp'))
sys.path.insert(0, str(CANDIDATE / 'tools/mcp'))
import server
from runtime import OPS, MUTATIONS, RuntimeErrorDetail

checks = 0
assert 'resync_world' in OPS and 'resync_world' in MUTATIONS
assert server.OP_SCHEMAS['resync_world'] == server.EMPTY
checks += 2
runtime = Mock()
pending = {'queued': True, 'active': False, 'completed': 0}
runtime.request.return_value = pending
assert server.dispatch(runtime, 'game_resync_world', {}) is pending
runtime.request.assert_called_once_with('resync_world', {})
checks += 2
for value in (None, [], False, 'world', {'snapshot_id': 'unowned'}, {'peer': 2}, {'force': True}):
    runtime.reset_mock()
    try:
        server.dispatch(runtime, 'game_resync_world', value)
    except RuntimeErrorDetail as error:
        assert error.code == 'invalid_args'
    else:
        raise AssertionError(f'invalid resync arguments reached native runtime: {value!r}')
    runtime.request.assert_not_called()
    checks += 2
for cause in ('native capture pending', 'previous resync request still pending'):
    runtime.reset_mock()
    runtime.request.side_effect = RuntimeErrorDetail('WORLD_RESYNC_REJECTED', cause)
    try:
        server.dispatch(runtime, 'game_resync_world', {})
    except RuntimeErrorDetail as error:
        assert str(error) == cause
    else:
        raise AssertionError('native resync rejection became success')
    runtime.request.assert_called_once_with('resync_world', {})
    checks += 2
runtime.request.side_effect = None
runtime.available.return_value = {'resync_world'}
tool = next(x for x in server.tools_for(runtime) if x.name == 'game_resync_world')
assert tool.annotations.destructiveHint is True and tool.annotations.idempotentHint is False
checks += 1
print(f'{checks} actual MCP resync admission/validation/no-replay cases passed; no game calls')
