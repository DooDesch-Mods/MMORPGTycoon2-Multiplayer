"""Bounded action/snapshot-owned node callback observation, without simulation writes."""
import re
from runtime import RuntimeErrorDetail
UUID = r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}'
SNAPSHOT_FIELDS = {'snapshot_uuid', 'region_id', 'node_index', 'graph_sha256'}
SCHEMAS = {
    'enable_node_observer': {'oneOf': [
        {'type': 'object', 'properties': {'action_uuid': {'type': 'string', 'pattern': '^'+UUID+'$'}},
         'required': ['action_uuid'], 'additionalProperties': False},
        {'type': 'object', 'properties': {
            'snapshot_uuid': {'type': 'string', 'pattern': '^'+UUID+'$'},
            'region_id': {'type': 'integer', 'minimum': 0, 'maximum': 2147483647},
            'node_index': {'type': 'integer', 'minimum': 0, 'maximum': 2147483647},
            'graph_sha256': {'type': 'string', 'pattern': '^[0-9a-f]{64}$'}},
         'required': sorted(SNAPSHOT_FIELDS), 'additionalProperties': False}]},
    'read_node_observer': {'type': 'object', 'properties': {}, 'additionalProperties': False}}
DESCRIPTIONS = {
    'enable_node_observer': 'Bind passive callbacks to a confirmed native Node action or one loaded Node selected from the current complete graph. Does not place, power or tick a Node. Do not replay a timed-out enable; read current observation first.',
    'read_node_observer': 'Read bounded original Node callback thread and scalar receipts with fresh native ownership. No native state writes.'}
MUTATIONS = {'enable_node_observer'}
def validate(op, args):
    if op not in SCHEMAS or not isinstance(args, dict):
        raise RuntimeErrorDetail('invalid_args', 'Node observer operation/argument object invalid')
    if op == 'read_node_observer':
        if args:
            raise RuntimeErrorDetail('invalid_args', 'Node observer read requires empty args')
        return
    if set(args) == {'action_uuid'} and isinstance(args['action_uuid'], str) and re.fullmatch(UUID, args['action_uuid']):
        return
    if set(args) == SNAPSHOT_FIELDS:
        if (isinstance(args['snapshot_uuid'], str) and re.fullmatch(UUID, args['snapshot_uuid']) and
                all(type(args[k]) is int and 0 <= args[k] <= 2147483647 for k in ('region_id', 'node_index')) and
                isinstance(args['graph_sha256'], str) and re.fullmatch('[0-9a-f]{64}', args['graph_sha256'])):
            return
    raise RuntimeErrorDetail('invalid_args', 'Node observer enable requires canonical action_uuid or exact snapshot_uuid/region_id/node_index/graph_sha256')
