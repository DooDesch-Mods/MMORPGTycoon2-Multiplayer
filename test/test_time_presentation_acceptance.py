import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from large_save_acceptance import confirm_time_ui
from large_group_readback import confirm_source
from local_acceptance import AcceptanceError

class TimePresentationProof(unittest.TestCase):
    def setUp(self):
        state = {'present': True, 'uncertain': False, 'revision': 1, 'selected': 0.0, 'pause': 1.0}
        self.engine = {'peer_id': 2, 'shared_time': {'initialized': True, 'hooks_ready': True,
            'status_read_confirmed': True, 'runtime': {'phase': 3, 'hook_mask': 3, 'epoch': 987,
            'presentation_uncertain': False, 'controller': {'started': True, 'write_locked': False,
            'applied': state, 'observed': copy.deepcopy(state)}}}}
        self.ui = {'capture_enabled': True, 'host_state_available': True, 'local_host': False,
            'epoch': 987, 'revision': 1, 'selected_scale': 0.0, 'pause_scale': 1.0, 'effective_scale': 0.0,
            'items': [{'scale': scale, 'native_state': 0 if scale == 0 else 1, 'active': scale == 0}
                      for scale in (0, 1, 3, 9)]}

    def test_confirmed_client_and_host(self):
        confirm_time_ui(self.ui, self.engine)
        self.engine['peer_id'], self.ui['local_host'] = 1, True
        confirm_time_ui(self.ui, self.engine)

    def test_host_original_items_are_observed_without_stores(self):
        self.engine['peer_id'], self.ui['local_host'] = 1, True
        for item in self.ui['items']:
            item.update(native_state=1, active=False)
        confirm_time_ui(self.ui, self.engine)

    def test_client_selection_uses_effective_modal_scale(self):
        self.engine['shared_time']['runtime']['controller']['applied'].update(selected=9.0, pause=0.0)
        self.ui.update(selected_scale=9.0, pause_scale=0.0)
        confirm_time_ui(self.ui, self.engine)

    def test_native_button_disagrees(self):
        self.ui['items'][1]['native_state'] = 0
        with self.assertRaises(AcceptanceError):
            confirm_time_ui(self.ui, self.engine)

    def test_authority_fields_must_match(self):
        for field, value in [('epoch', 986), ('revision', 2), ('revision', True), ('local_host', True),
                             ('selected_scale', 1), ('pause_scale', 0), ('effective_scale', 1),
                             ('capture_enabled', False), ('host_state_available', False)]:
            with self.subTest(field=field, value=value):
                ui = copy.deepcopy(self.ui)
                ui[field] = value
                with self.assertRaises(AcceptanceError):
                    confirm_time_ui(ui, self.engine)

    def test_inactive_or_uncertain_runtime_is_rejected(self):
        for field, value in [('phase', 1), ('hook_mask', 1), ('presentation_uncertain', True)]:
            engine = copy.deepcopy(self.engine)
            engine['shared_time']['runtime'][field] = value
            with self.assertRaises(AcceptanceError):
                confirm_time_ui(self.ui, engine)
        self.engine['shared_time']['runtime']['controller']['write_locked'] = True
        with self.assertRaises(AcceptanceError):
            confirm_time_ui(self.ui, self.engine)

    def test_legacy_mode_remains_separate(self):
        ui = copy.deepcopy(self.ui)
        ui.update(capture_enabled=False, host_state_available=False)
        confirm_time_ui(ui, {'peer_id': 2})
        with self.assertRaises(AcceptanceError):
            confirm_time_ui(self.ui, {'peer_id': 2})

    def test_only_exact_legacy_failure_can_resume(self):
        source = {'passed': False, 'proof': {key: True for key in
                  ('host_native_load', 'host_native_capture', 'transfer_and_client_load', 'network_identity_confirmed')},
                  'error': {'step': 'client-native-time-items',
                            'cause': 'AcceptanceError: time reader unexpectedly captures or applies state: {}'}}
        confirm_source(source, True)
        with self.assertRaises(AcceptanceError):
            confirm_source(source, False)
        source['error']['step'] = 'client-final-readback'
        with self.assertRaises(AcceptanceError):
            confirm_source(source, True)

    def test_failed_transfer_cannot_resume(self):
        source = {'passed': True, 'proof': {'transfer_and_client_load': False}}
        with self.assertRaises(AcceptanceError):
            confirm_source(source, True)

if __name__ == '__main__':
    unittest.main()
