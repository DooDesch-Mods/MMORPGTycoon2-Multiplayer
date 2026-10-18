"""Native lobby Chat arguments stay bounded before crossing the game mailbox."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mcp'))
import lobby_ops
from runtime import RuntimeErrorDetail

TOKEN = '00000000-0000-4000-8000-000000000001'

class LobbyChatTests(unittest.TestCase):
    def check_args(self, **extra):
        lobby_ops.validate('native_lobby_button', {'acceptance_token': TOKEN,
                                                   'button': 'chat_send', **extra})

    def test_send_existing_native_draft(self):
        self.check_args()

    def test_unicode_at_exact_byte_boundary(self):
        self.check_args(text='ü' * 120)

    def test_over_byte_limit_rejects(self):
        with self.assertRaisesRegex(RuntimeErrorDetail, 'exceeds 240 bytes'):
            self.check_args(text='ü' * 121)

    def test_empty_controls_and_nontext_reject(self):
        for text in ('', 'a\nb', '\x7f', '\x85', None, 12):
            with self.subTest(text=text), self.assertRaises(RuntimeErrorDetail):
                self.check_args(text=text)

    def test_invalid_unicode_rejects(self):
        with self.assertRaisesRegex(RuntimeErrorDetail, 'valid UTF-8'):
            self.check_args(text='\ud800')

    def test_text_on_other_buttons_rejects(self):
        with self.assertRaises(RuntimeErrorDetail):
            lobby_ops.validate('native_lobby_button', {'acceptance_token': TOKEN,
                                                       'button': 'leave', 'text': 'hello'})

    def test_multiple_optional_fields_reject(self):
        with self.assertRaisesRegex(RuntimeErrorDetail, 'one optional'):
            self.check_args(text='hello', lobby_id='12')

    def test_native_start_menu_target(self):
        lobby_ops.validate('native_lobby_button', {'acceptance_token': TOKEN, 'button': 'start_menu'})
        self.assertIn('start_menu', lobby_ops.SCHEMAS['native_lobby_button']['properties']['button']['enum'])
        for extra in ({'text': 'hello'}, {'world': 'Test world'}, {'lobby_id': '12'}):
            with self.subTest(extra=extra), self.assertRaises(RuntimeErrorDetail):
                lobby_ops.validate('native_lobby_button', {'acceptance_token': TOKEN, 'button': 'start_menu', **extra})

    def test_existing_join_and_pick_contract(self):
        for button, extra in (('join', {'lobby_id': '12'}), ('pick', {'world': 'Test world'})):
            lobby_ops.validate('native_lobby_button', {'acceptance_token': TOKEN,
                                                       'button': button, **extra})

if __name__ == '__main__':
    unittest.main()
