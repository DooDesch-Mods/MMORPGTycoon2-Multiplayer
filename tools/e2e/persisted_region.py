"""Compare saved Region content without constructor-only random generators."""
from tools.e2e.region_properties import region_state


def persisted_region_state(value):
    # The native save has no Region RNG fields. Its constructor seeds +0x268
    # from time; Finish binds +0x160 to that same source. Action RNG checks
    # remain separate from this saved-content comparison.
    state = region_state(value)
    return {key: field for key, field in state.items() if key not in ('random', 'type_random')}
