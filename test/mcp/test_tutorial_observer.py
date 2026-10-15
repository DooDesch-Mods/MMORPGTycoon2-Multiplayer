"""Read-only tutorial observer adapter contracts without game execution."""
import unittest
from unittest.mock import Mock

from runtime import MUTATIONS, OPS, RuntimeErrorDetail
from server import dispatch, tools_for


class TutorialObserverTests(unittest.TestCase):
    def test_staff_activity_preserves_native_unknown_receipt(self):
        args={"token":"b473438a-510a-4f85-8337-b4e154ea15d6","native_origin_uuid":"77e1531e-709c-4355-b079-465f53cb61a2",
              "world_epoch":1,"world_generation":2,"operation":"staff_activity_state","expected_development":{"project":{"present":True}}}
        receipt={"attempted":False,"read_only":True,"confirmed":False,"unknown":True,"retry_blocked":True,
                 "cause":"original LocArg exception", "native_calls":{"formatter":{"attempted":1,"returned":0,"confirmed":False}}}
        runtime=Mock();runtime.request.return_value=receipt
        self.assertIs(dispatch(runtime,"game_tutorial_startingpoint",args),receipt)
        runtime.request.assert_called_once_with("tutorial_startingpoint",args)

    def test_staff_activity_cannot_supply_labels_or_developer_pointer(self):
        args={"token":"b473438a-510a-4f85-8337-b4e154ea15d6","native_origin_uuid":"77e1531e-709c-4355-b079-465f53cb61a2",
              "world_epoch":1,"world_generation":2,"operation":"staff_activity_state","expected_development":{"project":{"present":True}}}
        for extra in ({"label":"arbitrary"},{"developer":42},{"call":"IdleTick"}):
            runtime=Mock()
            with self.subTest(extra=extra),self.assertRaises(RuntimeErrorDetail):
                dispatch(runtime,"game_tutorial_startingpoint",{**args,**extra})
            runtime.request.assert_not_called()

    def test_research_state_preserves_read_only_pending_receipt(self):
        args={"token":"b473438a-510a-4f85-8337-b4e154ea15d6","native_origin_uuid":"77e1531e-709c-4355-b079-465f53cb61a2",
              "world_epoch":1,"world_generation":2,"operation":"client_research_state","expected_development":{"project":{"present":False}}}
        receipt={"attempted":False,"returned":True,"confirmed":True,"unknown":False,"read_only":True,"ready":False,
                 "cause":"active page={dev_tab_staff} required={dev_tab_tech}","ui":{"grid_owner":"null","button_disabled":1}}
        runtime=Mock();runtime.request.return_value=receipt
        self.assertIs(dispatch(runtime,"game_tutorial_startingpoint",args),receipt)
        runtime.request.assert_called_once_with("tutorial_startingpoint",args)

    def test_staff_state_preserves_effectless_pending_details(self):
        args={"token":"b473438a-510a-4f85-8337-b4e154ea15d6","native_origin_uuid":"77e1531e-709c-4355-b079-465f53cb61a2",
              "world_epoch":1,"world_generation":2,"operation":"client_staff_state","expected_development":{"project":{"present":True}}}
        receipt={"attempted":False,"returned":True,"confirmed":True,"unknown":False,"read_only":True,"ready":False,
                 "cause":"pips owner=null","ui":{"pips_owner":"null","button_toggle":1,"available_workers":1}}
        runtime=Mock();runtime.request.return_value=receipt
        self.assertIs(dispatch(runtime,"game_tutorial_startingpoint",args),receipt)
        runtime.request.assert_called_once_with("tutorial_startingpoint",args)

    def test_staff_has_no_arbitrary_worker_or_button_argument(self):
        args={"token":"b473438a-510a-4f85-8337-b4e154ea15d6","native_origin_uuid":"77e1531e-709c-4355-b079-465f53cb61a2",
              "world_epoch":1,"world_generation":2,"operation":"client_staff_one","expected_development":{"project":{"present":True}}}
        for bad in ({**args,"desired_workers":2},{**args,"button":42},{**args,"operation":"client_staff_two"}):
            runtime=Mock()
            with self.subTest(arguments=bad),self.assertRaises(RuntimeErrorDetail):
                dispatch(runtime,"game_tutorial_startingpoint",bad)
            runtime.request.assert_not_called()

    def test_startingpoint_client_ui_preserves_submission_not_completion(self):
        good={"token":"b473438a-510a-4f85-8337-b4e154ea15d6","native_origin_uuid":"77e1531e-709c-4355-b079-465f53cb61a2",
              "world_epoch":1,"world_generation":2,"operation":"client_commit","expected_development":{"project":{"present":True}}}
        receipt={"attempted":True,"returned":True,"confirmed":True,"unknown":False,"submitted":True,
                 "action_uuid":"f473438a-510a-4f85-8337-b4e154ea15d6","host_completion_confirmed":False}
        for operation in ("client_show","client_commit","client_research_show","client_research","client_staff_show","client_staff_one"):
            with self.subTest(operation=operation):
                runtime=Mock();runtime.request.return_value=receipt
                args={**good,"operation":operation}
                self.assertIs(dispatch(runtime,"game_tutorial_startingpoint",args),receipt)
                runtime.request.assert_called_once_with("tutorial_startingpoint",args)
        for bad in ({**good,"operation":"client_complete"},{**good,"command":"CommitTech"},{**good,"world_epoch":0}):
            runtime=Mock()
            with self.subTest(arguments=bad),self.assertRaises(RuntimeErrorDetail):
                dispatch(runtime,"game_tutorial_startingpoint",bad)
            runtime.request.assert_not_called()

    def test_startingpoint_fixed_mutation_preserves_actual_development_guards(self):
        runtime=Mock()
        runtime.available.return_value={"tutorial_startingpoint"}
        tool=next(tool for tool in tools_for(runtime) if tool.name=="game_tutorial_startingpoint")
        self.assertIn("tutorial_startingpoint",MUTATIONS)
        self.assertFalse(tool.annotations.readOnlyHint)
        self.assertEqual(tool.inputSchema["properties"]["operation"]["enum"],["begin","show","complete","commit","client_show","client_commit","client_research_show","client_research","client_research_state","client_staff_show","client_staff_state","client_staff_one","staff_activity_state"])
        args={"token":"b473438a-510a-4f85-8337-b4e154ea15d6","native_origin_uuid":"77e1531e-709c-4355-b079-465f53cb61a2",
              "world_epoch":1,"world_generation":2,"operation":"complete","expected_development":{"project":{"present":True}}}
        runtime.request.return_value={"attempted":True,"confirmed":True}
        self.assertEqual(dispatch(runtime,"game_tutorial_startingpoint",args),runtime.request.return_value)
        runtime.request.assert_called_once_with("tutorial_startingpoint",args)

    def test_startingpoint_begin_preserves_original_payment_and_unknown_receipts(self):
        args={"token":"b473438a-510a-4f85-8337-b4e154ea15d6","native_origin_uuid":"77e1531e-709c-4355-b079-465f53cb61a2",
              "world_epoch":1,"world_generation":2,"operation":"begin",
              "expected_development":{"project":{"present":False},"startingpoint":{"record_present":True,"purchased":False}}}
        receipts=({"operation":"begin","attempted":True,"returned":True,"payment_accepted":True,"confirmed":True,"unknown":False,"price":100},
                  {"operation":"begin","attempted":True,"returned":False,"confirmed":False,"unknown":True,
                   "cause":"startingpoint begin original Pay exception=5"})
        for receipt in receipts:
            with self.subTest(receipt=receipt):
                runtime=Mock()
                runtime.request.return_value=receipt
                self.assertIs(dispatch(runtime,"game_tutorial_startingpoint",args),receipt)
                runtime.request.assert_called_once_with("tutorial_startingpoint",args)

    def test_startingpoint_begin_requires_existing_native_origin_and_readback_guards(self):
        good={"token":"b473438a-510a-4f85-8337-b4e154ea15d6","native_origin_uuid":"77e1531e-709c-4355-b079-465f53cb61a2",
              "world_epoch":1,"world_generation":2,"operation":"begin","expected_development":{"project":{"present":False}}}
        for bad in ({**good,"world_epoch":0},{**good,"native_origin_uuid":"other"},
                    {**good,"expected_development":{}},{**good,"price":0}):
            runtime=Mock()
            with self.subTest(arguments=bad),self.assertRaises(RuntimeErrorDetail):
                dispatch(runtime,"game_tutorial_startingpoint",bad)
            runtime.request.assert_not_called()

    def test_startingpoint_invalid_fields_never_reach_native(self):
        good={"token":"b473438a-510a-4f85-8337-b4e154ea15d6","native_origin_uuid":"77e1531e-709c-4355-b079-465f53cb61a2",
              "world_epoch":1,"world_generation":2,"operation":"commit","expected_development":{"project":{"present":True}}}
        for bad in (None,{},[],{**good,"operation":"deploy-other"},{**good,"address":42},{**good,"token":"replay"},
                    {**good,"world_epoch":0},{**good,"world_generation":True},{**good,"world_epoch":2**64},
                    {**good,"expected_development":{}},{**good,"expected_development":[]}):
            runtime=Mock()
            with self.subTest(arguments=bad),self.assertRaises(RuntimeErrorDetail):
                dispatch(runtime,"game_tutorial_startingpoint",bad)
            runtime.request.assert_not_called()

    def test_startingpoint_failure_retains_native_cause(self):
        args={"token":"b473438a-510a-4f85-8337-b4e154ea15d6","native_origin_uuid":"77e1531e-709c-4355-b079-465f53cb61a2",
              "world_epoch":1,"world_generation":2,"operation":"show","expected_development":{"project":{"present":True}}}
        runtime=Mock()
        runtime.request.side_effect=RuntimeErrorDetail("TUTORIAL_STARTINGPOINT_REJECTED","Development owner changed")
        with self.assertRaisesRegex(RuntimeErrorDetail,"Development owner changed"):
            dispatch(runtime,"game_tutorial_startingpoint",args)

    def test_focus_request_and_readback_keep_distinct_contracts(self):
        runtime=Mock()
        runtime.available.return_value={"tutorial_movement_focus", "read_tutorial_movement_focus"}
        listed={tool.name:tool for tool in tools_for(runtime)}
        self.assertIn("tutorial_movement_focus",MUTATIONS)
        self.assertNotIn("read_tutorial_movement_focus",MUTATIONS)
        self.assertFalse(listed["game_tutorial_movement_focus"].annotations.readOnlyHint)
        self.assertTrue(listed["game_read_tutorial_movement_focus"].annotations.readOnlyHint)
        valid={"native_origin_uuid":"77e1531e-709c-4355-b079-465f53cb61a2"}
        runtime.request.return_value={"requested":True,"ready":False}
        self.assertEqual(dispatch(runtime,"game_tutorial_movement_focus",valid),{"requested":True,"ready":False})
        runtime.request.assert_called_once_with("tutorial_movement_focus",valid)
        for bad in ({}, {**valid,"hwnd":42}, {**valid,"native_origin_uuid":"other"}, None):
            runtime=Mock()
            with self.subTest(arguments=bad),self.assertRaises(RuntimeErrorDetail):
                dispatch(runtime,"game_tutorial_movement_focus",bad)
            runtime.request.assert_not_called()
        runtime=Mock()
        with self.assertRaises(RuntimeErrorDetail):
            dispatch(runtime,"game_read_tutorial_movement_focus",valid)
        runtime.request.assert_not_called()

    def test_movement_is_fixed_serialized_mutation_with_separate_readback(self):
        runtime=Mock()
        runtime.available.return_value={"tutorial_movement", "read_tutorial_movement"}
        listed={tool.name:tool for tool in tools_for(runtime)}
        self.assertIn("tutorial_movement",MUTATIONS)
        self.assertNotIn("read_tutorial_movement",MUTATIONS)
        self.assertFalse(listed["game_tutorial_movement"].annotations.readOnlyHint)
        self.assertTrue(listed["game_read_tutorial_movement"].annotations.readOnlyHint)
        self.assertEqual(set(listed["game_tutorial_movement"].inputSchema["required"]), {"acceptance_token", "native_origin_uuid"})
        args={"acceptance_token":"b473438a-510a-4f85-8337-b4e154ea15d6", "native_origin_uuid":"77e1531e-709c-4355-b079-465f53cb61a2"}
        runtime.request.return_value={"phase":"down_queued"}
        self.assertEqual(dispatch(runtime,"game_tutorial_movement",args),{"phase":"down_queued"})
        runtime.request.assert_called_once_with("tutorial_movement",args)

    def test_movement_invalid_inputs_never_reach_native(self):
        good={"acceptance_token":"b473438a-510a-4f85-8337-b4e154ea15d6", "native_origin_uuid":"77e1531e-709c-4355-b079-465f53cb61a2"}
        for bad in (None, [], {}, {**good,"key":"A"}, {**good,"duration":5}, {**good,"acceptance_token":"replay"}):
            runtime=Mock()
            with self.subTest(arguments=bad),self.assertRaises(RuntimeErrorDetail):
                dispatch(runtime,"game_tutorial_movement",bad)
            runtime.request.assert_not_called()
        runtime=Mock()
        with self.assertRaises(RuntimeErrorDetail):
            dispatch(runtime,"game_read_tutorial_movement",{"key":"W"})
        runtime.request.assert_not_called()

    def test_progress_read_is_read_only_and_preserves_the_receipt(self):
        runtime=Mock()
        runtime.available.return_value={"read_tutorial_progress"}
        tool=next(tool for tool in tools_for(runtime) if tool.name=="game_read_tutorial_progress")
        self.assertIn("read_tutorial_progress",OPS)
        self.assertNotIn("read_tutorial_progress",MUTATIONS)
        self.assertTrue(tool.annotations.readOnlyHint)
        self.assertFalse(tool.annotations.destructiveHint)
        self.assertFalse(tool.inputSchema["additionalProperties"])
        self.assertEqual(tool.inputSchema["properties"],{})
        runtime.request.return_value={"schema":1,"client_sequence_fenced":True,"tutorial":{"active":[]}}
        self.assertEqual(dispatch(runtime,"game_read_tutorial_progress",{}),runtime.request.return_value)
        runtime.request.assert_called_once_with("read_tutorial_progress",{})

    def test_progress_invalid_arguments_and_native_failure_stay_visible(self):
        for bad in (None,[],[{}],{"value":100},{"complete":True}):
            runtime=Mock()
            with self.subTest(arguments=bad),self.assertRaises(RuntimeErrorDetail):
                dispatch(runtime,"game_read_tutorial_progress",bad)
            runtime.request.assert_not_called()
        runtime=Mock()
        runtime.request.side_effect=RuntimeErrorDetail("TUTORIAL_PROGRESS_READ_REJECTED","tutorial origin changed")
        with self.assertRaisesRegex(RuntimeErrorDetail,"tutorial origin changed"):
            dispatch(runtime,"game_read_tutorial_progress",{})

    def test_development_receipt_is_read_only_and_has_no_arguments(self):
        runtime=Mock()
        runtime.available.return_value={"read_tutorial_development"}
        tool=next(tool for tool in tools_for(runtime) if tool.name=="game_read_tutorial_development")
        self.assertIn("read_tutorial_development",OPS)
        self.assertNotIn("read_tutorial_development",MUTATIONS)
        self.assertTrue(tool.annotations.readOnlyHint)
        self.assertFalse(tool.annotations.destructiveHint)
        self.assertFalse(tool.inputSchema["additionalProperties"])
        self.assertEqual(tool.inputSchema["properties"],{})

    def test_development_dispatch_preserves_native_receipt(self):
        runtime=Mock()
        runtime.request.return_value={"schema":1,"project":{"present":True,"tech_key":"startingpoint"},
                                      "startingpoint":{"owned":False,"inventory_count":0}}
        result=dispatch(runtime,"game_read_tutorial_development",{})
        runtime.request.assert_called_once_with("read_tutorial_development",{})
        self.assertEqual(result,runtime.request.return_value)

    def test_development_invalid_arguments_never_reach_the_mailbox(self):
        for bad in (None,[],[{}],{"commit":True},{"tech_key":"startingpoint"}):
            runtime=Mock()
            with self.subTest(arguments=bad),self.assertRaises(RuntimeErrorDetail):
                dispatch(runtime,"game_read_tutorial_development",bad)
            runtime.request.assert_not_called()

    def test_development_failure_keeps_the_native_cause(self):
        runtime=Mock()
        runtime.request.side_effect=RuntimeErrorDetail("TUTORIAL_DEVELOPMENT_READ_REJECTED","current_world_read_wrong_thread")
        with self.assertRaisesRegex(RuntimeErrorDetail,"current_world_read_wrong_thread"):
            dispatch(runtime,"game_read_tutorial_development",{})

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

    def test_finish_close_preserves_native_receipt_and_fixed_index(self):
        args={"token":"b473438a-510a-4f85-8337-b4e154ea15d6",
              "snapshot_uuid":"77e1531e-709c-4355-b079-465f53cb61a2",
              "world_epoch":1,"world_generation":2,"current_step_index":103}
        receipt={"attempted":True,"confirmed":False,"unknown":True,"cause":"original Show exception"}
        runtime=Mock();runtime.request.return_value=receipt
        self.assertIs(dispatch(runtime,"game_tutorial_finish_ok",args),receipt)
        runtime.request.assert_called_once_with("tutorial_finish_ok",args)
        runtime.available.return_value={"tutorial_finish_ok"}
        tool=next(tool for tool in tools_for(runtime) if tool.name=="game_tutorial_finish_ok")
        self.assertIn("tutorial_finish_ok",OPS)
        self.assertIn("tutorial_finish_ok",MUTATIONS)
        self.assertFalse(tool.annotations.readOnlyHint)
        self.assertEqual(tool.inputSchema["properties"]["current_step_index"]["const"],103)

    def test_finish_close_rejects_other_messages_and_arbitrary_calls(self):
        args={"token":"b473438a-510a-4f85-8337-b4e154ea15d6",
              "snapshot_uuid":"77e1531e-709c-4355-b079-465f53cb61a2",
              "world_epoch":1,"world_generation":2,"current_step_index":103}
        for bad in ({**args,"current_step_index":4},{**args,"current_step_index":104},
                    {**args,"current_step_index":True},{**args,"world_epoch":0},
                    {**args,"window":42},{**args,"command":"TutorialComplete"}):
            runtime=Mock()
            with self.subTest(arguments=bad),self.assertRaises(RuntimeErrorDetail):
                dispatch(runtime,"game_tutorial_finish_ok",bad)
            runtime.request.assert_not_called()

    def test_welcome_bad_guards_never_reach_native_mailbox(self):
        valid={"token":"b473438a-510a-4f85-8337-b4e154ea15d6",
               "snapshot_uuid":"77e1531e-709c-4355-b079-465f53cb61a2",
               "world_epoch":1,"world_generation":1,"current_step_index":4}
        bad=[{}, {**valid,"extra":True}, {**valid,"token":"replay"},
             {**valid,"world_epoch":0}, {**valid,"world_generation":True},
             {**valid,"current_step_index":35}, {**valid,"current_step_index":103}, {**valid,"current_step_index":True},
             {**valid,"current_step_index":4294967300}]
        for arguments in bad:
            runtime=Mock()
            with self.subTest(arguments=arguments),self.assertRaises(RuntimeErrorDetail):
                dispatch(runtime,"game_tutorial_welcome_ok",arguments)
            runtime.request.assert_not_called()


if __name__=="__main__":
    unittest.main()
