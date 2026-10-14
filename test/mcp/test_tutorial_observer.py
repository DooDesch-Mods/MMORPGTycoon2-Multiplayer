"""Read-only tutorial observer adapter contracts without game execution."""
import unittest
from unittest.mock import Mock

from runtime import MUTATIONS, OPS, RuntimeErrorDetail
from server import dispatch, tools_for


class TutorialObserverTests(unittest.TestCase):
    def test_observation_is_read_only_and_has_no_arguments(self):
        runtime=Mock()
        runtime.available.return_value={"read_tutorial"}
        tool=next(tool for tool in tools_for(runtime) if tool.name=="game_read_tutorial")
        self.assertIn("read_tutorial",OPS)
        self.assertNotIn("read_tutorial",MUTATIONS)
        self.assertTrue(tool.annotations.readOnlyHint)
        self.assertFalse(tool.annotations.destructiveHint)
        self.assertFalse(tool.inputSchema["additionalProperties"])
        self.assertEqual(tool.inputSchema["properties"],{})

    def test_dispatch_preserves_native_scalar_receipt(self):
        runtime=Mock()
        runtime.request.return_value={"state":"active","tutorials":[{"scenario_id":"initial_tutorial","current_step_index":4,"current_step_id":"welcome"}]}
        result=dispatch(runtime,"game_read_tutorial",{})
        runtime.request.assert_called_once_with("read_tutorial",{})
        self.assertEqual(result,runtime.request.return_value)

    def test_invalid_arguments_never_reach_the_mailbox(self):
        for bad in (None,[],[{}],{"advance":True},{"scenario_id":"initial_tutorial"},{"index":4}):
            runtime=Mock()
            with self.subTest(arguments=bad),self.assertRaises(RuntimeErrorDetail):
                dispatch(runtime,"game_read_tutorial",bad)
            runtime.request.assert_not_called()

    def test_native_failure_is_not_converted_to_inactive(self):
        runtime=Mock()
        runtime.request.side_effect=RuntimeErrorDetail("TUTORIAL_READ_REJECTED","current_world_read_wrong_thread")
        with self.assertRaisesRegex(RuntimeErrorDetail,"current_world_read_wrong_thread"):
            dispatch(runtime,"game_read_tutorial",{})

    def test_welcome_close_is_a_serialized_mutation(self):
        runtime=Mock()
        runtime.available.return_value={"tutorial_welcome_ok"}
        tool=next(tool for tool in tools_for(runtime) if tool.name=="game_tutorial_welcome_ok")
        self.assertIn("tutorial_welcome_ok",MUTATIONS)
        self.assertFalse(tool.annotations.readOnlyHint)
        self.assertTrue(tool.annotations.destructiveHint)

    def test_welcome_bad_guards_never_reach_native_mailbox(self):
        valid={"token":"b473438a-510a-4f85-8337-b4e154ea15d6",
               "snapshot_uuid":"77e1531e-709c-4355-b079-465f53cb61a2",
               "world_epoch":1,"world_generation":1,"current_step_index":4}
        bad=[{}, {**valid,"extra":True}, {**valid,"token":"replay"},
             {**valid,"world_epoch":0}, {**valid,"world_generation":True},
             {**valid,"current_step_index":35}, {**valid,"current_step_index":True},
             {**valid,"current_step_index":4294967300}]
        for arguments in bad:
            runtime=Mock()
            with self.subTest(arguments=arguments),self.assertRaises(RuntimeErrorDetail):
                dispatch(runtime,"game_tutorial_welcome_ok",arguments)
            runtime.request.assert_not_called()


if __name__=="__main__":
    unittest.main()
