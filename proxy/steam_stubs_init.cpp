// Auto-generated: fills function pointers for Steam API proxy
#include <windows.h>
#include <cstdio>

extern "C" void* __GetHSteamPipe_ptr;
extern "C" void* __GetHSteamUser_ptr;
extern "C" void* __SteamAPI_GetHSteamPipe_ptr;
extern "C" void* __SteamAPI_GetHSteamUser_ptr;
extern "C" void* __SteamAPI_GetSteamInstallPath_ptr;
extern "C" void* __SteamAPI_ISteamApps_BGetDLCDataByIndex_ptr;
extern "C" void* __SteamAPI_ISteamApps_BIsAppInstalled_ptr;
extern "C" void* __SteamAPI_ISteamApps_BIsCybercafe_ptr;
extern "C" void* __SteamAPI_ISteamApps_BIsDlcInstalled_ptr;
extern "C" void* __SteamAPI_ISteamApps_BIsLowViolence_ptr;
extern "C" void* __SteamAPI_ISteamApps_BIsSubscribed_ptr;
extern "C" void* __SteamAPI_ISteamApps_BIsSubscribedApp_ptr;
extern "C" void* __SteamAPI_ISteamApps_BIsSubscribedFromFamilySharing_ptr;
extern "C" void* __SteamAPI_ISteamApps_BIsSubscribedFromFreeWeekend_ptr;
extern "C" void* __SteamAPI_ISteamApps_BIsTimedTrial_ptr;
extern "C" void* __SteamAPI_ISteamApps_BIsVACBanned_ptr;
extern "C" void* __SteamAPI_ISteamApps_GetAppBuildId_ptr;
extern "C" void* __SteamAPI_ISteamApps_GetAppInstallDir_ptr;
extern "C" void* __SteamAPI_ISteamApps_GetAppOwner_ptr;
extern "C" void* __SteamAPI_ISteamApps_GetAvailableGameLanguages_ptr;
extern "C" void* __SteamAPI_ISteamApps_GetBetaInfo_ptr;
extern "C" void* __SteamAPI_ISteamApps_GetCurrentBetaName_ptr;
extern "C" void* __SteamAPI_ISteamApps_GetCurrentGameLanguage_ptr;
extern "C" void* __SteamAPI_ISteamApps_GetDLCCount_ptr;
extern "C" void* __SteamAPI_ISteamApps_GetDlcDownloadProgress_ptr;
extern "C" void* __SteamAPI_ISteamApps_GetEarliestPurchaseUnixTime_ptr;
extern "C" void* __SteamAPI_ISteamApps_GetFileDetails_ptr;
extern "C" void* __SteamAPI_ISteamApps_GetInstalledDepots_ptr;
extern "C" void* __SteamAPI_ISteamApps_GetLaunchCommandLine_ptr;
extern "C" void* __SteamAPI_ISteamApps_GetLaunchQueryParam_ptr;
extern "C" void* __SteamAPI_ISteamApps_GetNumBetas_ptr;
extern "C" void* __SteamAPI_ISteamApps_InstallDLC_ptr;
extern "C" void* __SteamAPI_ISteamApps_MarkContentCorrupt_ptr;
extern "C" void* __SteamAPI_ISteamApps_RequestAllProofOfPurchaseKeys_ptr;
extern "C" void* __SteamAPI_ISteamApps_RequestAppProofOfPurchaseKey_ptr;
extern "C" void* __SteamAPI_ISteamApps_SetActiveBeta_ptr;
extern "C" void* __SteamAPI_ISteamApps_SetDlcContext_ptr;
extern "C" void* __SteamAPI_ISteamApps_UninstallDLC_ptr;
extern "C" void* __SteamAPI_ISteamClient_BReleaseSteamPipe_ptr;
extern "C" void* __SteamAPI_ISteamClient_BShutdownIfAllPipesClosed_ptr;
extern "C" void* __SteamAPI_ISteamClient_ConnectToGlobalUser_ptr;
extern "C" void* __SteamAPI_ISteamClient_CreateLocalUser_ptr;
extern "C" void* __SteamAPI_ISteamClient_CreateSteamPipe_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetIPCCallCount_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamApps_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamController_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamFriends_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamGameSearch_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamGameServer_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamGameServerStats_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamGenericInterface_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamHTMLSurface_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamHTTP_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamInput_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamInventory_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamMatchmaking_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamMatchmakingServers_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamMusic_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamMusicRemote_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamNetworking_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamParentalSettings_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamParties_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamRemotePlay_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamRemoteStorage_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamScreenshots_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamUGC_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamUser_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamUserStats_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamUtils_ptr;
extern "C" void* __SteamAPI_ISteamClient_GetISteamVideo_ptr;
extern "C" void* __SteamAPI_ISteamClient_ReleaseUser_ptr;
extern "C" void* __SteamAPI_ISteamClient_SetLocalIPBinding_ptr;
extern "C" void* __SteamAPI_ISteamClient_SetWarningMessageHook_ptr;
extern "C" void* __SteamAPI_ISteamController_ActivateActionSet_ptr;
extern "C" void* __SteamAPI_ISteamController_ActivateActionSetLayer_ptr;
extern "C" void* __SteamAPI_ISteamController_DeactivateActionSetLayer_ptr;
extern "C" void* __SteamAPI_ISteamController_DeactivateAllActionSetLayers_ptr;
extern "C" void* __SteamAPI_ISteamController_GetActionOriginFromXboxOrigin_ptr;
extern "C" void* __SteamAPI_ISteamController_GetActionSetHandle_ptr;
extern "C" void* __SteamAPI_ISteamController_GetActiveActionSetLayers_ptr;
extern "C" void* __SteamAPI_ISteamController_GetAnalogActionData_ptr;
extern "C" void* __SteamAPI_ISteamController_GetAnalogActionHandle_ptr;
extern "C" void* __SteamAPI_ISteamController_GetAnalogActionOrigins_ptr;
extern "C" void* __SteamAPI_ISteamController_GetConnectedControllers_ptr;
extern "C" void* __SteamAPI_ISteamController_GetControllerBindingRevision_ptr;
extern "C" void* __SteamAPI_ISteamController_GetControllerForGamepadIndex_ptr;
extern "C" void* __SteamAPI_ISteamController_GetCurrentActionSet_ptr;
extern "C" void* __SteamAPI_ISteamController_GetDigitalActionData_ptr;
extern "C" void* __SteamAPI_ISteamController_GetDigitalActionHandle_ptr;
extern "C" void* __SteamAPI_ISteamController_GetDigitalActionOrigins_ptr;
extern "C" void* __SteamAPI_ISteamController_GetGamepadIndexForController_ptr;
extern "C" void* __SteamAPI_ISteamController_GetGlyphForActionOrigin_ptr;
extern "C" void* __SteamAPI_ISteamController_GetGlyphForXboxOrigin_ptr;
extern "C" void* __SteamAPI_ISteamController_GetInputTypeForHandle_ptr;
extern "C" void* __SteamAPI_ISteamController_GetMotionData_ptr;
extern "C" void* __SteamAPI_ISteamController_GetStringForActionOrigin_ptr;
extern "C" void* __SteamAPI_ISteamController_GetStringForXboxOrigin_ptr;
extern "C" void* __SteamAPI_ISteamController_Init_ptr;
extern "C" void* __SteamAPI_ISteamController_RunFrame_ptr;
extern "C" void* __SteamAPI_ISteamController_SetLEDColor_ptr;
extern "C" void* __SteamAPI_ISteamController_ShowBindingPanel_ptr;
extern "C" void* __SteamAPI_ISteamController_Shutdown_ptr;
extern "C" void* __SteamAPI_ISteamController_StopAnalogActionMomentum_ptr;
extern "C" void* __SteamAPI_ISteamController_TranslateActionOrigin_ptr;
extern "C" void* __SteamAPI_ISteamController_TriggerHapticPulse_ptr;
extern "C" void* __SteamAPI_ISteamController_TriggerRepeatedHapticPulse_ptr;
extern "C" void* __SteamAPI_ISteamController_TriggerVibration_ptr;
extern "C" void* __SteamAPI_ISteamFriends_ActivateGameOverlay_ptr;
extern "C" void* __SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog_ptr;
extern "C" void* __SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialogConnectString_ptr;
extern "C" void* __SteamAPI_ISteamFriends_ActivateGameOverlayRemotePlayTogetherInviteDialog_ptr;
extern "C" void* __SteamAPI_ISteamFriends_ActivateGameOverlayToStore_ptr;
extern "C" void* __SteamAPI_ISteamFriends_ActivateGameOverlayToUser_ptr;
extern "C" void* __SteamAPI_ISteamFriends_ActivateGameOverlayToWebPage_ptr;
extern "C" void* __SteamAPI_ISteamFriends_BHasEquippedProfileItem_ptr;
extern "C" void* __SteamAPI_ISteamFriends_ClearRichPresence_ptr;
extern "C" void* __SteamAPI_ISteamFriends_CloseClanChatWindowInSteam_ptr;
extern "C" void* __SteamAPI_ISteamFriends_DownloadClanActivityCounts_ptr;
extern "C" void* __SteamAPI_ISteamFriends_EnumerateFollowingList_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetChatMemberByIndex_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetClanActivityCounts_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetClanByIndex_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetClanChatMemberCount_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetClanChatMessage_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetClanCount_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetClanName_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetClanOfficerByIndex_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetClanOfficerCount_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetClanOwner_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetClanTag_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetCoplayFriend_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetCoplayFriendCount_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFollowerCount_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendByIndex_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendCoplayGame_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendCoplayTime_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendCount_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendCountFromSource_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendFromSourceByIndex_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendGamePlayed_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendMessage_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendPersonaName_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendPersonaNameHistory_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendPersonaState_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendRelationship_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendRichPresence_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendRichPresenceKeyByIndex_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendRichPresenceKeyCount_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendSteamLevel_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendsGroupCount_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendsGroupIDByIndex_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendsGroupMembersCount_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendsGroupMembersList_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetFriendsGroupName_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetLargeFriendAvatar_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetMediumFriendAvatar_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetNumChatsWithUnreadPriorityMessages_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetPersonaName_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetPersonaState_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetPlayerNickname_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetProfileItemPropertyString_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetProfileItemPropertyUint_ptr;
extern "C" void* __SteamAPI_ISteamFriends_GetSmallFriendAvatar_ptr;
extern "C" void* __SteamAPI_ISteamFriends_HasFriend_ptr;
extern "C" void* __SteamAPI_ISteamFriends_InviteUserToGame_ptr;
extern "C" void* __SteamAPI_ISteamFriends_IsClanChatAdmin_ptr;
extern "C" void* __SteamAPI_ISteamFriends_IsClanChatWindowOpenInSteam_ptr;
extern "C" void* __SteamAPI_ISteamFriends_IsClanOfficialGameGroup_ptr;
extern "C" void* __SteamAPI_ISteamFriends_IsClanPublic_ptr;
extern "C" void* __SteamAPI_ISteamFriends_IsFollowing_ptr;
extern "C" void* __SteamAPI_ISteamFriends_IsUserInSource_ptr;
extern "C" void* __SteamAPI_ISteamFriends_JoinClanChatRoom_ptr;
extern "C" void* __SteamAPI_ISteamFriends_LeaveClanChatRoom_ptr;
extern "C" void* __SteamAPI_ISteamFriends_OpenClanChatWindowInSteam_ptr;
extern "C" void* __SteamAPI_ISteamFriends_RegisterProtocolInOverlayBrowser_ptr;
extern "C" void* __SteamAPI_ISteamFriends_ReplyToFriendMessage_ptr;
extern "C" void* __SteamAPI_ISteamFriends_RequestClanOfficerList_ptr;
extern "C" void* __SteamAPI_ISteamFriends_RequestEquippedProfileItems_ptr;
extern "C" void* __SteamAPI_ISteamFriends_RequestFriendRichPresence_ptr;
extern "C" void* __SteamAPI_ISteamFriends_RequestUserInformation_ptr;
extern "C" void* __SteamAPI_ISteamFriends_SendClanChatMessage_ptr;
extern "C" void* __SteamAPI_ISteamFriends_SetInGameVoiceSpeaking_ptr;
extern "C" void* __SteamAPI_ISteamFriends_SetListenForFriendsMessages_ptr;
extern "C" void* __SteamAPI_ISteamFriends_SetPlayedWith_ptr;
extern "C" void* __SteamAPI_ISteamFriends_SetRichPresence_ptr;
extern "C" void* __SteamAPI_ISteamGameSearch_AcceptGame_ptr;
extern "C" void* __SteamAPI_ISteamGameSearch_AddGameSearchParams_ptr;
extern "C" void* __SteamAPI_ISteamGameSearch_CancelRequestPlayersForGame_ptr;
extern "C" void* __SteamAPI_ISteamGameSearch_DeclineGame_ptr;
extern "C" void* __SteamAPI_ISteamGameSearch_EndGame_ptr;
extern "C" void* __SteamAPI_ISteamGameSearch_EndGameSearch_ptr;
extern "C" void* __SteamAPI_ISteamGameSearch_HostConfirmGameStart_ptr;
extern "C" void* __SteamAPI_ISteamGameSearch_RequestPlayersForGame_ptr;
extern "C" void* __SteamAPI_ISteamGameSearch_RetrieveConnectionDetails_ptr;
extern "C" void* __SteamAPI_ISteamGameSearch_SearchForGameSolo_ptr;
extern "C" void* __SteamAPI_ISteamGameSearch_SearchForGameWithLobby_ptr;
extern "C" void* __SteamAPI_ISteamGameSearch_SetConnectionDetails_ptr;
extern "C" void* __SteamAPI_ISteamGameSearch_SetGameHostParams_ptr;
extern "C" void* __SteamAPI_ISteamGameSearch_SubmitPlayerResult_ptr;
extern "C" void* __SteamAPI_ISteamGameServerStats_ClearUserAchievement_ptr;
extern "C" void* __SteamAPI_ISteamGameServerStats_GetUserAchievement_ptr;
extern "C" void* __SteamAPI_ISteamGameServerStats_GetUserStatFloat_ptr;
extern "C" void* __SteamAPI_ISteamGameServerStats_GetUserStatInt32_ptr;
extern "C" void* __SteamAPI_ISteamGameServerStats_RequestUserStats_ptr;
extern "C" void* __SteamAPI_ISteamGameServerStats_SetUserAchievement_ptr;
extern "C" void* __SteamAPI_ISteamGameServerStats_SetUserStatFloat_ptr;
extern "C" void* __SteamAPI_ISteamGameServerStats_SetUserStatInt32_ptr;
extern "C" void* __SteamAPI_ISteamGameServerStats_StoreUserStats_ptr;
extern "C" void* __SteamAPI_ISteamGameServerStats_UpdateUserAvgRateStat_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_AssociateWithClan_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_BLoggedOn_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_BSecure_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_BUpdateUserData_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_BeginAuthSession_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_CancelAuthTicket_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_ClearAllKeyValues_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_ComputeNewPlayerCompatibility_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_CreateUnauthenticatedUserConnection_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_EndAuthSession_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_GetAuthSessionTicket_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_GetGameplayStats_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_GetNextOutgoingPacket_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_GetPublicIP_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_GetServerReputation_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_GetSteamID_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_HandleIncomingPacket_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_LogOff_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_LogOn_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_LogOnAnonymous_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_RequestUserGroupStatus_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SendUserConnectAndAuthenticate_DEPRECATED_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SendUserDisconnect_DEPRECATED_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SetAdvertiseServerActive_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SetBotPlayerCount_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SetDedicatedServer_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SetGameData_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SetGameDescription_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SetGameTags_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SetKeyValue_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SetMapName_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SetMaxPlayerCount_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SetModDir_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SetPasswordProtected_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SetProduct_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SetRegion_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SetServerName_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SetSpectatorPort_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_SetSpectatorServerName_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_UserHasLicenseForApp_ptr;
extern "C" void* __SteamAPI_ISteamGameServer_WasRestartRequested_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_AddHeader_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_AllowStartRequest_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_CopyToClipboard_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_CreateBrowser_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_ExecuteJavascript_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_FileLoadDialogResponse_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_Find_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_GetLinkAtPosition_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_GoBack_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_GoForward_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_Init_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_JSDialogResponse_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_KeyChar_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_KeyDown_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_KeyUp_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_LoadURL_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_MouseDoubleClick_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_MouseDown_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_MouseMove_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_MouseUp_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_MouseWheel_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_OpenDeveloperTools_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_PasteFromClipboard_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_Reload_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_RemoveBrowser_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_SetBackgroundMode_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_SetCookie_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_SetDPIScalingFactor_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_SetHorizontalScroll_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_SetKeyFocus_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_SetPageScaleFactor_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_SetSize_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_SetVerticalScroll_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_Shutdown_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_StopFind_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_StopLoad_ptr;
extern "C" void* __SteamAPI_ISteamHTMLSurface_ViewSource_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_CreateCookieContainer_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_CreateHTTPRequest_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_DeferHTTPRequest_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_GetHTTPDownloadProgressPct_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_GetHTTPRequestWasTimedOut_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_GetHTTPResponseBodyData_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_GetHTTPResponseBodySize_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_GetHTTPResponseHeaderSize_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_GetHTTPResponseHeaderValue_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_GetHTTPStreamingResponseBodyData_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_PrioritizeHTTPRequest_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_ReleaseCookieContainer_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_ReleaseHTTPRequest_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_SendHTTPRequest_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_SendHTTPRequestAndStreamResponse_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_SetCookie_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_SetHTTPRequestAbsoluteTimeoutMS_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_SetHTTPRequestContextValue_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_SetHTTPRequestCookieContainer_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_SetHTTPRequestGetOrPostParameter_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_SetHTTPRequestHeaderValue_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_SetHTTPRequestNetworkActivityTimeout_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_SetHTTPRequestRawPostBody_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_SetHTTPRequestRequiresVerifiedCertificate_ptr;
extern "C" void* __SteamAPI_ISteamHTTP_SetHTTPRequestUserAgentInfo_ptr;
extern "C" void* __SteamAPI_ISteamInput_ActivateActionSet_ptr;
extern "C" void* __SteamAPI_ISteamInput_ActivateActionSetLayer_ptr;
extern "C" void* __SteamAPI_ISteamInput_BNewDataAvailable_ptr;
extern "C" void* __SteamAPI_ISteamInput_BWaitForData_ptr;
extern "C" void* __SteamAPI_ISteamInput_DeactivateActionSetLayer_ptr;
extern "C" void* __SteamAPI_ISteamInput_DeactivateAllActionSetLayers_ptr;
extern "C" void* __SteamAPI_ISteamInput_EnableActionEventCallbacks_ptr;
extern "C" void* __SteamAPI_ISteamInput_EnableDeviceCallbacks_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetActionOriginFromXboxOrigin_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetActionSetHandle_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetActiveActionSetLayers_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetAnalogActionData_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetAnalogActionHandle_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetAnalogActionOrigins_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetConnectedControllers_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetControllerForGamepadIndex_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetCurrentActionSet_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetDeviceBindingRevision_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetDigitalActionData_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetDigitalActionHandle_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetDigitalActionOrigins_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetGamepadIndexForController_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetGlyphForActionOrigin_Legacy_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetGlyphForXboxOrigin_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetGlyphPNGForActionOrigin_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetGlyphSVGForActionOrigin_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetInputTypeForHandle_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetMotionData_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetRemotePlaySessionID_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetSessionInputConfigurationSettings_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetStringForActionOrigin_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetStringForAnalogActionName_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetStringForDigitalActionName_ptr;
extern "C" void* __SteamAPI_ISteamInput_GetStringForXboxOrigin_ptr;
extern "C" void* __SteamAPI_ISteamInput_Init_ptr;
extern "C" void* __SteamAPI_ISteamInput_Legacy_TriggerHapticPulse_ptr;
extern "C" void* __SteamAPI_ISteamInput_Legacy_TriggerRepeatedHapticPulse_ptr;
extern "C" void* __SteamAPI_ISteamInput_RunFrame_ptr;
extern "C" void* __SteamAPI_ISteamInput_SetDualSenseTriggerEffect_ptr;
extern "C" void* __SteamAPI_ISteamInput_SetInputActionManifestFilePath_ptr;
extern "C" void* __SteamAPI_ISteamInput_SetLEDColor_ptr;
extern "C" void* __SteamAPI_ISteamInput_ShowBindingPanel_ptr;
extern "C" void* __SteamAPI_ISteamInput_Shutdown_ptr;
extern "C" void* __SteamAPI_ISteamInput_StopAnalogActionMomentum_ptr;
extern "C" void* __SteamAPI_ISteamInput_TranslateActionOrigin_ptr;
extern "C" void* __SteamAPI_ISteamInput_TriggerSimpleHapticEvent_ptr;
extern "C" void* __SteamAPI_ISteamInput_TriggerVibration_ptr;
extern "C" void* __SteamAPI_ISteamInput_TriggerVibrationExtended_ptr;
extern "C" void* __SteamAPI_ISteamInventory_AddPromoItem_ptr;
extern "C" void* __SteamAPI_ISteamInventory_AddPromoItems_ptr;
extern "C" void* __SteamAPI_ISteamInventory_CheckResultSteamID_ptr;
extern "C" void* __SteamAPI_ISteamInventory_ConsumeItem_ptr;
extern "C" void* __SteamAPI_ISteamInventory_DeserializeResult_ptr;
extern "C" void* __SteamAPI_ISteamInventory_DestroyResult_ptr;
extern "C" void* __SteamAPI_ISteamInventory_ExchangeItems_ptr;
extern "C" void* __SteamAPI_ISteamInventory_GenerateItems_ptr;
extern "C" void* __SteamAPI_ISteamInventory_GetAllItems_ptr;
extern "C" void* __SteamAPI_ISteamInventory_GetEligiblePromoItemDefinitionIDs_ptr;
extern "C" void* __SteamAPI_ISteamInventory_GetItemDefinitionIDs_ptr;
extern "C" void* __SteamAPI_ISteamInventory_GetItemDefinitionProperty_ptr;
extern "C" void* __SteamAPI_ISteamInventory_GetItemPrice_ptr;
extern "C" void* __SteamAPI_ISteamInventory_GetItemsByID_ptr;
extern "C" void* __SteamAPI_ISteamInventory_GetItemsWithPrices_ptr;
extern "C" void* __SteamAPI_ISteamInventory_GetNumItemsWithPrices_ptr;
extern "C" void* __SteamAPI_ISteamInventory_GetResultItemProperty_ptr;
extern "C" void* __SteamAPI_ISteamInventory_GetResultItems_ptr;
extern "C" void* __SteamAPI_ISteamInventory_GetResultStatus_ptr;
extern "C" void* __SteamAPI_ISteamInventory_GetResultTimestamp_ptr;
extern "C" void* __SteamAPI_ISteamInventory_GrantPromoItems_ptr;
extern "C" void* __SteamAPI_ISteamInventory_InspectItem_ptr;
extern "C" void* __SteamAPI_ISteamInventory_LoadItemDefinitions_ptr;
extern "C" void* __SteamAPI_ISteamInventory_RemoveProperty_ptr;
extern "C" void* __SteamAPI_ISteamInventory_RequestEligiblePromoItemDefinitionsIDs_ptr;
extern "C" void* __SteamAPI_ISteamInventory_RequestPrices_ptr;
extern "C" void* __SteamAPI_ISteamInventory_SendItemDropHeartbeat_ptr;
extern "C" void* __SteamAPI_ISteamInventory_SerializeResult_ptr;
extern "C" void* __SteamAPI_ISteamInventory_SetPropertyBool_ptr;
extern "C" void* __SteamAPI_ISteamInventory_SetPropertyFloat_ptr;
extern "C" void* __SteamAPI_ISteamInventory_SetPropertyInt64_ptr;
extern "C" void* __SteamAPI_ISteamInventory_SetPropertyString_ptr;
extern "C" void* __SteamAPI_ISteamInventory_StartPurchase_ptr;
extern "C" void* __SteamAPI_ISteamInventory_StartUpdateProperties_ptr;
extern "C" void* __SteamAPI_ISteamInventory_SubmitUpdateProperties_ptr;
extern "C" void* __SteamAPI_ISteamInventory_TradeItems_ptr;
extern "C" void* __SteamAPI_ISteamInventory_TransferItemQuantity_ptr;
extern "C" void* __SteamAPI_ISteamInventory_TriggerItemDrop_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingPingResponse_ServerFailedToRespond_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingPingResponse_ServerResponded_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingPlayersResponse_AddPlayerToList_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingPlayersResponse_PlayersFailedToRespond_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingPlayersResponse_PlayersRefreshComplete_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingRulesResponse_RulesFailedToRespond_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingRulesResponse_RulesRefreshComplete_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingRulesResponse_RulesResponded_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServerListResponse_RefreshComplete_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServerListResponse_ServerFailedToRespond_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServerListResponse_ServerResponded_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_CancelQuery_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_CancelServerQuery_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_GetServerCount_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_GetServerDetails_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_IsRefreshing_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_PingServer_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_PlayerDetails_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_RefreshQuery_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_RefreshServer_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_ReleaseRequest_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_RequestFavoritesServerList_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_RequestFriendsServerList_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_RequestHistoryServerList_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_RequestInternetServerList_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_RequestLANServerList_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_RequestSpectatorServerList_ptr;
extern "C" void* __SteamAPI_ISteamMatchmakingServers_ServerRules_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_AddFavoriteGame_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_AddRequestLobbyListCompatibleMembersFilter_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_AddRequestLobbyListFilterSlotsAvailable_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_AddRequestLobbyListNearValueFilter_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_AddRequestLobbyListNumericalFilter_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_CreateLobby_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_DeleteLobbyData_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_GetFavoriteGame_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_GetFavoriteGameCount_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_GetLobbyByIndex_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_GetLobbyChatEntry_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_GetLobbyData_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_GetLobbyDataCount_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_GetLobbyGameServer_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_GetLobbyMemberData_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_GetLobbyOwner_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_GetNumLobbyMembers_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_InviteUserToLobby_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_JoinLobby_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_LeaveLobby_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_RemoveFavoriteGame_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_RequestLobbyData_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_RequestLobbyList_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_SendLobbyChatMsg_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_SetLinkedLobby_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_SetLobbyData_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_SetLobbyGameServer_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_SetLobbyJoinable_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_SetLobbyMemberData_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_SetLobbyMemberLimit_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_SetLobbyOwner_ptr;
extern "C" void* __SteamAPI_ISteamMatchmaking_SetLobbyType_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_BActivationSuccess_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_BIsCurrentMusicRemote_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_CurrentEntryDidChange_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_CurrentEntryIsAvailable_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_CurrentEntryWillChange_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_DeregisterSteamMusicRemote_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_EnableLooped_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_EnablePlayNext_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_EnablePlayPrevious_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_EnablePlaylists_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_EnableQueue_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_EnableShuffled_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_PlaylistDidChange_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_PlaylistWillChange_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_QueueDidChange_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_QueueWillChange_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_RegisterSteamMusicRemote_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_ResetPlaylistEntries_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_ResetQueueEntries_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_SetCurrentPlaylistEntry_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_SetCurrentQueueEntry_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_SetDisplayName_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_SetPNGIcon_64x64_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_SetPlaylistEntry_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_SetQueueEntry_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_UpdateCurrentEntryCoverArt_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_UpdateCurrentEntryElapsedSeconds_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_UpdateCurrentEntryText_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_UpdateLooped_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_UpdatePlaybackStatus_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_UpdateShuffled_ptr;
extern "C" void* __SteamAPI_ISteamMusicRemote_UpdateVolume_ptr;
extern "C" void* __SteamAPI_ISteamMusic_BIsEnabled_ptr;
extern "C" void* __SteamAPI_ISteamMusic_BIsPlaying_ptr;
extern "C" void* __SteamAPI_ISteamMusic_GetPlaybackStatus_ptr;
extern "C" void* __SteamAPI_ISteamMusic_GetVolume_ptr;
extern "C" void* __SteamAPI_ISteamMusic_Pause_ptr;
extern "C" void* __SteamAPI_ISteamMusic_Play_ptr;
extern "C" void* __SteamAPI_ISteamMusic_PlayNext_ptr;
extern "C" void* __SteamAPI_ISteamMusic_PlayPrevious_ptr;
extern "C" void* __SteamAPI_ISteamMusic_SetVolume_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingFakeUDPPort_DestroyFakeUDPPort_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingFakeUDPPort_ReceiveMessages_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingFakeUDPPort_ScheduleCleanup_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingFakeUDPPort_SendMessageToFakeIP_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingMessages_CloseChannelWithUser_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingMessages_SendMessageToUser_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_AcceptConnection_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_BeginAsyncRequestFakeIP_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_CloseConnection_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_CloseListenSocket_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_ConfigureConnectionLanes_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_ConnectByIPAddress_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_ConnectP2P_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_ConnectP2PCustomSignaling_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_ConnectToHostedDedicatedServer_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_CreateFakeUDPPort_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_CreateHostedDedicatedServerListenSocket_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_CreateListenSocketIP_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2PFakeIP_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_CreatePollGroup_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_CreateSocketPair_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_DestroyPollGroup_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_FindRelayAuthTicketForServer_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_FlushMessagesOnConnection_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_GetAuthenticationStatus_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_GetCertificateRequest_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_GetConnectionInfo_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_GetConnectionName_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_GetConnectionRealTimeStatus_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_GetConnectionUserData_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_GetDetailedConnectionStatus_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_GetFakeIP_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_GetGameCoordinatorServerLogin_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerAddress_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPOPID_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPort_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_GetIdentity_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_GetListenSocketAddress_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_GetRemoteFakeIPForConnection_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_InitAuthentication_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnPollGroup_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_ReceivedP2PCustomSignal_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_ReceivedRelayAuthTicket_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_ResetIdentity_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_RunCallbacks_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_SendMessageToConnection_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_SendMessages_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_SetCertificate_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_SetConnectionName_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_SetConnectionPollGroup_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingSockets_SetConnectionUserData_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_AllocateMessage_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_CheckPingDataUpToDate_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_ConvertPingLocationToString_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_EstimatePingTimeBetweenTwoLocations_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_EstimatePingTimeFromLocalHost_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_GetConfigValue_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_GetConfigValueInfo_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_GetDirectPingToPOP_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_GetIPv4FakeIPType_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_GetLocalPingLocation_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_GetLocalTimestamp_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_GetPOPCount_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_GetPOPList_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_GetPingToDataCenter_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_GetRealIdentityForFakeIP_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_IsFakeIPv4_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_IterateGenericEditableConfigValues_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_ParsePingLocationString_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SetConfigValue_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SetConfigValueStruct_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueFloat_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueInt32_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueString_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SetDebugOutputFunction_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_FakeIPResult_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionFailed_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionRequest_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetAuthenticationStatusChanged_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetConnectionStatusChanged_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamRelayNetworkStatusChanged_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueFloat_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValuePtr_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueString_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_GetFakeIPType_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ParseString_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ToString_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ParseString_ptr;
extern "C" void* __SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ToString_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_AllowP2PPacketRelay_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_CloseP2PChannelWithUser_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_CloseP2PSessionWithUser_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_CreateConnectionSocket_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_CreateListenSocket_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_CreateP2PConnectionSocket_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_DestroyListenSocket_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_DestroySocket_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_GetListenSocketInfo_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_GetMaxPacketSize_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_GetP2PSessionState_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_GetSocketConnectionType_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_GetSocketInfo_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_IsDataAvailable_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_IsDataAvailableOnSocket_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_IsP2PPacketAvailable_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_ReadP2PPacket_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_RetrieveData_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_RetrieveDataFromSocket_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_SendDataOnSocket_ptr;
extern "C" void* __SteamAPI_ISteamNetworking_SendP2PPacket_ptr;
extern "C" void* __SteamAPI_ISteamParentalSettings_BIsAppBlocked_ptr;
extern "C" void* __SteamAPI_ISteamParentalSettings_BIsAppInBlockList_ptr;
extern "C" void* __SteamAPI_ISteamParentalSettings_BIsFeatureBlocked_ptr;
extern "C" void* __SteamAPI_ISteamParentalSettings_BIsFeatureInBlockList_ptr;
extern "C" void* __SteamAPI_ISteamParentalSettings_BIsParentalLockEnabled_ptr;
extern "C" void* __SteamAPI_ISteamParentalSettings_BIsParentalLockLocked_ptr;
extern "C" void* __SteamAPI_ISteamParties_CancelReservation_ptr;
extern "C" void* __SteamAPI_ISteamParties_ChangeNumOpenSlots_ptr;
extern "C" void* __SteamAPI_ISteamParties_CreateBeacon_ptr;
extern "C" void* __SteamAPI_ISteamParties_DestroyBeacon_ptr;
extern "C" void* __SteamAPI_ISteamParties_GetAvailableBeaconLocations_ptr;
extern "C" void* __SteamAPI_ISteamParties_GetBeaconByIndex_ptr;
extern "C" void* __SteamAPI_ISteamParties_GetBeaconDetails_ptr;
extern "C" void* __SteamAPI_ISteamParties_GetBeaconLocationData_ptr;
extern "C" void* __SteamAPI_ISteamParties_GetNumActiveBeacons_ptr;
extern "C" void* __SteamAPI_ISteamParties_GetNumAvailableBeaconLocations_ptr;
extern "C" void* __SteamAPI_ISteamParties_JoinParty_ptr;
extern "C" void* __SteamAPI_ISteamParties_OnReservationCompleted_ptr;
extern "C" void* __SteamAPI_ISteamRemotePlay_BEnableRemotePlayTogetherDirectInput_ptr;
extern "C" void* __SteamAPI_ISteamRemotePlay_BGetSessionClientResolution_ptr;
extern "C" void* __SteamAPI_ISteamRemotePlay_BSendRemotePlayTogetherInvite_ptr;
extern "C" void* __SteamAPI_ISteamRemotePlay_CreateMouseCursor_ptr;
extern "C" void* __SteamAPI_ISteamRemotePlay_DisableRemotePlayTogetherDirectInput_ptr;
extern "C" void* __SteamAPI_ISteamRemotePlay_GetInput_ptr;
extern "C" void* __SteamAPI_ISteamRemotePlay_GetSessionClientFormFactor_ptr;
extern "C" void* __SteamAPI_ISteamRemotePlay_GetSessionClientName_ptr;
extern "C" void* __SteamAPI_ISteamRemotePlay_GetSessionCount_ptr;
extern "C" void* __SteamAPI_ISteamRemotePlay_GetSessionID_ptr;
extern "C" void* __SteamAPI_ISteamRemotePlay_GetSessionSteamID_ptr;
extern "C" void* __SteamAPI_ISteamRemotePlay_SetMouseCursor_ptr;
extern "C" void* __SteamAPI_ISteamRemotePlay_SetMousePosition_ptr;
extern "C" void* __SteamAPI_ISteamRemotePlay_SetMouseVisibility_ptr;
extern "C" void* __SteamAPI_ISteamRemotePlay_ShowRemotePlayTogetherUI_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_BeginFileWriteBatch_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_CommitPublishedFileUpdate_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_CreatePublishedFileUpdateRequest_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_DeletePublishedFile_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_EndFileWriteBatch_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_EnumeratePublishedFilesByUserAction_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_EnumeratePublishedWorkshopFiles_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_EnumerateUserPublishedFiles_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_EnumerateUserSharedWorkshopFiles_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_EnumerateUserSubscribedFiles_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_FileDelete_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_FileExists_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_FileForget_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_FilePersisted_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_FileRead_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_FileReadAsync_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_FileReadAsyncComplete_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_FileShare_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_FileWrite_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_FileWriteAsync_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_FileWriteStreamCancel_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_FileWriteStreamClose_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_FileWriteStreamOpen_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_FileWriteStreamWriteChunk_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_GetCachedUGCCount_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_GetCachedUGCHandle_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_GetFileCount_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_GetFileNameAndSize_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_GetFileSize_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_GetFileTimestamp_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_GetLocalFileChange_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_GetLocalFileChangeCount_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_GetPublishedFileDetails_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_GetPublishedItemVoteDetails_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_GetQuota_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_GetSyncPlatforms_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_GetUGCDetails_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_GetUGCDownloadProgress_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_GetUserPublishedItemVoteDetails_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_IsCloudEnabledForAccount_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_IsCloudEnabledForApp_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_PublishVideo_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_PublishWorkshopFile_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_SetCloudEnabledForApp_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_SetSyncPlatforms_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_SetUserPublishedFileAction_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_SubscribePublishedFile_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_UGCDownload_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_UGCDownloadToLocation_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_UGCRead_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_UnsubscribePublishedFile_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_UpdatePublishedFileDescription_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_UpdatePublishedFileFile_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_UpdatePublishedFilePreviewFile_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_UpdatePublishedFileSetChangeDescription_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTags_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTitle_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_UpdatePublishedFileVisibility_ptr;
extern "C" void* __SteamAPI_ISteamRemoteStorage_UpdateUserPublishedItemVote_ptr;
extern "C" void* __SteamAPI_ISteamScreenshots_AddScreenshotToLibrary_ptr;
extern "C" void* __SteamAPI_ISteamScreenshots_AddVRScreenshotToLibrary_ptr;
extern "C" void* __SteamAPI_ISteamScreenshots_HookScreenshots_ptr;
extern "C" void* __SteamAPI_ISteamScreenshots_IsScreenshotsHooked_ptr;
extern "C" void* __SteamAPI_ISteamScreenshots_SetLocation_ptr;
extern "C" void* __SteamAPI_ISteamScreenshots_TagPublishedFile_ptr;
extern "C" void* __SteamAPI_ISteamScreenshots_TagUser_ptr;
extern "C" void* __SteamAPI_ISteamScreenshots_TriggerScreenshot_ptr;
extern "C" void* __SteamAPI_ISteamScreenshots_WriteScreenshot_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_AddGamePhaseTag_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_AddInstantaneousTimelineEvent_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_AddRangeTimelineEvent_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_ClearTimelineTooltip_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_DoesEventRecordingExist_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_DoesGamePhaseRecordingExist_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_EndGamePhase_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_EndRangeTimelineEvent_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_OpenOverlayToGamePhase_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_OpenOverlayToTimelineEvent_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_RemoveTimelineEvent_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_SetGamePhaseAttribute_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_SetGamePhaseID_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_SetTimelineGameMode_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_SetTimelineTooltip_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_StartGamePhase_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_StartRangeTimelineEvent_ptr;
extern "C" void* __SteamAPI_ISteamTimeline_UpdateRangeTimelineEvent_ptr;
extern "C" void* __SteamAPI_ISteamUGC_AddAppDependency_ptr;
extern "C" void* __SteamAPI_ISteamUGC_AddContentDescriptor_ptr;
extern "C" void* __SteamAPI_ISteamUGC_AddDependency_ptr;
extern "C" void* __SteamAPI_ISteamUGC_AddExcludedTag_ptr;
extern "C" void* __SteamAPI_ISteamUGC_AddItemKeyValueTag_ptr;
extern "C" void* __SteamAPI_ISteamUGC_AddItemPreviewFile_ptr;
extern "C" void* __SteamAPI_ISteamUGC_AddItemPreviewVideo_ptr;
extern "C" void* __SteamAPI_ISteamUGC_AddItemToFavorites_ptr;
extern "C" void* __SteamAPI_ISteamUGC_AddRequiredKeyValueTag_ptr;
extern "C" void* __SteamAPI_ISteamUGC_AddRequiredTag_ptr;
extern "C" void* __SteamAPI_ISteamUGC_AddRequiredTagGroup_ptr;
extern "C" void* __SteamAPI_ISteamUGC_BInitWorkshopForGameServer_ptr;
extern "C" void* __SteamAPI_ISteamUGC_CreateItem_ptr;
extern "C" void* __SteamAPI_ISteamUGC_CreateQueryAllUGCRequestCursor_ptr;
extern "C" void* __SteamAPI_ISteamUGC_CreateQueryAllUGCRequestPage_ptr;
extern "C" void* __SteamAPI_ISteamUGC_CreateQueryUGCDetailsRequest_ptr;
extern "C" void* __SteamAPI_ISteamUGC_CreateQueryUserUGCRequest_ptr;
extern "C" void* __SteamAPI_ISteamUGC_DeleteItem_ptr;
extern "C" void* __SteamAPI_ISteamUGC_DownloadItem_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetAppDependencies_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetItemDownloadInfo_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetItemInstallInfo_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetItemState_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetItemUpdateProgress_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetNumSubscribedItems_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetNumSupportedGameVersions_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetQueryFirstUGCKeyValueTag_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetQueryUGCAdditionalPreview_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetQueryUGCChildren_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetQueryUGCContentDescriptors_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetQueryUGCKeyValueTag_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetQueryUGCMetadata_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetQueryUGCNumAdditionalPreviews_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetQueryUGCNumKeyValueTags_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetQueryUGCNumTags_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetQueryUGCPreviewURL_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetQueryUGCResult_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetQueryUGCStatistic_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetQueryUGCTag_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetQueryUGCTagDisplayName_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetSubscribedItems_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetSupportedGameVersionData_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetUserContentDescriptorPreferences_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetUserItemVote_ptr;
extern "C" void* __SteamAPI_ISteamUGC_GetWorkshopEULAStatus_ptr;
extern "C" void* __SteamAPI_ISteamUGC_ReleaseQueryUGCRequest_ptr;
extern "C" void* __SteamAPI_ISteamUGC_RemoveAllItemKeyValueTags_ptr;
extern "C" void* __SteamAPI_ISteamUGC_RemoveAppDependency_ptr;
extern "C" void* __SteamAPI_ISteamUGC_RemoveContentDescriptor_ptr;
extern "C" void* __SteamAPI_ISteamUGC_RemoveDependency_ptr;
extern "C" void* __SteamAPI_ISteamUGC_RemoveItemFromFavorites_ptr;
extern "C" void* __SteamAPI_ISteamUGC_RemoveItemKeyValueTags_ptr;
extern "C" void* __SteamAPI_ISteamUGC_RemoveItemPreview_ptr;
extern "C" void* __SteamAPI_ISteamUGC_RequestUGCDetails_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SendQueryUGCRequest_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetAdminQuery_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetAllowCachedResponse_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetAllowLegacyUpload_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetCloudFileNameFilter_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetItemContent_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetItemDescription_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetItemMetadata_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetItemPreview_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetItemTags_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetItemTitle_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetItemUpdateLanguage_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetItemVisibility_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetItemsDisabledLocally_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetLanguage_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetMatchAnyTag_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetRankedByTrendDays_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetRequiredGameVersions_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetReturnAdditionalPreviews_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetReturnChildren_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetReturnKeyValueTags_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetReturnLongDescription_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetReturnMetadata_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetReturnOnlyIDs_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetReturnPlaytimeStats_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetReturnTotalOnly_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetSearchText_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetSubscriptionsLoadOrder_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetTimeCreatedDateRange_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetTimeUpdatedDateRange_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SetUserItemVote_ptr;
extern "C" void* __SteamAPI_ISteamUGC_ShowWorkshopEULA_ptr;
extern "C" void* __SteamAPI_ISteamUGC_StartItemUpdate_ptr;
extern "C" void* __SteamAPI_ISteamUGC_StartPlaytimeTracking_ptr;
extern "C" void* __SteamAPI_ISteamUGC_StopPlaytimeTracking_ptr;
extern "C" void* __SteamAPI_ISteamUGC_StopPlaytimeTrackingForAllItems_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SubmitItemUpdate_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SubscribeItem_ptr;
extern "C" void* __SteamAPI_ISteamUGC_SuspendDownloads_ptr;
extern "C" void* __SteamAPI_ISteamUGC_UnsubscribeItem_ptr;
extern "C" void* __SteamAPI_ISteamUGC_UpdateItemPreviewFile_ptr;
extern "C" void* __SteamAPI_ISteamUGC_UpdateItemPreviewVideo_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_AttachLeaderboardUGC_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_ClearAchievement_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_DownloadLeaderboardEntries_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_DownloadLeaderboardEntriesForUsers_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_FindLeaderboard_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_FindOrCreateLeaderboard_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetAchievement_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetAchievementAchievedPercent_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetAchievementAndUnlockTime_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetAchievementIcon_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetAchievementName_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetAchievementProgressLimitsFloat_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetAchievementProgressLimitsInt32_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetGlobalStatDouble_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetGlobalStatHistoryDouble_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetGlobalStatHistoryInt64_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetGlobalStatInt64_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetLeaderboardDisplayType_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetLeaderboardEntryCount_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetLeaderboardName_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetLeaderboardSortMethod_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetMostAchievedAchievementInfo_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetNextMostAchievedAchievementInfo_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetNumAchievements_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetNumberOfCurrentPlayers_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetStatFloat_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetStatInt32_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetUserAchievement_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetUserAchievementAndUnlockTime_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetUserStatFloat_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_GetUserStatInt32_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_IndicateAchievementProgress_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_RequestGlobalAchievementPercentages_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_RequestGlobalStats_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_RequestUserStats_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_ResetAllStats_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_SetAchievement_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_SetStatFloat_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_SetStatInt32_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_StoreStats_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_UpdateAvgRateStat_ptr;
extern "C" void* __SteamAPI_ISteamUserStats_UploadLeaderboardScore_ptr;
extern "C" void* __SteamAPI_ISteamUser_AdvertiseGame_ptr;
extern "C" void* __SteamAPI_ISteamUser_BIsBehindNAT_ptr;
extern "C" void* __SteamAPI_ISteamUser_BIsPhoneIdentifying_ptr;
extern "C" void* __SteamAPI_ISteamUser_BIsPhoneRequiringVerification_ptr;
extern "C" void* __SteamAPI_ISteamUser_BIsPhoneVerified_ptr;
extern "C" void* __SteamAPI_ISteamUser_BIsTwoFactorEnabled_ptr;
extern "C" void* __SteamAPI_ISteamUser_BLoggedOn_ptr;
extern "C" void* __SteamAPI_ISteamUser_BSetDurationControlOnlineState_ptr;
extern "C" void* __SteamAPI_ISteamUser_BeginAuthSession_ptr;
extern "C" void* __SteamAPI_ISteamUser_CancelAuthTicket_ptr;
extern "C" void* __SteamAPI_ISteamUser_DecompressVoice_ptr;
extern "C" void* __SteamAPI_ISteamUser_EndAuthSession_ptr;
extern "C" void* __SteamAPI_ISteamUser_GetAuthSessionTicket_ptr;
extern "C" void* __SteamAPI_ISteamUser_GetAuthTicketForWebApi_ptr;
extern "C" void* __SteamAPI_ISteamUser_GetAvailableVoice_ptr;
extern "C" void* __SteamAPI_ISteamUser_GetDurationControl_ptr;
extern "C" void* __SteamAPI_ISteamUser_GetEncryptedAppTicket_ptr;
extern "C" void* __SteamAPI_ISteamUser_GetGameBadgeLevel_ptr;
extern "C" void* __SteamAPI_ISteamUser_GetHSteamUser_ptr;
extern "C" void* __SteamAPI_ISteamUser_GetMarketEligibility_ptr;
extern "C" void* __SteamAPI_ISteamUser_GetPlayerSteamLevel_ptr;
extern "C" void* __SteamAPI_ISteamUser_GetSteamID_ptr;
extern "C" void* __SteamAPI_ISteamUser_GetUserDataFolder_ptr;
extern "C" void* __SteamAPI_ISteamUser_GetVoice_ptr;
extern "C" void* __SteamAPI_ISteamUser_GetVoiceOptimalSampleRate_ptr;
extern "C" void* __SteamAPI_ISteamUser_InitiateGameConnection_DEPRECATED_ptr;
extern "C" void* __SteamAPI_ISteamUser_RequestEncryptedAppTicket_ptr;
extern "C" void* __SteamAPI_ISteamUser_RequestStoreAuthURL_ptr;
extern "C" void* __SteamAPI_ISteamUser_StartVoiceRecording_ptr;
extern "C" void* __SteamAPI_ISteamUser_StopVoiceRecording_ptr;
extern "C" void* __SteamAPI_ISteamUser_TerminateGameConnection_DEPRECATED_ptr;
extern "C" void* __SteamAPI_ISteamUser_TrackAppUsageEvent_ptr;
extern "C" void* __SteamAPI_ISteamUser_UserHasLicenseForApp_ptr;
extern "C" void* __SteamAPI_ISteamUtils_BOverlayNeedsPresent_ptr;
extern "C" void* __SteamAPI_ISteamUtils_CheckFileSignature_ptr;
extern "C" void* __SteamAPI_ISteamUtils_DismissFloatingGamepadTextInput_ptr;
extern "C" void* __SteamAPI_ISteamUtils_DismissGamepadTextInput_ptr;
extern "C" void* __SteamAPI_ISteamUtils_FilterText_ptr;
extern "C" void* __SteamAPI_ISteamUtils_GetAPICallFailureReason_ptr;
extern "C" void* __SteamAPI_ISteamUtils_GetAPICallResult_ptr;
extern "C" void* __SteamAPI_ISteamUtils_GetAppID_ptr;
extern "C" void* __SteamAPI_ISteamUtils_GetConnectedUniverse_ptr;
extern "C" void* __SteamAPI_ISteamUtils_GetCurrentBatteryPower_ptr;
extern "C" void* __SteamAPI_ISteamUtils_GetEnteredGamepadTextInput_ptr;
extern "C" void* __SteamAPI_ISteamUtils_GetEnteredGamepadTextLength_ptr;
extern "C" void* __SteamAPI_ISteamUtils_GetIPCCallCount_ptr;
extern "C" void* __SteamAPI_ISteamUtils_GetIPCountry_ptr;
extern "C" void* __SteamAPI_ISteamUtils_GetIPv6ConnectivityState_ptr;
extern "C" void* __SteamAPI_ISteamUtils_GetImageRGBA_ptr;
extern "C" void* __SteamAPI_ISteamUtils_GetImageSize_ptr;
extern "C" void* __SteamAPI_ISteamUtils_GetSecondsSinceAppActive_ptr;
extern "C" void* __SteamAPI_ISteamUtils_GetSecondsSinceComputerActive_ptr;
extern "C" void* __SteamAPI_ISteamUtils_GetServerRealTime_ptr;
extern "C" void* __SteamAPI_ISteamUtils_GetSteamUILanguage_ptr;
extern "C" void* __SteamAPI_ISteamUtils_InitFilterText_ptr;
extern "C" void* __SteamAPI_ISteamUtils_IsAPICallCompleted_ptr;
extern "C" void* __SteamAPI_ISteamUtils_IsOverlayEnabled_ptr;
extern "C" void* __SteamAPI_ISteamUtils_IsSteamChinaLauncher_ptr;
extern "C" void* __SteamAPI_ISteamUtils_IsSteamInBigPictureMode_ptr;
extern "C" void* __SteamAPI_ISteamUtils_IsSteamRunningInVR_ptr;
extern "C" void* __SteamAPI_ISteamUtils_IsSteamRunningOnSteamDeck_ptr;
extern "C" void* __SteamAPI_ISteamUtils_IsVRHeadsetStreamingEnabled_ptr;
extern "C" void* __SteamAPI_ISteamUtils_SetGameLauncherMode_ptr;
extern "C" void* __SteamAPI_ISteamUtils_SetOverlayNotificationInset_ptr;
extern "C" void* __SteamAPI_ISteamUtils_SetOverlayNotificationPosition_ptr;
extern "C" void* __SteamAPI_ISteamUtils_SetVRHeadsetStreamingEnabled_ptr;
extern "C" void* __SteamAPI_ISteamUtils_SetWarningMessageHook_ptr;
extern "C" void* __SteamAPI_ISteamUtils_ShowFloatingGamepadTextInput_ptr;
extern "C" void* __SteamAPI_ISteamUtils_ShowGamepadTextInput_ptr;
extern "C" void* __SteamAPI_ISteamUtils_StartVRDashboard_ptr;
extern "C" void* __SteamAPI_ISteamVideo_GetOPFSettings_ptr;
extern "C" void* __SteamAPI_ISteamVideo_GetOPFStringForApp_ptr;
extern "C" void* __SteamAPI_ISteamVideo_GetVideoURL_ptr;
extern "C" void* __SteamAPI_ISteamVideo_IsBroadcasting_ptr;
extern "C" void* __SteamAPI_InitAnonymousUser_ptr;
extern "C" void* __SteamAPI_InitFlat_ptr;
extern "C" void* __SteamAPI_InitSafe_ptr;
extern "C" void* __SteamAPI_IsSteamRunning_ptr;
extern "C" void* __SteamAPI_ManualDispatch_FreeLastCallback_ptr;
extern "C" void* __SteamAPI_ManualDispatch_GetAPICallResult_ptr;
extern "C" void* __SteamAPI_ManualDispatch_GetNextCallback_ptr;
extern "C" void* __SteamAPI_ManualDispatch_Init_ptr;
extern "C" void* __SteamAPI_ManualDispatch_RunFrame_ptr;
extern "C" void* __SteamAPI_MatchMakingKeyValuePair_t_Construct_ptr;
extern "C" void* __SteamAPI_RegisterCallResult_ptr;
extern "C" void* __SteamAPI_RegisterCallback_ptr;
extern "C" void* __SteamAPI_ReleaseCurrentThreadMemory_ptr;
extern "C" void* __SteamAPI_RestartAppIfNecessary_ptr;
extern "C" void* __SteamAPI_SetBreakpadAppID_ptr;
extern "C" void* __SteamAPI_SetMiniDumpComment_ptr;
extern "C" void* __SteamAPI_SetTryCatchCallbacks_ptr;
extern "C" void* __SteamAPI_Shutdown_ptr;
extern "C" void* __SteamAPI_SteamApps_v008_ptr;
extern "C" void* __SteamAPI_SteamController_v008_ptr;
extern "C" void* __SteamAPI_SteamDatagramHostedAddress_Clear_ptr;
extern "C" void* __SteamAPI_SteamDatagramHostedAddress_GetPopID_ptr;
extern "C" void* __SteamAPI_SteamDatagramHostedAddress_SetDevAddress_ptr;
extern "C" void* __SteamAPI_SteamFriends_v018_ptr;
extern "C" void* __SteamAPI_SteamGameSearch_v001_ptr;
extern "C" void* __SteamAPI_SteamGameServerHTTP_v003_ptr;
extern "C" void* __SteamAPI_SteamGameServerInventory_v003_ptr;
extern "C" void* __SteamAPI_SteamGameServerNetworkingMessages_SteamAPI_v002_ptr;
extern "C" void* __SteamAPI_SteamGameServerNetworkingSockets_SteamAPI_v012_ptr;
extern "C" void* __SteamAPI_SteamGameServerNetworking_v006_ptr;
extern "C" void* __SteamAPI_SteamGameServerStats_v001_ptr;
extern "C" void* __SteamAPI_SteamGameServerUGC_v021_ptr;
extern "C" void* __SteamAPI_SteamGameServerUtils_v010_ptr;
extern "C" void* __SteamAPI_SteamGameServer_v015_ptr;
extern "C" void* __SteamAPI_SteamHTMLSurface_v005_ptr;
extern "C" void* __SteamAPI_SteamHTTP_v003_ptr;
extern "C" void* __SteamAPI_SteamIPAddress_t_IsSet_ptr;
extern "C" void* __SteamAPI_SteamInput_v006_ptr;
extern "C" void* __SteamAPI_SteamInventory_v003_ptr;
extern "C" void* __SteamAPI_SteamMatchmakingServers_v002_ptr;
extern "C" void* __SteamAPI_SteamMatchmaking_v009_ptr;
extern "C" void* __SteamAPI_SteamMusicRemote_v001_ptr;
extern "C" void* __SteamAPI_SteamMusic_v001_ptr;
extern "C" void* __SteamAPI_SteamNetworkingConfigValue_t_SetFloat_ptr;
extern "C" void* __SteamAPI_SteamNetworkingConfigValue_t_SetInt32_ptr;
extern "C" void* __SteamAPI_SteamNetworkingConfigValue_t_SetInt64_ptr;
extern "C" void* __SteamAPI_SteamNetworkingConfigValue_t_SetPtr_ptr;
extern "C" void* __SteamAPI_SteamNetworkingConfigValue_t_SetString_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIPAddr_Clear_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIPAddr_GetFakeIPType_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIPAddr_GetIPv4_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIPAddr_IsEqualTo_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIPAddr_IsFakeIP_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIPAddr_IsIPv4_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIPAddr_IsIPv6AllZeros_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIPAddr_IsLocalHost_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIPAddr_ParseString_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIPAddr_SetIPv4_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIPAddr_SetIPv6_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIPAddr_SetIPv6LocalHost_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIPAddr_ToString_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_Clear_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_GetFakeIPType_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_GetGenericBytes_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_GetGenericString_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_GetIPAddr_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_GetIPv4_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_GetPSNID_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_GetSteamID_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_GetSteamID64_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_GetXboxPairwiseID_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_IsEqualTo_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_IsFakeIP_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_IsInvalid_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_IsLocalHost_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_ParseString_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_SetGenericBytes_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_SetGenericString_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_SetIPAddr_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_SetIPv4Addr_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_SetLocalHost_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_SetPSNID_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_SetSteamID_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_SetSteamID64_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_SetXboxPairwiseID_ptr;
extern "C" void* __SteamAPI_SteamNetworkingIdentity_ToString_ptr;
extern "C" void* __SteamAPI_SteamNetworkingMessage_t_Release_ptr;
extern "C" void* __SteamAPI_SteamNetworkingMessages_SteamAPI_v002_ptr;
extern "C" void* __SteamAPI_SteamNetworkingSockets_SteamAPI_v012_ptr;
extern "C" void* __SteamAPI_SteamNetworkingUtils_SteamAPI_v004_ptr;
extern "C" void* __SteamAPI_SteamNetworking_v006_ptr;
extern "C" void* __SteamAPI_SteamParentalSettings_v001_ptr;
extern "C" void* __SteamAPI_SteamParties_v002_ptr;
extern "C" void* __SteamAPI_SteamRemotePlay_v003_ptr;
extern "C" void* __SteamAPI_SteamRemoteStorage_v016_ptr;
extern "C" void* __SteamAPI_SteamScreenshots_v003_ptr;
extern "C" void* __SteamAPI_SteamTimeline_v004_ptr;
extern "C" void* __SteamAPI_SteamUGC_v021_ptr;
extern "C" void* __SteamAPI_SteamUserStats_v013_ptr;
extern "C" void* __SteamAPI_SteamUser_v023_ptr;
extern "C" void* __SteamAPI_SteamUtils_v010_ptr;
extern "C" void* __SteamAPI_SteamVideo_v007_ptr;
extern "C" void* __SteamAPI_UnregisterCallResult_ptr;
extern "C" void* __SteamAPI_UnregisterCallback_ptr;
extern "C" void* __SteamAPI_UseBreakpadCrashHandler_ptr;
extern "C" void* __SteamAPI_WriteMiniDump_ptr;
extern "C" void* __SteamAPI_gameserveritem_t_Construct_ptr;
extern "C" void* __SteamAPI_gameserveritem_t_GetName_ptr;
extern "C" void* __SteamAPI_gameserveritem_t_SetName_ptr;
extern "C" void* __SteamAPI_servernetadr_t_Assign_ptr;
extern "C" void* __SteamAPI_servernetadr_t_Construct_ptr;
extern "C" void* __SteamAPI_servernetadr_t_GetConnectionAddressString_ptr;
extern "C" void* __SteamAPI_servernetadr_t_GetConnectionPort_ptr;
extern "C" void* __SteamAPI_servernetadr_t_GetIP_ptr;
extern "C" void* __SteamAPI_servernetadr_t_GetQueryAddressString_ptr;
extern "C" void* __SteamAPI_servernetadr_t_GetQueryPort_ptr;
extern "C" void* __SteamAPI_servernetadr_t_Init_ptr;
extern "C" void* __SteamAPI_servernetadr_t_IsLessThan_ptr;
extern "C" void* __SteamAPI_servernetadr_t_SetConnectionPort_ptr;
extern "C" void* __SteamAPI_servernetadr_t_SetIP_ptr;
extern "C" void* __SteamAPI_servernetadr_t_SetQueryPort_ptr;
extern "C" void* __SteamClient_ptr;
extern "C" void* __SteamGameServer_BSecure_ptr;
extern "C" void* __SteamGameServer_GetHSteamPipe_ptr;
extern "C" void* __SteamGameServer_GetHSteamUser_ptr;
extern "C" void* __SteamGameServer_GetIPCCallCount_ptr;
extern "C" void* __SteamGameServer_GetSteamID_ptr;
extern "C" void* __SteamGameServer_InitSafe_ptr;
extern "C" void* __SteamGameServer_RunCallbacks_ptr;
extern "C" void* __SteamGameServer_Shutdown_ptr;
extern "C" void* __SteamInternal_ContextInit_ptr;
extern "C" void* __SteamInternal_CreateInterface_ptr;
extern "C" void* __SteamInternal_FindOrCreateGameServerInterface_ptr;
extern "C" void* __SteamInternal_FindOrCreateUserInterface_ptr;
extern "C" void* __SteamInternal_GameServer_Init_V2_ptr;
extern "C" void* __SteamInternal_SteamAPI_Init_ptr;
extern "C" void* __g_pSteamClientGameServer_ptr;

bool fill_steam_pointers(HMODULE original) {
    bool complete = true;
    __GetHSteamPipe_ptr = (void*)GetProcAddress(original, "GetHSteamPipe");
    if (!__GetHSteamPipe_ptr) { fprintf(stderr, "error: missing Steam export GetHSteamPipe\n"); complete = false; }
    __GetHSteamUser_ptr = (void*)GetProcAddress(original, "GetHSteamUser");
    if (!__GetHSteamUser_ptr) { fprintf(stderr, "error: missing Steam export GetHSteamUser\n"); complete = false; }
    __SteamAPI_GetHSteamPipe_ptr = (void*)GetProcAddress(original, "SteamAPI_GetHSteamPipe");
    if (!__SteamAPI_GetHSteamPipe_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_GetHSteamPipe\n"); complete = false; }
    __SteamAPI_GetHSteamUser_ptr = (void*)GetProcAddress(original, "SteamAPI_GetHSteamUser");
    if (!__SteamAPI_GetHSteamUser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_GetHSteamUser\n"); complete = false; }
    __SteamAPI_GetSteamInstallPath_ptr = (void*)GetProcAddress(original, "SteamAPI_GetSteamInstallPath");
    if (!__SteamAPI_GetSteamInstallPath_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_GetSteamInstallPath\n"); complete = false; }
    __SteamAPI_ISteamApps_BGetDLCDataByIndex_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_BGetDLCDataByIndex");
    if (!__SteamAPI_ISteamApps_BGetDLCDataByIndex_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_BGetDLCDataByIndex\n"); complete = false; }
    __SteamAPI_ISteamApps_BIsAppInstalled_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_BIsAppInstalled");
    if (!__SteamAPI_ISteamApps_BIsAppInstalled_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_BIsAppInstalled\n"); complete = false; }
    __SteamAPI_ISteamApps_BIsCybercafe_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_BIsCybercafe");
    if (!__SteamAPI_ISteamApps_BIsCybercafe_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_BIsCybercafe\n"); complete = false; }
    __SteamAPI_ISteamApps_BIsDlcInstalled_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_BIsDlcInstalled");
    if (!__SteamAPI_ISteamApps_BIsDlcInstalled_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_BIsDlcInstalled\n"); complete = false; }
    __SteamAPI_ISteamApps_BIsLowViolence_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_BIsLowViolence");
    if (!__SteamAPI_ISteamApps_BIsLowViolence_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_BIsLowViolence\n"); complete = false; }
    __SteamAPI_ISteamApps_BIsSubscribed_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_BIsSubscribed");
    if (!__SteamAPI_ISteamApps_BIsSubscribed_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_BIsSubscribed\n"); complete = false; }
    __SteamAPI_ISteamApps_BIsSubscribedApp_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_BIsSubscribedApp");
    if (!__SteamAPI_ISteamApps_BIsSubscribedApp_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_BIsSubscribedApp\n"); complete = false; }
    __SteamAPI_ISteamApps_BIsSubscribedFromFamilySharing_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_BIsSubscribedFromFamilySharing");
    if (!__SteamAPI_ISteamApps_BIsSubscribedFromFamilySharing_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_BIsSubscribedFromFamilySharing\n"); complete = false; }
    __SteamAPI_ISteamApps_BIsSubscribedFromFreeWeekend_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_BIsSubscribedFromFreeWeekend");
    if (!__SteamAPI_ISteamApps_BIsSubscribedFromFreeWeekend_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_BIsSubscribedFromFreeWeekend\n"); complete = false; }
    __SteamAPI_ISteamApps_BIsTimedTrial_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_BIsTimedTrial");
    if (!__SteamAPI_ISteamApps_BIsTimedTrial_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_BIsTimedTrial\n"); complete = false; }
    __SteamAPI_ISteamApps_BIsVACBanned_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_BIsVACBanned");
    if (!__SteamAPI_ISteamApps_BIsVACBanned_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_BIsVACBanned\n"); complete = false; }
    __SteamAPI_ISteamApps_GetAppBuildId_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_GetAppBuildId");
    if (!__SteamAPI_ISteamApps_GetAppBuildId_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_GetAppBuildId\n"); complete = false; }
    __SteamAPI_ISteamApps_GetAppInstallDir_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_GetAppInstallDir");
    if (!__SteamAPI_ISteamApps_GetAppInstallDir_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_GetAppInstallDir\n"); complete = false; }
    __SteamAPI_ISteamApps_GetAppOwner_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_GetAppOwner");
    if (!__SteamAPI_ISteamApps_GetAppOwner_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_GetAppOwner\n"); complete = false; }
    __SteamAPI_ISteamApps_GetAvailableGameLanguages_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_GetAvailableGameLanguages");
    if (!__SteamAPI_ISteamApps_GetAvailableGameLanguages_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_GetAvailableGameLanguages\n"); complete = false; }
    __SteamAPI_ISteamApps_GetBetaInfo_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_GetBetaInfo");
    if (!__SteamAPI_ISteamApps_GetBetaInfo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_GetBetaInfo\n"); complete = false; }
    __SteamAPI_ISteamApps_GetCurrentBetaName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_GetCurrentBetaName");
    if (!__SteamAPI_ISteamApps_GetCurrentBetaName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_GetCurrentBetaName\n"); complete = false; }
    __SteamAPI_ISteamApps_GetCurrentGameLanguage_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_GetCurrentGameLanguage");
    if (!__SteamAPI_ISteamApps_GetCurrentGameLanguage_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_GetCurrentGameLanguage\n"); complete = false; }
    __SteamAPI_ISteamApps_GetDLCCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_GetDLCCount");
    if (!__SteamAPI_ISteamApps_GetDLCCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_GetDLCCount\n"); complete = false; }
    __SteamAPI_ISteamApps_GetDlcDownloadProgress_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_GetDlcDownloadProgress");
    if (!__SteamAPI_ISteamApps_GetDlcDownloadProgress_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_GetDlcDownloadProgress\n"); complete = false; }
    __SteamAPI_ISteamApps_GetEarliestPurchaseUnixTime_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_GetEarliestPurchaseUnixTime");
    if (!__SteamAPI_ISteamApps_GetEarliestPurchaseUnixTime_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_GetEarliestPurchaseUnixTime\n"); complete = false; }
    __SteamAPI_ISteamApps_GetFileDetails_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_GetFileDetails");
    if (!__SteamAPI_ISteamApps_GetFileDetails_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_GetFileDetails\n"); complete = false; }
    __SteamAPI_ISteamApps_GetInstalledDepots_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_GetInstalledDepots");
    if (!__SteamAPI_ISteamApps_GetInstalledDepots_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_GetInstalledDepots\n"); complete = false; }
    __SteamAPI_ISteamApps_GetLaunchCommandLine_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_GetLaunchCommandLine");
    if (!__SteamAPI_ISteamApps_GetLaunchCommandLine_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_GetLaunchCommandLine\n"); complete = false; }
    __SteamAPI_ISteamApps_GetLaunchQueryParam_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_GetLaunchQueryParam");
    if (!__SteamAPI_ISteamApps_GetLaunchQueryParam_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_GetLaunchQueryParam\n"); complete = false; }
    __SteamAPI_ISteamApps_GetNumBetas_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_GetNumBetas");
    if (!__SteamAPI_ISteamApps_GetNumBetas_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_GetNumBetas\n"); complete = false; }
    __SteamAPI_ISteamApps_InstallDLC_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_InstallDLC");
    if (!__SteamAPI_ISteamApps_InstallDLC_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_InstallDLC\n"); complete = false; }
    __SteamAPI_ISteamApps_MarkContentCorrupt_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_MarkContentCorrupt");
    if (!__SteamAPI_ISteamApps_MarkContentCorrupt_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_MarkContentCorrupt\n"); complete = false; }
    __SteamAPI_ISteamApps_RequestAllProofOfPurchaseKeys_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_RequestAllProofOfPurchaseKeys");
    if (!__SteamAPI_ISteamApps_RequestAllProofOfPurchaseKeys_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_RequestAllProofOfPurchaseKeys\n"); complete = false; }
    __SteamAPI_ISteamApps_RequestAppProofOfPurchaseKey_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_RequestAppProofOfPurchaseKey");
    if (!__SteamAPI_ISteamApps_RequestAppProofOfPurchaseKey_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_RequestAppProofOfPurchaseKey\n"); complete = false; }
    __SteamAPI_ISteamApps_SetActiveBeta_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_SetActiveBeta");
    if (!__SteamAPI_ISteamApps_SetActiveBeta_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_SetActiveBeta\n"); complete = false; }
    __SteamAPI_ISteamApps_SetDlcContext_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_SetDlcContext");
    if (!__SteamAPI_ISteamApps_SetDlcContext_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_SetDlcContext\n"); complete = false; }
    __SteamAPI_ISteamApps_UninstallDLC_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamApps_UninstallDLC");
    if (!__SteamAPI_ISteamApps_UninstallDLC_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamApps_UninstallDLC\n"); complete = false; }
    __SteamAPI_ISteamClient_BReleaseSteamPipe_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_BReleaseSteamPipe");
    if (!__SteamAPI_ISteamClient_BReleaseSteamPipe_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_BReleaseSteamPipe\n"); complete = false; }
    __SteamAPI_ISteamClient_BShutdownIfAllPipesClosed_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_BShutdownIfAllPipesClosed");
    if (!__SteamAPI_ISteamClient_BShutdownIfAllPipesClosed_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_BShutdownIfAllPipesClosed\n"); complete = false; }
    __SteamAPI_ISteamClient_ConnectToGlobalUser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_ConnectToGlobalUser");
    if (!__SteamAPI_ISteamClient_ConnectToGlobalUser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_ConnectToGlobalUser\n"); complete = false; }
    __SteamAPI_ISteamClient_CreateLocalUser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_CreateLocalUser");
    if (!__SteamAPI_ISteamClient_CreateLocalUser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_CreateLocalUser\n"); complete = false; }
    __SteamAPI_ISteamClient_CreateSteamPipe_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_CreateSteamPipe");
    if (!__SteamAPI_ISteamClient_CreateSteamPipe_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_CreateSteamPipe\n"); complete = false; }
    __SteamAPI_ISteamClient_GetIPCCallCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetIPCCallCount");
    if (!__SteamAPI_ISteamClient_GetIPCCallCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetIPCCallCount\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamApps_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamApps");
    if (!__SteamAPI_ISteamClient_GetISteamApps_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamApps\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamController_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamController");
    if (!__SteamAPI_ISteamClient_GetISteamController_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamController\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamFriends_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamFriends");
    if (!__SteamAPI_ISteamClient_GetISteamFriends_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamFriends\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamGameSearch_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamGameSearch");
    if (!__SteamAPI_ISteamClient_GetISteamGameSearch_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamGameSearch\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamGameServer_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamGameServer");
    if (!__SteamAPI_ISteamClient_GetISteamGameServer_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamGameServer\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamGameServerStats_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamGameServerStats");
    if (!__SteamAPI_ISteamClient_GetISteamGameServerStats_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamGameServerStats\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamGenericInterface_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamGenericInterface");
    if (!__SteamAPI_ISteamClient_GetISteamGenericInterface_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamGenericInterface\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamHTMLSurface_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamHTMLSurface");
    if (!__SteamAPI_ISteamClient_GetISteamHTMLSurface_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamHTMLSurface\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamHTTP_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamHTTP");
    if (!__SteamAPI_ISteamClient_GetISteamHTTP_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamHTTP\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamInput_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamInput");
    if (!__SteamAPI_ISteamClient_GetISteamInput_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamInput\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamInventory_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamInventory");
    if (!__SteamAPI_ISteamClient_GetISteamInventory_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamInventory\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamMatchmaking_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamMatchmaking");
    if (!__SteamAPI_ISteamClient_GetISteamMatchmaking_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamMatchmaking\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamMatchmakingServers_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamMatchmakingServers");
    if (!__SteamAPI_ISteamClient_GetISteamMatchmakingServers_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamMatchmakingServers\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamMusic_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamMusic");
    if (!__SteamAPI_ISteamClient_GetISteamMusic_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamMusic\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamMusicRemote_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamMusicRemote");
    if (!__SteamAPI_ISteamClient_GetISteamMusicRemote_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamMusicRemote\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamNetworking_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamNetworking");
    if (!__SteamAPI_ISteamClient_GetISteamNetworking_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamNetworking\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamParentalSettings_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamParentalSettings");
    if (!__SteamAPI_ISteamClient_GetISteamParentalSettings_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamParentalSettings\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamParties_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamParties");
    if (!__SteamAPI_ISteamClient_GetISteamParties_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamParties\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamRemotePlay_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamRemotePlay");
    if (!__SteamAPI_ISteamClient_GetISteamRemotePlay_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamRemotePlay\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamRemoteStorage_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamRemoteStorage");
    if (!__SteamAPI_ISteamClient_GetISteamRemoteStorage_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamRemoteStorage\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamScreenshots_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamScreenshots");
    if (!__SteamAPI_ISteamClient_GetISteamScreenshots_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamScreenshots\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamUGC_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamUGC");
    if (!__SteamAPI_ISteamClient_GetISteamUGC_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamUGC\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamUser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamUser");
    if (!__SteamAPI_ISteamClient_GetISteamUser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamUser\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamUserStats_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamUserStats");
    if (!__SteamAPI_ISteamClient_GetISteamUserStats_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamUserStats\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamUtils_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamUtils");
    if (!__SteamAPI_ISteamClient_GetISteamUtils_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamUtils\n"); complete = false; }
    __SteamAPI_ISteamClient_GetISteamVideo_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_GetISteamVideo");
    if (!__SteamAPI_ISteamClient_GetISteamVideo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_GetISteamVideo\n"); complete = false; }
    __SteamAPI_ISteamClient_ReleaseUser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_ReleaseUser");
    if (!__SteamAPI_ISteamClient_ReleaseUser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_ReleaseUser\n"); complete = false; }
    __SteamAPI_ISteamClient_SetLocalIPBinding_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_SetLocalIPBinding");
    if (!__SteamAPI_ISteamClient_SetLocalIPBinding_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_SetLocalIPBinding\n"); complete = false; }
    __SteamAPI_ISteamClient_SetWarningMessageHook_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamClient_SetWarningMessageHook");
    if (!__SteamAPI_ISteamClient_SetWarningMessageHook_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamClient_SetWarningMessageHook\n"); complete = false; }
    __SteamAPI_ISteamController_ActivateActionSet_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_ActivateActionSet");
    if (!__SteamAPI_ISteamController_ActivateActionSet_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_ActivateActionSet\n"); complete = false; }
    __SteamAPI_ISteamController_ActivateActionSetLayer_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_ActivateActionSetLayer");
    if (!__SteamAPI_ISteamController_ActivateActionSetLayer_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_ActivateActionSetLayer\n"); complete = false; }
    __SteamAPI_ISteamController_DeactivateActionSetLayer_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_DeactivateActionSetLayer");
    if (!__SteamAPI_ISteamController_DeactivateActionSetLayer_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_DeactivateActionSetLayer\n"); complete = false; }
    __SteamAPI_ISteamController_DeactivateAllActionSetLayers_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_DeactivateAllActionSetLayers");
    if (!__SteamAPI_ISteamController_DeactivateAllActionSetLayers_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_DeactivateAllActionSetLayers\n"); complete = false; }
    __SteamAPI_ISteamController_GetActionOriginFromXboxOrigin_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetActionOriginFromXboxOrigin");
    if (!__SteamAPI_ISteamController_GetActionOriginFromXboxOrigin_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetActionOriginFromXboxOrigin\n"); complete = false; }
    __SteamAPI_ISteamController_GetActionSetHandle_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetActionSetHandle");
    if (!__SteamAPI_ISteamController_GetActionSetHandle_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetActionSetHandle\n"); complete = false; }
    __SteamAPI_ISteamController_GetActiveActionSetLayers_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetActiveActionSetLayers");
    if (!__SteamAPI_ISteamController_GetActiveActionSetLayers_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetActiveActionSetLayers\n"); complete = false; }
    __SteamAPI_ISteamController_GetAnalogActionData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetAnalogActionData");
    if (!__SteamAPI_ISteamController_GetAnalogActionData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetAnalogActionData\n"); complete = false; }
    __SteamAPI_ISteamController_GetAnalogActionHandle_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetAnalogActionHandle");
    if (!__SteamAPI_ISteamController_GetAnalogActionHandle_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetAnalogActionHandle\n"); complete = false; }
    __SteamAPI_ISteamController_GetAnalogActionOrigins_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetAnalogActionOrigins");
    if (!__SteamAPI_ISteamController_GetAnalogActionOrigins_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetAnalogActionOrigins\n"); complete = false; }
    __SteamAPI_ISteamController_GetConnectedControllers_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetConnectedControllers");
    if (!__SteamAPI_ISteamController_GetConnectedControllers_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetConnectedControllers\n"); complete = false; }
    __SteamAPI_ISteamController_GetControllerBindingRevision_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetControllerBindingRevision");
    if (!__SteamAPI_ISteamController_GetControllerBindingRevision_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetControllerBindingRevision\n"); complete = false; }
    __SteamAPI_ISteamController_GetControllerForGamepadIndex_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetControllerForGamepadIndex");
    if (!__SteamAPI_ISteamController_GetControllerForGamepadIndex_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetControllerForGamepadIndex\n"); complete = false; }
    __SteamAPI_ISteamController_GetCurrentActionSet_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetCurrentActionSet");
    if (!__SteamAPI_ISteamController_GetCurrentActionSet_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetCurrentActionSet\n"); complete = false; }
    __SteamAPI_ISteamController_GetDigitalActionData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetDigitalActionData");
    if (!__SteamAPI_ISteamController_GetDigitalActionData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetDigitalActionData\n"); complete = false; }
    __SteamAPI_ISteamController_GetDigitalActionHandle_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetDigitalActionHandle");
    if (!__SteamAPI_ISteamController_GetDigitalActionHandle_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetDigitalActionHandle\n"); complete = false; }
    __SteamAPI_ISteamController_GetDigitalActionOrigins_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetDigitalActionOrigins");
    if (!__SteamAPI_ISteamController_GetDigitalActionOrigins_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetDigitalActionOrigins\n"); complete = false; }
    __SteamAPI_ISteamController_GetGamepadIndexForController_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetGamepadIndexForController");
    if (!__SteamAPI_ISteamController_GetGamepadIndexForController_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetGamepadIndexForController\n"); complete = false; }
    __SteamAPI_ISteamController_GetGlyphForActionOrigin_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetGlyphForActionOrigin");
    if (!__SteamAPI_ISteamController_GetGlyphForActionOrigin_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetGlyphForActionOrigin\n"); complete = false; }
    __SteamAPI_ISteamController_GetGlyphForXboxOrigin_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetGlyphForXboxOrigin");
    if (!__SteamAPI_ISteamController_GetGlyphForXboxOrigin_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetGlyphForXboxOrigin\n"); complete = false; }
    __SteamAPI_ISteamController_GetInputTypeForHandle_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetInputTypeForHandle");
    if (!__SteamAPI_ISteamController_GetInputTypeForHandle_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetInputTypeForHandle\n"); complete = false; }
    __SteamAPI_ISteamController_GetMotionData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetMotionData");
    if (!__SteamAPI_ISteamController_GetMotionData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetMotionData\n"); complete = false; }
    __SteamAPI_ISteamController_GetStringForActionOrigin_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetStringForActionOrigin");
    if (!__SteamAPI_ISteamController_GetStringForActionOrigin_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetStringForActionOrigin\n"); complete = false; }
    __SteamAPI_ISteamController_GetStringForXboxOrigin_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_GetStringForXboxOrigin");
    if (!__SteamAPI_ISteamController_GetStringForXboxOrigin_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_GetStringForXboxOrigin\n"); complete = false; }
    __SteamAPI_ISteamController_Init_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_Init");
    if (!__SteamAPI_ISteamController_Init_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_Init\n"); complete = false; }
    __SteamAPI_ISteamController_RunFrame_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_RunFrame");
    if (!__SteamAPI_ISteamController_RunFrame_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_RunFrame\n"); complete = false; }
    __SteamAPI_ISteamController_SetLEDColor_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_SetLEDColor");
    if (!__SteamAPI_ISteamController_SetLEDColor_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_SetLEDColor\n"); complete = false; }
    __SteamAPI_ISteamController_ShowBindingPanel_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_ShowBindingPanel");
    if (!__SteamAPI_ISteamController_ShowBindingPanel_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_ShowBindingPanel\n"); complete = false; }
    __SteamAPI_ISteamController_Shutdown_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_Shutdown");
    if (!__SteamAPI_ISteamController_Shutdown_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_Shutdown\n"); complete = false; }
    __SteamAPI_ISteamController_StopAnalogActionMomentum_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_StopAnalogActionMomentum");
    if (!__SteamAPI_ISteamController_StopAnalogActionMomentum_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_StopAnalogActionMomentum\n"); complete = false; }
    __SteamAPI_ISteamController_TranslateActionOrigin_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_TranslateActionOrigin");
    if (!__SteamAPI_ISteamController_TranslateActionOrigin_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_TranslateActionOrigin\n"); complete = false; }
    __SteamAPI_ISteamController_TriggerHapticPulse_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_TriggerHapticPulse");
    if (!__SteamAPI_ISteamController_TriggerHapticPulse_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_TriggerHapticPulse\n"); complete = false; }
    __SteamAPI_ISteamController_TriggerRepeatedHapticPulse_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_TriggerRepeatedHapticPulse");
    if (!__SteamAPI_ISteamController_TriggerRepeatedHapticPulse_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_TriggerRepeatedHapticPulse\n"); complete = false; }
    __SteamAPI_ISteamController_TriggerVibration_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamController_TriggerVibration");
    if (!__SteamAPI_ISteamController_TriggerVibration_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamController_TriggerVibration\n"); complete = false; }
    __SteamAPI_ISteamFriends_ActivateGameOverlay_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_ActivateGameOverlay");
    if (!__SteamAPI_ISteamFriends_ActivateGameOverlay_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_ActivateGameOverlay\n"); complete = false; }
    __SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog");
    if (!__SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog\n"); complete = false; }
    __SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialogConnectString_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialogConnectString");
    if (!__SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialogConnectString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialogConnectString\n"); complete = false; }
    __SteamAPI_ISteamFriends_ActivateGameOverlayRemotePlayTogetherInviteDialog_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_ActivateGameOverlayRemotePlayTogetherInviteDialog");
    if (!__SteamAPI_ISteamFriends_ActivateGameOverlayRemotePlayTogetherInviteDialog_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_ActivateGameOverlayRemotePlayTogetherInviteDialog\n"); complete = false; }
    __SteamAPI_ISteamFriends_ActivateGameOverlayToStore_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_ActivateGameOverlayToStore");
    if (!__SteamAPI_ISteamFriends_ActivateGameOverlayToStore_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_ActivateGameOverlayToStore\n"); complete = false; }
    __SteamAPI_ISteamFriends_ActivateGameOverlayToUser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_ActivateGameOverlayToUser");
    if (!__SteamAPI_ISteamFriends_ActivateGameOverlayToUser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_ActivateGameOverlayToUser\n"); complete = false; }
    __SteamAPI_ISteamFriends_ActivateGameOverlayToWebPage_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_ActivateGameOverlayToWebPage");
    if (!__SteamAPI_ISteamFriends_ActivateGameOverlayToWebPage_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_ActivateGameOverlayToWebPage\n"); complete = false; }
    __SteamAPI_ISteamFriends_BHasEquippedProfileItem_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_BHasEquippedProfileItem");
    if (!__SteamAPI_ISteamFriends_BHasEquippedProfileItem_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_BHasEquippedProfileItem\n"); complete = false; }
    __SteamAPI_ISteamFriends_ClearRichPresence_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_ClearRichPresence");
    if (!__SteamAPI_ISteamFriends_ClearRichPresence_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_ClearRichPresence\n"); complete = false; }
    __SteamAPI_ISteamFriends_CloseClanChatWindowInSteam_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_CloseClanChatWindowInSteam");
    if (!__SteamAPI_ISteamFriends_CloseClanChatWindowInSteam_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_CloseClanChatWindowInSteam\n"); complete = false; }
    __SteamAPI_ISteamFriends_DownloadClanActivityCounts_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_DownloadClanActivityCounts");
    if (!__SteamAPI_ISteamFriends_DownloadClanActivityCounts_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_DownloadClanActivityCounts\n"); complete = false; }
    __SteamAPI_ISteamFriends_EnumerateFollowingList_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_EnumerateFollowingList");
    if (!__SteamAPI_ISteamFriends_EnumerateFollowingList_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_EnumerateFollowingList\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetChatMemberByIndex_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetChatMemberByIndex");
    if (!__SteamAPI_ISteamFriends_GetChatMemberByIndex_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetChatMemberByIndex\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetClanActivityCounts_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetClanActivityCounts");
    if (!__SteamAPI_ISteamFriends_GetClanActivityCounts_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetClanActivityCounts\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetClanByIndex_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetClanByIndex");
    if (!__SteamAPI_ISteamFriends_GetClanByIndex_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetClanByIndex\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetClanChatMemberCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetClanChatMemberCount");
    if (!__SteamAPI_ISteamFriends_GetClanChatMemberCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetClanChatMemberCount\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetClanChatMessage_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetClanChatMessage");
    if (!__SteamAPI_ISteamFriends_GetClanChatMessage_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetClanChatMessage\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetClanCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetClanCount");
    if (!__SteamAPI_ISteamFriends_GetClanCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetClanCount\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetClanName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetClanName");
    if (!__SteamAPI_ISteamFriends_GetClanName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetClanName\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetClanOfficerByIndex_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetClanOfficerByIndex");
    if (!__SteamAPI_ISteamFriends_GetClanOfficerByIndex_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetClanOfficerByIndex\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetClanOfficerCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetClanOfficerCount");
    if (!__SteamAPI_ISteamFriends_GetClanOfficerCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetClanOfficerCount\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetClanOwner_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetClanOwner");
    if (!__SteamAPI_ISteamFriends_GetClanOwner_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetClanOwner\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetClanTag_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetClanTag");
    if (!__SteamAPI_ISteamFriends_GetClanTag_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetClanTag\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetCoplayFriend_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetCoplayFriend");
    if (!__SteamAPI_ISteamFriends_GetCoplayFriend_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetCoplayFriend\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetCoplayFriendCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetCoplayFriendCount");
    if (!__SteamAPI_ISteamFriends_GetCoplayFriendCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetCoplayFriendCount\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFollowerCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFollowerCount");
    if (!__SteamAPI_ISteamFriends_GetFollowerCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFollowerCount\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendByIndex_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendByIndex");
    if (!__SteamAPI_ISteamFriends_GetFriendByIndex_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendByIndex\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendCoplayGame_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendCoplayGame");
    if (!__SteamAPI_ISteamFriends_GetFriendCoplayGame_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendCoplayGame\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendCoplayTime_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendCoplayTime");
    if (!__SteamAPI_ISteamFriends_GetFriendCoplayTime_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendCoplayTime\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendCount");
    if (!__SteamAPI_ISteamFriends_GetFriendCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendCount\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendCountFromSource_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendCountFromSource");
    if (!__SteamAPI_ISteamFriends_GetFriendCountFromSource_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendCountFromSource\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendFromSourceByIndex_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendFromSourceByIndex");
    if (!__SteamAPI_ISteamFriends_GetFriendFromSourceByIndex_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendFromSourceByIndex\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendGamePlayed_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendGamePlayed");
    if (!__SteamAPI_ISteamFriends_GetFriendGamePlayed_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendGamePlayed\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendMessage_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendMessage");
    if (!__SteamAPI_ISteamFriends_GetFriendMessage_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendMessage\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendPersonaName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendPersonaName");
    if (!__SteamAPI_ISteamFriends_GetFriendPersonaName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendPersonaName\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendPersonaNameHistory_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendPersonaNameHistory");
    if (!__SteamAPI_ISteamFriends_GetFriendPersonaNameHistory_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendPersonaNameHistory\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendPersonaState_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendPersonaState");
    if (!__SteamAPI_ISteamFriends_GetFriendPersonaState_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendPersonaState\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendRelationship_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendRelationship");
    if (!__SteamAPI_ISteamFriends_GetFriendRelationship_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendRelationship\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendRichPresence_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendRichPresence");
    if (!__SteamAPI_ISteamFriends_GetFriendRichPresence_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendRichPresence\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendRichPresenceKeyByIndex_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendRichPresenceKeyByIndex");
    if (!__SteamAPI_ISteamFriends_GetFriendRichPresenceKeyByIndex_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendRichPresenceKeyByIndex\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendRichPresenceKeyCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendRichPresenceKeyCount");
    if (!__SteamAPI_ISteamFriends_GetFriendRichPresenceKeyCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendRichPresenceKeyCount\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendSteamLevel_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendSteamLevel");
    if (!__SteamAPI_ISteamFriends_GetFriendSteamLevel_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendSteamLevel\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendsGroupCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendsGroupCount");
    if (!__SteamAPI_ISteamFriends_GetFriendsGroupCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendsGroupCount\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendsGroupIDByIndex_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendsGroupIDByIndex");
    if (!__SteamAPI_ISteamFriends_GetFriendsGroupIDByIndex_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendsGroupIDByIndex\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendsGroupMembersCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendsGroupMembersCount");
    if (!__SteamAPI_ISteamFriends_GetFriendsGroupMembersCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendsGroupMembersCount\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendsGroupMembersList_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendsGroupMembersList");
    if (!__SteamAPI_ISteamFriends_GetFriendsGroupMembersList_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendsGroupMembersList\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetFriendsGroupName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetFriendsGroupName");
    if (!__SteamAPI_ISteamFriends_GetFriendsGroupName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetFriendsGroupName\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetLargeFriendAvatar_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetLargeFriendAvatar");
    if (!__SteamAPI_ISteamFriends_GetLargeFriendAvatar_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetLargeFriendAvatar\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetMediumFriendAvatar_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetMediumFriendAvatar");
    if (!__SteamAPI_ISteamFriends_GetMediumFriendAvatar_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetMediumFriendAvatar\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetNumChatsWithUnreadPriorityMessages_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetNumChatsWithUnreadPriorityMessages");
    if (!__SteamAPI_ISteamFriends_GetNumChatsWithUnreadPriorityMessages_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetNumChatsWithUnreadPriorityMessages\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetPersonaName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetPersonaName");
    if (!__SteamAPI_ISteamFriends_GetPersonaName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetPersonaName\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetPersonaState_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetPersonaState");
    if (!__SteamAPI_ISteamFriends_GetPersonaState_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetPersonaState\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetPlayerNickname_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetPlayerNickname");
    if (!__SteamAPI_ISteamFriends_GetPlayerNickname_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetPlayerNickname\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetProfileItemPropertyString_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetProfileItemPropertyString");
    if (!__SteamAPI_ISteamFriends_GetProfileItemPropertyString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetProfileItemPropertyString\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetProfileItemPropertyUint_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetProfileItemPropertyUint");
    if (!__SteamAPI_ISteamFriends_GetProfileItemPropertyUint_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetProfileItemPropertyUint\n"); complete = false; }
    __SteamAPI_ISteamFriends_GetSmallFriendAvatar_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_GetSmallFriendAvatar");
    if (!__SteamAPI_ISteamFriends_GetSmallFriendAvatar_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_GetSmallFriendAvatar\n"); complete = false; }
    __SteamAPI_ISteamFriends_HasFriend_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_HasFriend");
    if (!__SteamAPI_ISteamFriends_HasFriend_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_HasFriend\n"); complete = false; }
    __SteamAPI_ISteamFriends_InviteUserToGame_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_InviteUserToGame");
    if (!__SteamAPI_ISteamFriends_InviteUserToGame_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_InviteUserToGame\n"); complete = false; }
    __SteamAPI_ISteamFriends_IsClanChatAdmin_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_IsClanChatAdmin");
    if (!__SteamAPI_ISteamFriends_IsClanChatAdmin_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_IsClanChatAdmin\n"); complete = false; }
    __SteamAPI_ISteamFriends_IsClanChatWindowOpenInSteam_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_IsClanChatWindowOpenInSteam");
    if (!__SteamAPI_ISteamFriends_IsClanChatWindowOpenInSteam_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_IsClanChatWindowOpenInSteam\n"); complete = false; }
    __SteamAPI_ISteamFriends_IsClanOfficialGameGroup_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_IsClanOfficialGameGroup");
    if (!__SteamAPI_ISteamFriends_IsClanOfficialGameGroup_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_IsClanOfficialGameGroup\n"); complete = false; }
    __SteamAPI_ISteamFriends_IsClanPublic_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_IsClanPublic");
    if (!__SteamAPI_ISteamFriends_IsClanPublic_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_IsClanPublic\n"); complete = false; }
    __SteamAPI_ISteamFriends_IsFollowing_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_IsFollowing");
    if (!__SteamAPI_ISteamFriends_IsFollowing_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_IsFollowing\n"); complete = false; }
    __SteamAPI_ISteamFriends_IsUserInSource_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_IsUserInSource");
    if (!__SteamAPI_ISteamFriends_IsUserInSource_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_IsUserInSource\n"); complete = false; }
    __SteamAPI_ISteamFriends_JoinClanChatRoom_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_JoinClanChatRoom");
    if (!__SteamAPI_ISteamFriends_JoinClanChatRoom_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_JoinClanChatRoom\n"); complete = false; }
    __SteamAPI_ISteamFriends_LeaveClanChatRoom_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_LeaveClanChatRoom");
    if (!__SteamAPI_ISteamFriends_LeaveClanChatRoom_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_LeaveClanChatRoom\n"); complete = false; }
    __SteamAPI_ISteamFriends_OpenClanChatWindowInSteam_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_OpenClanChatWindowInSteam");
    if (!__SteamAPI_ISteamFriends_OpenClanChatWindowInSteam_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_OpenClanChatWindowInSteam\n"); complete = false; }
    __SteamAPI_ISteamFriends_RegisterProtocolInOverlayBrowser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_RegisterProtocolInOverlayBrowser");
    if (!__SteamAPI_ISteamFriends_RegisterProtocolInOverlayBrowser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_RegisterProtocolInOverlayBrowser\n"); complete = false; }
    __SteamAPI_ISteamFriends_ReplyToFriendMessage_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_ReplyToFriendMessage");
    if (!__SteamAPI_ISteamFriends_ReplyToFriendMessage_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_ReplyToFriendMessage\n"); complete = false; }
    __SteamAPI_ISteamFriends_RequestClanOfficerList_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_RequestClanOfficerList");
    if (!__SteamAPI_ISteamFriends_RequestClanOfficerList_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_RequestClanOfficerList\n"); complete = false; }
    __SteamAPI_ISteamFriends_RequestEquippedProfileItems_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_RequestEquippedProfileItems");
    if (!__SteamAPI_ISteamFriends_RequestEquippedProfileItems_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_RequestEquippedProfileItems\n"); complete = false; }
    __SteamAPI_ISteamFriends_RequestFriendRichPresence_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_RequestFriendRichPresence");
    if (!__SteamAPI_ISteamFriends_RequestFriendRichPresence_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_RequestFriendRichPresence\n"); complete = false; }
    __SteamAPI_ISteamFriends_RequestUserInformation_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_RequestUserInformation");
    if (!__SteamAPI_ISteamFriends_RequestUserInformation_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_RequestUserInformation\n"); complete = false; }
    __SteamAPI_ISteamFriends_SendClanChatMessage_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_SendClanChatMessage");
    if (!__SteamAPI_ISteamFriends_SendClanChatMessage_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_SendClanChatMessage\n"); complete = false; }
    __SteamAPI_ISteamFriends_SetInGameVoiceSpeaking_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_SetInGameVoiceSpeaking");
    if (!__SteamAPI_ISteamFriends_SetInGameVoiceSpeaking_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_SetInGameVoiceSpeaking\n"); complete = false; }
    __SteamAPI_ISteamFriends_SetListenForFriendsMessages_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_SetListenForFriendsMessages");
    if (!__SteamAPI_ISteamFriends_SetListenForFriendsMessages_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_SetListenForFriendsMessages\n"); complete = false; }
    __SteamAPI_ISteamFriends_SetPlayedWith_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_SetPlayedWith");
    if (!__SteamAPI_ISteamFriends_SetPlayedWith_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_SetPlayedWith\n"); complete = false; }
    __SteamAPI_ISteamFriends_SetRichPresence_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamFriends_SetRichPresence");
    if (!__SteamAPI_ISteamFriends_SetRichPresence_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamFriends_SetRichPresence\n"); complete = false; }
    __SteamAPI_ISteamGameSearch_AcceptGame_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameSearch_AcceptGame");
    if (!__SteamAPI_ISteamGameSearch_AcceptGame_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameSearch_AcceptGame\n"); complete = false; }
    __SteamAPI_ISteamGameSearch_AddGameSearchParams_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameSearch_AddGameSearchParams");
    if (!__SteamAPI_ISteamGameSearch_AddGameSearchParams_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameSearch_AddGameSearchParams\n"); complete = false; }
    __SteamAPI_ISteamGameSearch_CancelRequestPlayersForGame_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameSearch_CancelRequestPlayersForGame");
    if (!__SteamAPI_ISteamGameSearch_CancelRequestPlayersForGame_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameSearch_CancelRequestPlayersForGame\n"); complete = false; }
    __SteamAPI_ISteamGameSearch_DeclineGame_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameSearch_DeclineGame");
    if (!__SteamAPI_ISteamGameSearch_DeclineGame_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameSearch_DeclineGame\n"); complete = false; }
    __SteamAPI_ISteamGameSearch_EndGame_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameSearch_EndGame");
    if (!__SteamAPI_ISteamGameSearch_EndGame_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameSearch_EndGame\n"); complete = false; }
    __SteamAPI_ISteamGameSearch_EndGameSearch_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameSearch_EndGameSearch");
    if (!__SteamAPI_ISteamGameSearch_EndGameSearch_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameSearch_EndGameSearch\n"); complete = false; }
    __SteamAPI_ISteamGameSearch_HostConfirmGameStart_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameSearch_HostConfirmGameStart");
    if (!__SteamAPI_ISteamGameSearch_HostConfirmGameStart_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameSearch_HostConfirmGameStart\n"); complete = false; }
    __SteamAPI_ISteamGameSearch_RequestPlayersForGame_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameSearch_RequestPlayersForGame");
    if (!__SteamAPI_ISteamGameSearch_RequestPlayersForGame_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameSearch_RequestPlayersForGame\n"); complete = false; }
    __SteamAPI_ISteamGameSearch_RetrieveConnectionDetails_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameSearch_RetrieveConnectionDetails");
    if (!__SteamAPI_ISteamGameSearch_RetrieveConnectionDetails_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameSearch_RetrieveConnectionDetails\n"); complete = false; }
    __SteamAPI_ISteamGameSearch_SearchForGameSolo_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameSearch_SearchForGameSolo");
    if (!__SteamAPI_ISteamGameSearch_SearchForGameSolo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameSearch_SearchForGameSolo\n"); complete = false; }
    __SteamAPI_ISteamGameSearch_SearchForGameWithLobby_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameSearch_SearchForGameWithLobby");
    if (!__SteamAPI_ISteamGameSearch_SearchForGameWithLobby_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameSearch_SearchForGameWithLobby\n"); complete = false; }
    __SteamAPI_ISteamGameSearch_SetConnectionDetails_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameSearch_SetConnectionDetails");
    if (!__SteamAPI_ISteamGameSearch_SetConnectionDetails_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameSearch_SetConnectionDetails\n"); complete = false; }
    __SteamAPI_ISteamGameSearch_SetGameHostParams_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameSearch_SetGameHostParams");
    if (!__SteamAPI_ISteamGameSearch_SetGameHostParams_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameSearch_SetGameHostParams\n"); complete = false; }
    __SteamAPI_ISteamGameSearch_SubmitPlayerResult_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameSearch_SubmitPlayerResult");
    if (!__SteamAPI_ISteamGameSearch_SubmitPlayerResult_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameSearch_SubmitPlayerResult\n"); complete = false; }
    __SteamAPI_ISteamGameServerStats_ClearUserAchievement_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServerStats_ClearUserAchievement");
    if (!__SteamAPI_ISteamGameServerStats_ClearUserAchievement_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServerStats_ClearUserAchievement\n"); complete = false; }
    __SteamAPI_ISteamGameServerStats_GetUserAchievement_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServerStats_GetUserAchievement");
    if (!__SteamAPI_ISteamGameServerStats_GetUserAchievement_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServerStats_GetUserAchievement\n"); complete = false; }
    __SteamAPI_ISteamGameServerStats_GetUserStatFloat_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServerStats_GetUserStatFloat");
    if (!__SteamAPI_ISteamGameServerStats_GetUserStatFloat_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServerStats_GetUserStatFloat\n"); complete = false; }
    __SteamAPI_ISteamGameServerStats_GetUserStatInt32_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServerStats_GetUserStatInt32");
    if (!__SteamAPI_ISteamGameServerStats_GetUserStatInt32_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServerStats_GetUserStatInt32\n"); complete = false; }
    __SteamAPI_ISteamGameServerStats_RequestUserStats_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServerStats_RequestUserStats");
    if (!__SteamAPI_ISteamGameServerStats_RequestUserStats_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServerStats_RequestUserStats\n"); complete = false; }
    __SteamAPI_ISteamGameServerStats_SetUserAchievement_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServerStats_SetUserAchievement");
    if (!__SteamAPI_ISteamGameServerStats_SetUserAchievement_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServerStats_SetUserAchievement\n"); complete = false; }
    __SteamAPI_ISteamGameServerStats_SetUserStatFloat_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServerStats_SetUserStatFloat");
    if (!__SteamAPI_ISteamGameServerStats_SetUserStatFloat_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServerStats_SetUserStatFloat\n"); complete = false; }
    __SteamAPI_ISteamGameServerStats_SetUserStatInt32_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServerStats_SetUserStatInt32");
    if (!__SteamAPI_ISteamGameServerStats_SetUserStatInt32_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServerStats_SetUserStatInt32\n"); complete = false; }
    __SteamAPI_ISteamGameServerStats_StoreUserStats_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServerStats_StoreUserStats");
    if (!__SteamAPI_ISteamGameServerStats_StoreUserStats_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServerStats_StoreUserStats\n"); complete = false; }
    __SteamAPI_ISteamGameServerStats_UpdateUserAvgRateStat_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServerStats_UpdateUserAvgRateStat");
    if (!__SteamAPI_ISteamGameServerStats_UpdateUserAvgRateStat_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServerStats_UpdateUserAvgRateStat\n"); complete = false; }
    __SteamAPI_ISteamGameServer_AssociateWithClan_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_AssociateWithClan");
    if (!__SteamAPI_ISteamGameServer_AssociateWithClan_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_AssociateWithClan\n"); complete = false; }
    __SteamAPI_ISteamGameServer_BLoggedOn_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_BLoggedOn");
    if (!__SteamAPI_ISteamGameServer_BLoggedOn_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_BLoggedOn\n"); complete = false; }
    __SteamAPI_ISteamGameServer_BSecure_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_BSecure");
    if (!__SteamAPI_ISteamGameServer_BSecure_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_BSecure\n"); complete = false; }
    __SteamAPI_ISteamGameServer_BUpdateUserData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_BUpdateUserData");
    if (!__SteamAPI_ISteamGameServer_BUpdateUserData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_BUpdateUserData\n"); complete = false; }
    __SteamAPI_ISteamGameServer_BeginAuthSession_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_BeginAuthSession");
    if (!__SteamAPI_ISteamGameServer_BeginAuthSession_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_BeginAuthSession\n"); complete = false; }
    __SteamAPI_ISteamGameServer_CancelAuthTicket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_CancelAuthTicket");
    if (!__SteamAPI_ISteamGameServer_CancelAuthTicket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_CancelAuthTicket\n"); complete = false; }
    __SteamAPI_ISteamGameServer_ClearAllKeyValues_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_ClearAllKeyValues");
    if (!__SteamAPI_ISteamGameServer_ClearAllKeyValues_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_ClearAllKeyValues\n"); complete = false; }
    __SteamAPI_ISteamGameServer_ComputeNewPlayerCompatibility_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_ComputeNewPlayerCompatibility");
    if (!__SteamAPI_ISteamGameServer_ComputeNewPlayerCompatibility_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_ComputeNewPlayerCompatibility\n"); complete = false; }
    __SteamAPI_ISteamGameServer_CreateUnauthenticatedUserConnection_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_CreateUnauthenticatedUserConnection");
    if (!__SteamAPI_ISteamGameServer_CreateUnauthenticatedUserConnection_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_CreateUnauthenticatedUserConnection\n"); complete = false; }
    __SteamAPI_ISteamGameServer_EndAuthSession_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_EndAuthSession");
    if (!__SteamAPI_ISteamGameServer_EndAuthSession_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_EndAuthSession\n"); complete = false; }
    __SteamAPI_ISteamGameServer_GetAuthSessionTicket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_GetAuthSessionTicket");
    if (!__SteamAPI_ISteamGameServer_GetAuthSessionTicket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_GetAuthSessionTicket\n"); complete = false; }
    __SteamAPI_ISteamGameServer_GetGameplayStats_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_GetGameplayStats");
    if (!__SteamAPI_ISteamGameServer_GetGameplayStats_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_GetGameplayStats\n"); complete = false; }
    __SteamAPI_ISteamGameServer_GetNextOutgoingPacket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_GetNextOutgoingPacket");
    if (!__SteamAPI_ISteamGameServer_GetNextOutgoingPacket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_GetNextOutgoingPacket\n"); complete = false; }
    __SteamAPI_ISteamGameServer_GetPublicIP_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_GetPublicIP");
    if (!__SteamAPI_ISteamGameServer_GetPublicIP_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_GetPublicIP\n"); complete = false; }
    __SteamAPI_ISteamGameServer_GetServerReputation_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_GetServerReputation");
    if (!__SteamAPI_ISteamGameServer_GetServerReputation_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_GetServerReputation\n"); complete = false; }
    __SteamAPI_ISteamGameServer_GetSteamID_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_GetSteamID");
    if (!__SteamAPI_ISteamGameServer_GetSteamID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_GetSteamID\n"); complete = false; }
    __SteamAPI_ISteamGameServer_HandleIncomingPacket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_HandleIncomingPacket");
    if (!__SteamAPI_ISteamGameServer_HandleIncomingPacket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_HandleIncomingPacket\n"); complete = false; }
    __SteamAPI_ISteamGameServer_LogOff_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_LogOff");
    if (!__SteamAPI_ISteamGameServer_LogOff_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_LogOff\n"); complete = false; }
    __SteamAPI_ISteamGameServer_LogOn_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_LogOn");
    if (!__SteamAPI_ISteamGameServer_LogOn_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_LogOn\n"); complete = false; }
    __SteamAPI_ISteamGameServer_LogOnAnonymous_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_LogOnAnonymous");
    if (!__SteamAPI_ISteamGameServer_LogOnAnonymous_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_LogOnAnonymous\n"); complete = false; }
    __SteamAPI_ISteamGameServer_RequestUserGroupStatus_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_RequestUserGroupStatus");
    if (!__SteamAPI_ISteamGameServer_RequestUserGroupStatus_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_RequestUserGroupStatus\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SendUserConnectAndAuthenticate_DEPRECATED_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SendUserConnectAndAuthenticate_DEPRECATED");
    if (!__SteamAPI_ISteamGameServer_SendUserConnectAndAuthenticate_DEPRECATED_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SendUserConnectAndAuthenticate_DEPRECATED\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SendUserDisconnect_DEPRECATED_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SendUserDisconnect_DEPRECATED");
    if (!__SteamAPI_ISteamGameServer_SendUserDisconnect_DEPRECATED_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SendUserDisconnect_DEPRECATED\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SetAdvertiseServerActive_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SetAdvertiseServerActive");
    if (!__SteamAPI_ISteamGameServer_SetAdvertiseServerActive_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SetAdvertiseServerActive\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SetBotPlayerCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SetBotPlayerCount");
    if (!__SteamAPI_ISteamGameServer_SetBotPlayerCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SetBotPlayerCount\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SetDedicatedServer_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SetDedicatedServer");
    if (!__SteamAPI_ISteamGameServer_SetDedicatedServer_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SetDedicatedServer\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SetGameData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SetGameData");
    if (!__SteamAPI_ISteamGameServer_SetGameData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SetGameData\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SetGameDescription_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SetGameDescription");
    if (!__SteamAPI_ISteamGameServer_SetGameDescription_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SetGameDescription\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SetGameTags_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SetGameTags");
    if (!__SteamAPI_ISteamGameServer_SetGameTags_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SetGameTags\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SetKeyValue_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SetKeyValue");
    if (!__SteamAPI_ISteamGameServer_SetKeyValue_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SetKeyValue\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SetMapName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SetMapName");
    if (!__SteamAPI_ISteamGameServer_SetMapName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SetMapName\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SetMaxPlayerCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SetMaxPlayerCount");
    if (!__SteamAPI_ISteamGameServer_SetMaxPlayerCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SetMaxPlayerCount\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SetModDir_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SetModDir");
    if (!__SteamAPI_ISteamGameServer_SetModDir_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SetModDir\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SetPasswordProtected_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SetPasswordProtected");
    if (!__SteamAPI_ISteamGameServer_SetPasswordProtected_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SetPasswordProtected\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SetProduct_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SetProduct");
    if (!__SteamAPI_ISteamGameServer_SetProduct_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SetProduct\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SetRegion_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SetRegion");
    if (!__SteamAPI_ISteamGameServer_SetRegion_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SetRegion\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SetServerName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SetServerName");
    if (!__SteamAPI_ISteamGameServer_SetServerName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SetServerName\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SetSpectatorPort_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SetSpectatorPort");
    if (!__SteamAPI_ISteamGameServer_SetSpectatorPort_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SetSpectatorPort\n"); complete = false; }
    __SteamAPI_ISteamGameServer_SetSpectatorServerName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_SetSpectatorServerName");
    if (!__SteamAPI_ISteamGameServer_SetSpectatorServerName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_SetSpectatorServerName\n"); complete = false; }
    __SteamAPI_ISteamGameServer_UserHasLicenseForApp_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_UserHasLicenseForApp");
    if (!__SteamAPI_ISteamGameServer_UserHasLicenseForApp_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_UserHasLicenseForApp\n"); complete = false; }
    __SteamAPI_ISteamGameServer_WasRestartRequested_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamGameServer_WasRestartRequested");
    if (!__SteamAPI_ISteamGameServer_WasRestartRequested_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamGameServer_WasRestartRequested\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_AddHeader_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_AddHeader");
    if (!__SteamAPI_ISteamHTMLSurface_AddHeader_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_AddHeader\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_AllowStartRequest_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_AllowStartRequest");
    if (!__SteamAPI_ISteamHTMLSurface_AllowStartRequest_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_AllowStartRequest\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_CopyToClipboard_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_CopyToClipboard");
    if (!__SteamAPI_ISteamHTMLSurface_CopyToClipboard_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_CopyToClipboard\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_CreateBrowser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_CreateBrowser");
    if (!__SteamAPI_ISteamHTMLSurface_CreateBrowser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_CreateBrowser\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_ExecuteJavascript_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_ExecuteJavascript");
    if (!__SteamAPI_ISteamHTMLSurface_ExecuteJavascript_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_ExecuteJavascript\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_FileLoadDialogResponse_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_FileLoadDialogResponse");
    if (!__SteamAPI_ISteamHTMLSurface_FileLoadDialogResponse_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_FileLoadDialogResponse\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_Find_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_Find");
    if (!__SteamAPI_ISteamHTMLSurface_Find_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_Find\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_GetLinkAtPosition_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_GetLinkAtPosition");
    if (!__SteamAPI_ISteamHTMLSurface_GetLinkAtPosition_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_GetLinkAtPosition\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_GoBack_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_GoBack");
    if (!__SteamAPI_ISteamHTMLSurface_GoBack_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_GoBack\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_GoForward_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_GoForward");
    if (!__SteamAPI_ISteamHTMLSurface_GoForward_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_GoForward\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_Init_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_Init");
    if (!__SteamAPI_ISteamHTMLSurface_Init_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_Init\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_JSDialogResponse_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_JSDialogResponse");
    if (!__SteamAPI_ISteamHTMLSurface_JSDialogResponse_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_JSDialogResponse\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_KeyChar_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_KeyChar");
    if (!__SteamAPI_ISteamHTMLSurface_KeyChar_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_KeyChar\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_KeyDown_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_KeyDown");
    if (!__SteamAPI_ISteamHTMLSurface_KeyDown_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_KeyDown\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_KeyUp_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_KeyUp");
    if (!__SteamAPI_ISteamHTMLSurface_KeyUp_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_KeyUp\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_LoadURL_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_LoadURL");
    if (!__SteamAPI_ISteamHTMLSurface_LoadURL_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_LoadURL\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_MouseDoubleClick_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_MouseDoubleClick");
    if (!__SteamAPI_ISteamHTMLSurface_MouseDoubleClick_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_MouseDoubleClick\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_MouseDown_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_MouseDown");
    if (!__SteamAPI_ISteamHTMLSurface_MouseDown_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_MouseDown\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_MouseMove_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_MouseMove");
    if (!__SteamAPI_ISteamHTMLSurface_MouseMove_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_MouseMove\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_MouseUp_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_MouseUp");
    if (!__SteamAPI_ISteamHTMLSurface_MouseUp_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_MouseUp\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_MouseWheel_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_MouseWheel");
    if (!__SteamAPI_ISteamHTMLSurface_MouseWheel_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_MouseWheel\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_OpenDeveloperTools_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_OpenDeveloperTools");
    if (!__SteamAPI_ISteamHTMLSurface_OpenDeveloperTools_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_OpenDeveloperTools\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_PasteFromClipboard_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_PasteFromClipboard");
    if (!__SteamAPI_ISteamHTMLSurface_PasteFromClipboard_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_PasteFromClipboard\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_Reload_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_Reload");
    if (!__SteamAPI_ISteamHTMLSurface_Reload_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_Reload\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_RemoveBrowser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_RemoveBrowser");
    if (!__SteamAPI_ISteamHTMLSurface_RemoveBrowser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_RemoveBrowser\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_SetBackgroundMode_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_SetBackgroundMode");
    if (!__SteamAPI_ISteamHTMLSurface_SetBackgroundMode_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_SetBackgroundMode\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_SetCookie_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_SetCookie");
    if (!__SteamAPI_ISteamHTMLSurface_SetCookie_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_SetCookie\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_SetDPIScalingFactor_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_SetDPIScalingFactor");
    if (!__SteamAPI_ISteamHTMLSurface_SetDPIScalingFactor_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_SetDPIScalingFactor\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_SetHorizontalScroll_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_SetHorizontalScroll");
    if (!__SteamAPI_ISteamHTMLSurface_SetHorizontalScroll_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_SetHorizontalScroll\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_SetKeyFocus_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_SetKeyFocus");
    if (!__SteamAPI_ISteamHTMLSurface_SetKeyFocus_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_SetKeyFocus\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_SetPageScaleFactor_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_SetPageScaleFactor");
    if (!__SteamAPI_ISteamHTMLSurface_SetPageScaleFactor_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_SetPageScaleFactor\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_SetSize_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_SetSize");
    if (!__SteamAPI_ISteamHTMLSurface_SetSize_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_SetSize\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_SetVerticalScroll_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_SetVerticalScroll");
    if (!__SteamAPI_ISteamHTMLSurface_SetVerticalScroll_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_SetVerticalScroll\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_Shutdown_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_Shutdown");
    if (!__SteamAPI_ISteamHTMLSurface_Shutdown_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_Shutdown\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_StopFind_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_StopFind");
    if (!__SteamAPI_ISteamHTMLSurface_StopFind_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_StopFind\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_StopLoad_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_StopLoad");
    if (!__SteamAPI_ISteamHTMLSurface_StopLoad_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_StopLoad\n"); complete = false; }
    __SteamAPI_ISteamHTMLSurface_ViewSource_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTMLSurface_ViewSource");
    if (!__SteamAPI_ISteamHTMLSurface_ViewSource_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTMLSurface_ViewSource\n"); complete = false; }
    __SteamAPI_ISteamHTTP_CreateCookieContainer_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_CreateCookieContainer");
    if (!__SteamAPI_ISteamHTTP_CreateCookieContainer_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_CreateCookieContainer\n"); complete = false; }
    __SteamAPI_ISteamHTTP_CreateHTTPRequest_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_CreateHTTPRequest");
    if (!__SteamAPI_ISteamHTTP_CreateHTTPRequest_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_CreateHTTPRequest\n"); complete = false; }
    __SteamAPI_ISteamHTTP_DeferHTTPRequest_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_DeferHTTPRequest");
    if (!__SteamAPI_ISteamHTTP_DeferHTTPRequest_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_DeferHTTPRequest\n"); complete = false; }
    __SteamAPI_ISteamHTTP_GetHTTPDownloadProgressPct_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_GetHTTPDownloadProgressPct");
    if (!__SteamAPI_ISteamHTTP_GetHTTPDownloadProgressPct_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_GetHTTPDownloadProgressPct\n"); complete = false; }
    __SteamAPI_ISteamHTTP_GetHTTPRequestWasTimedOut_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_GetHTTPRequestWasTimedOut");
    if (!__SteamAPI_ISteamHTTP_GetHTTPRequestWasTimedOut_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_GetHTTPRequestWasTimedOut\n"); complete = false; }
    __SteamAPI_ISteamHTTP_GetHTTPResponseBodyData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_GetHTTPResponseBodyData");
    if (!__SteamAPI_ISteamHTTP_GetHTTPResponseBodyData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_GetHTTPResponseBodyData\n"); complete = false; }
    __SteamAPI_ISteamHTTP_GetHTTPResponseBodySize_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_GetHTTPResponseBodySize");
    if (!__SteamAPI_ISteamHTTP_GetHTTPResponseBodySize_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_GetHTTPResponseBodySize\n"); complete = false; }
    __SteamAPI_ISteamHTTP_GetHTTPResponseHeaderSize_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_GetHTTPResponseHeaderSize");
    if (!__SteamAPI_ISteamHTTP_GetHTTPResponseHeaderSize_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_GetHTTPResponseHeaderSize\n"); complete = false; }
    __SteamAPI_ISteamHTTP_GetHTTPResponseHeaderValue_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_GetHTTPResponseHeaderValue");
    if (!__SteamAPI_ISteamHTTP_GetHTTPResponseHeaderValue_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_GetHTTPResponseHeaderValue\n"); complete = false; }
    __SteamAPI_ISteamHTTP_GetHTTPStreamingResponseBodyData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_GetHTTPStreamingResponseBodyData");
    if (!__SteamAPI_ISteamHTTP_GetHTTPStreamingResponseBodyData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_GetHTTPStreamingResponseBodyData\n"); complete = false; }
    __SteamAPI_ISteamHTTP_PrioritizeHTTPRequest_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_PrioritizeHTTPRequest");
    if (!__SteamAPI_ISteamHTTP_PrioritizeHTTPRequest_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_PrioritizeHTTPRequest\n"); complete = false; }
    __SteamAPI_ISteamHTTP_ReleaseCookieContainer_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_ReleaseCookieContainer");
    if (!__SteamAPI_ISteamHTTP_ReleaseCookieContainer_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_ReleaseCookieContainer\n"); complete = false; }
    __SteamAPI_ISteamHTTP_ReleaseHTTPRequest_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_ReleaseHTTPRequest");
    if (!__SteamAPI_ISteamHTTP_ReleaseHTTPRequest_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_ReleaseHTTPRequest\n"); complete = false; }
    __SteamAPI_ISteamHTTP_SendHTTPRequest_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_SendHTTPRequest");
    if (!__SteamAPI_ISteamHTTP_SendHTTPRequest_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_SendHTTPRequest\n"); complete = false; }
    __SteamAPI_ISteamHTTP_SendHTTPRequestAndStreamResponse_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_SendHTTPRequestAndStreamResponse");
    if (!__SteamAPI_ISteamHTTP_SendHTTPRequestAndStreamResponse_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_SendHTTPRequestAndStreamResponse\n"); complete = false; }
    __SteamAPI_ISteamHTTP_SetCookie_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_SetCookie");
    if (!__SteamAPI_ISteamHTTP_SetCookie_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_SetCookie\n"); complete = false; }
    __SteamAPI_ISteamHTTP_SetHTTPRequestAbsoluteTimeoutMS_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_SetHTTPRequestAbsoluteTimeoutMS");
    if (!__SteamAPI_ISteamHTTP_SetHTTPRequestAbsoluteTimeoutMS_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_SetHTTPRequestAbsoluteTimeoutMS\n"); complete = false; }
    __SteamAPI_ISteamHTTP_SetHTTPRequestContextValue_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_SetHTTPRequestContextValue");
    if (!__SteamAPI_ISteamHTTP_SetHTTPRequestContextValue_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_SetHTTPRequestContextValue\n"); complete = false; }
    __SteamAPI_ISteamHTTP_SetHTTPRequestCookieContainer_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_SetHTTPRequestCookieContainer");
    if (!__SteamAPI_ISteamHTTP_SetHTTPRequestCookieContainer_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_SetHTTPRequestCookieContainer\n"); complete = false; }
    __SteamAPI_ISteamHTTP_SetHTTPRequestGetOrPostParameter_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_SetHTTPRequestGetOrPostParameter");
    if (!__SteamAPI_ISteamHTTP_SetHTTPRequestGetOrPostParameter_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_SetHTTPRequestGetOrPostParameter\n"); complete = false; }
    __SteamAPI_ISteamHTTP_SetHTTPRequestHeaderValue_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_SetHTTPRequestHeaderValue");
    if (!__SteamAPI_ISteamHTTP_SetHTTPRequestHeaderValue_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_SetHTTPRequestHeaderValue\n"); complete = false; }
    __SteamAPI_ISteamHTTP_SetHTTPRequestNetworkActivityTimeout_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_SetHTTPRequestNetworkActivityTimeout");
    if (!__SteamAPI_ISteamHTTP_SetHTTPRequestNetworkActivityTimeout_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_SetHTTPRequestNetworkActivityTimeout\n"); complete = false; }
    __SteamAPI_ISteamHTTP_SetHTTPRequestRawPostBody_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_SetHTTPRequestRawPostBody");
    if (!__SteamAPI_ISteamHTTP_SetHTTPRequestRawPostBody_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_SetHTTPRequestRawPostBody\n"); complete = false; }
    __SteamAPI_ISteamHTTP_SetHTTPRequestRequiresVerifiedCertificate_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_SetHTTPRequestRequiresVerifiedCertificate");
    if (!__SteamAPI_ISteamHTTP_SetHTTPRequestRequiresVerifiedCertificate_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_SetHTTPRequestRequiresVerifiedCertificate\n"); complete = false; }
    __SteamAPI_ISteamHTTP_SetHTTPRequestUserAgentInfo_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamHTTP_SetHTTPRequestUserAgentInfo");
    if (!__SteamAPI_ISteamHTTP_SetHTTPRequestUserAgentInfo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamHTTP_SetHTTPRequestUserAgentInfo\n"); complete = false; }
    __SteamAPI_ISteamInput_ActivateActionSet_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_ActivateActionSet");
    if (!__SteamAPI_ISteamInput_ActivateActionSet_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_ActivateActionSet\n"); complete = false; }
    __SteamAPI_ISteamInput_ActivateActionSetLayer_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_ActivateActionSetLayer");
    if (!__SteamAPI_ISteamInput_ActivateActionSetLayer_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_ActivateActionSetLayer\n"); complete = false; }
    __SteamAPI_ISteamInput_BNewDataAvailable_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_BNewDataAvailable");
    if (!__SteamAPI_ISteamInput_BNewDataAvailable_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_BNewDataAvailable\n"); complete = false; }
    __SteamAPI_ISteamInput_BWaitForData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_BWaitForData");
    if (!__SteamAPI_ISteamInput_BWaitForData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_BWaitForData\n"); complete = false; }
    __SteamAPI_ISteamInput_DeactivateActionSetLayer_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_DeactivateActionSetLayer");
    if (!__SteamAPI_ISteamInput_DeactivateActionSetLayer_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_DeactivateActionSetLayer\n"); complete = false; }
    __SteamAPI_ISteamInput_DeactivateAllActionSetLayers_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_DeactivateAllActionSetLayers");
    if (!__SteamAPI_ISteamInput_DeactivateAllActionSetLayers_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_DeactivateAllActionSetLayers\n"); complete = false; }
    __SteamAPI_ISteamInput_EnableActionEventCallbacks_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_EnableActionEventCallbacks");
    if (!__SteamAPI_ISteamInput_EnableActionEventCallbacks_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_EnableActionEventCallbacks\n"); complete = false; }
    __SteamAPI_ISteamInput_EnableDeviceCallbacks_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_EnableDeviceCallbacks");
    if (!__SteamAPI_ISteamInput_EnableDeviceCallbacks_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_EnableDeviceCallbacks\n"); complete = false; }
    __SteamAPI_ISteamInput_GetActionOriginFromXboxOrigin_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetActionOriginFromXboxOrigin");
    if (!__SteamAPI_ISteamInput_GetActionOriginFromXboxOrigin_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetActionOriginFromXboxOrigin\n"); complete = false; }
    __SteamAPI_ISteamInput_GetActionSetHandle_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetActionSetHandle");
    if (!__SteamAPI_ISteamInput_GetActionSetHandle_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetActionSetHandle\n"); complete = false; }
    __SteamAPI_ISteamInput_GetActiveActionSetLayers_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetActiveActionSetLayers");
    if (!__SteamAPI_ISteamInput_GetActiveActionSetLayers_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetActiveActionSetLayers\n"); complete = false; }
    __SteamAPI_ISteamInput_GetAnalogActionData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetAnalogActionData");
    if (!__SteamAPI_ISteamInput_GetAnalogActionData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetAnalogActionData\n"); complete = false; }
    __SteamAPI_ISteamInput_GetAnalogActionHandle_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetAnalogActionHandle");
    if (!__SteamAPI_ISteamInput_GetAnalogActionHandle_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetAnalogActionHandle\n"); complete = false; }
    __SteamAPI_ISteamInput_GetAnalogActionOrigins_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetAnalogActionOrigins");
    if (!__SteamAPI_ISteamInput_GetAnalogActionOrigins_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetAnalogActionOrigins\n"); complete = false; }
    __SteamAPI_ISteamInput_GetConnectedControllers_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetConnectedControllers");
    if (!__SteamAPI_ISteamInput_GetConnectedControllers_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetConnectedControllers\n"); complete = false; }
    __SteamAPI_ISteamInput_GetControllerForGamepadIndex_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetControllerForGamepadIndex");
    if (!__SteamAPI_ISteamInput_GetControllerForGamepadIndex_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetControllerForGamepadIndex\n"); complete = false; }
    __SteamAPI_ISteamInput_GetCurrentActionSet_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetCurrentActionSet");
    if (!__SteamAPI_ISteamInput_GetCurrentActionSet_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetCurrentActionSet\n"); complete = false; }
    __SteamAPI_ISteamInput_GetDeviceBindingRevision_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetDeviceBindingRevision");
    if (!__SteamAPI_ISteamInput_GetDeviceBindingRevision_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetDeviceBindingRevision\n"); complete = false; }
    __SteamAPI_ISteamInput_GetDigitalActionData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetDigitalActionData");
    if (!__SteamAPI_ISteamInput_GetDigitalActionData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetDigitalActionData\n"); complete = false; }
    __SteamAPI_ISteamInput_GetDigitalActionHandle_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetDigitalActionHandle");
    if (!__SteamAPI_ISteamInput_GetDigitalActionHandle_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetDigitalActionHandle\n"); complete = false; }
    __SteamAPI_ISteamInput_GetDigitalActionOrigins_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetDigitalActionOrigins");
    if (!__SteamAPI_ISteamInput_GetDigitalActionOrigins_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetDigitalActionOrigins\n"); complete = false; }
    __SteamAPI_ISteamInput_GetGamepadIndexForController_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetGamepadIndexForController");
    if (!__SteamAPI_ISteamInput_GetGamepadIndexForController_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetGamepadIndexForController\n"); complete = false; }
    __SteamAPI_ISteamInput_GetGlyphForActionOrigin_Legacy_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetGlyphForActionOrigin_Legacy");
    if (!__SteamAPI_ISteamInput_GetGlyphForActionOrigin_Legacy_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetGlyphForActionOrigin_Legacy\n"); complete = false; }
    __SteamAPI_ISteamInput_GetGlyphForXboxOrigin_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetGlyphForXboxOrigin");
    if (!__SteamAPI_ISteamInput_GetGlyphForXboxOrigin_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetGlyphForXboxOrigin\n"); complete = false; }
    __SteamAPI_ISteamInput_GetGlyphPNGForActionOrigin_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetGlyphPNGForActionOrigin");
    if (!__SteamAPI_ISteamInput_GetGlyphPNGForActionOrigin_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetGlyphPNGForActionOrigin\n"); complete = false; }
    __SteamAPI_ISteamInput_GetGlyphSVGForActionOrigin_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetGlyphSVGForActionOrigin");
    if (!__SteamAPI_ISteamInput_GetGlyphSVGForActionOrigin_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetGlyphSVGForActionOrigin\n"); complete = false; }
    __SteamAPI_ISteamInput_GetInputTypeForHandle_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetInputTypeForHandle");
    if (!__SteamAPI_ISteamInput_GetInputTypeForHandle_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetInputTypeForHandle\n"); complete = false; }
    __SteamAPI_ISteamInput_GetMotionData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetMotionData");
    if (!__SteamAPI_ISteamInput_GetMotionData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetMotionData\n"); complete = false; }
    __SteamAPI_ISteamInput_GetRemotePlaySessionID_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetRemotePlaySessionID");
    if (!__SteamAPI_ISteamInput_GetRemotePlaySessionID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetRemotePlaySessionID\n"); complete = false; }
    __SteamAPI_ISteamInput_GetSessionInputConfigurationSettings_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetSessionInputConfigurationSettings");
    if (!__SteamAPI_ISteamInput_GetSessionInputConfigurationSettings_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetSessionInputConfigurationSettings\n"); complete = false; }
    __SteamAPI_ISteamInput_GetStringForActionOrigin_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetStringForActionOrigin");
    if (!__SteamAPI_ISteamInput_GetStringForActionOrigin_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetStringForActionOrigin\n"); complete = false; }
    __SteamAPI_ISteamInput_GetStringForAnalogActionName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetStringForAnalogActionName");
    if (!__SteamAPI_ISteamInput_GetStringForAnalogActionName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetStringForAnalogActionName\n"); complete = false; }
    __SteamAPI_ISteamInput_GetStringForDigitalActionName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetStringForDigitalActionName");
    if (!__SteamAPI_ISteamInput_GetStringForDigitalActionName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetStringForDigitalActionName\n"); complete = false; }
    __SteamAPI_ISteamInput_GetStringForXboxOrigin_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_GetStringForXboxOrigin");
    if (!__SteamAPI_ISteamInput_GetStringForXboxOrigin_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_GetStringForXboxOrigin\n"); complete = false; }
    __SteamAPI_ISteamInput_Init_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_Init");
    if (!__SteamAPI_ISteamInput_Init_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_Init\n"); complete = false; }
    __SteamAPI_ISteamInput_Legacy_TriggerHapticPulse_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_Legacy_TriggerHapticPulse");
    if (!__SteamAPI_ISteamInput_Legacy_TriggerHapticPulse_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_Legacy_TriggerHapticPulse\n"); complete = false; }
    __SteamAPI_ISteamInput_Legacy_TriggerRepeatedHapticPulse_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_Legacy_TriggerRepeatedHapticPulse");
    if (!__SteamAPI_ISteamInput_Legacy_TriggerRepeatedHapticPulse_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_Legacy_TriggerRepeatedHapticPulse\n"); complete = false; }
    __SteamAPI_ISteamInput_RunFrame_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_RunFrame");
    if (!__SteamAPI_ISteamInput_RunFrame_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_RunFrame\n"); complete = false; }
    __SteamAPI_ISteamInput_SetDualSenseTriggerEffect_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_SetDualSenseTriggerEffect");
    if (!__SteamAPI_ISteamInput_SetDualSenseTriggerEffect_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_SetDualSenseTriggerEffect\n"); complete = false; }
    __SteamAPI_ISteamInput_SetInputActionManifestFilePath_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_SetInputActionManifestFilePath");
    if (!__SteamAPI_ISteamInput_SetInputActionManifestFilePath_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_SetInputActionManifestFilePath\n"); complete = false; }
    __SteamAPI_ISteamInput_SetLEDColor_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_SetLEDColor");
    if (!__SteamAPI_ISteamInput_SetLEDColor_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_SetLEDColor\n"); complete = false; }
    __SteamAPI_ISteamInput_ShowBindingPanel_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_ShowBindingPanel");
    if (!__SteamAPI_ISteamInput_ShowBindingPanel_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_ShowBindingPanel\n"); complete = false; }
    __SteamAPI_ISteamInput_Shutdown_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_Shutdown");
    if (!__SteamAPI_ISteamInput_Shutdown_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_Shutdown\n"); complete = false; }
    __SteamAPI_ISteamInput_StopAnalogActionMomentum_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_StopAnalogActionMomentum");
    if (!__SteamAPI_ISteamInput_StopAnalogActionMomentum_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_StopAnalogActionMomentum\n"); complete = false; }
    __SteamAPI_ISteamInput_TranslateActionOrigin_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_TranslateActionOrigin");
    if (!__SteamAPI_ISteamInput_TranslateActionOrigin_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_TranslateActionOrigin\n"); complete = false; }
    __SteamAPI_ISteamInput_TriggerSimpleHapticEvent_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_TriggerSimpleHapticEvent");
    if (!__SteamAPI_ISteamInput_TriggerSimpleHapticEvent_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_TriggerSimpleHapticEvent\n"); complete = false; }
    __SteamAPI_ISteamInput_TriggerVibration_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_TriggerVibration");
    if (!__SteamAPI_ISteamInput_TriggerVibration_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_TriggerVibration\n"); complete = false; }
    __SteamAPI_ISteamInput_TriggerVibrationExtended_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInput_TriggerVibrationExtended");
    if (!__SteamAPI_ISteamInput_TriggerVibrationExtended_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInput_TriggerVibrationExtended\n"); complete = false; }
    __SteamAPI_ISteamInventory_AddPromoItem_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_AddPromoItem");
    if (!__SteamAPI_ISteamInventory_AddPromoItem_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_AddPromoItem\n"); complete = false; }
    __SteamAPI_ISteamInventory_AddPromoItems_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_AddPromoItems");
    if (!__SteamAPI_ISteamInventory_AddPromoItems_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_AddPromoItems\n"); complete = false; }
    __SteamAPI_ISteamInventory_CheckResultSteamID_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_CheckResultSteamID");
    if (!__SteamAPI_ISteamInventory_CheckResultSteamID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_CheckResultSteamID\n"); complete = false; }
    __SteamAPI_ISteamInventory_ConsumeItem_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_ConsumeItem");
    if (!__SteamAPI_ISteamInventory_ConsumeItem_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_ConsumeItem\n"); complete = false; }
    __SteamAPI_ISteamInventory_DeserializeResult_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_DeserializeResult");
    if (!__SteamAPI_ISteamInventory_DeserializeResult_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_DeserializeResult\n"); complete = false; }
    __SteamAPI_ISteamInventory_DestroyResult_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_DestroyResult");
    if (!__SteamAPI_ISteamInventory_DestroyResult_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_DestroyResult\n"); complete = false; }
    __SteamAPI_ISteamInventory_ExchangeItems_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_ExchangeItems");
    if (!__SteamAPI_ISteamInventory_ExchangeItems_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_ExchangeItems\n"); complete = false; }
    __SteamAPI_ISteamInventory_GenerateItems_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_GenerateItems");
    if (!__SteamAPI_ISteamInventory_GenerateItems_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_GenerateItems\n"); complete = false; }
    __SteamAPI_ISteamInventory_GetAllItems_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_GetAllItems");
    if (!__SteamAPI_ISteamInventory_GetAllItems_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_GetAllItems\n"); complete = false; }
    __SteamAPI_ISteamInventory_GetEligiblePromoItemDefinitionIDs_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_GetEligiblePromoItemDefinitionIDs");
    if (!__SteamAPI_ISteamInventory_GetEligiblePromoItemDefinitionIDs_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_GetEligiblePromoItemDefinitionIDs\n"); complete = false; }
    __SteamAPI_ISteamInventory_GetItemDefinitionIDs_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_GetItemDefinitionIDs");
    if (!__SteamAPI_ISteamInventory_GetItemDefinitionIDs_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_GetItemDefinitionIDs\n"); complete = false; }
    __SteamAPI_ISteamInventory_GetItemDefinitionProperty_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_GetItemDefinitionProperty");
    if (!__SteamAPI_ISteamInventory_GetItemDefinitionProperty_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_GetItemDefinitionProperty\n"); complete = false; }
    __SteamAPI_ISteamInventory_GetItemPrice_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_GetItemPrice");
    if (!__SteamAPI_ISteamInventory_GetItemPrice_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_GetItemPrice\n"); complete = false; }
    __SteamAPI_ISteamInventory_GetItemsByID_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_GetItemsByID");
    if (!__SteamAPI_ISteamInventory_GetItemsByID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_GetItemsByID\n"); complete = false; }
    __SteamAPI_ISteamInventory_GetItemsWithPrices_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_GetItemsWithPrices");
    if (!__SteamAPI_ISteamInventory_GetItemsWithPrices_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_GetItemsWithPrices\n"); complete = false; }
    __SteamAPI_ISteamInventory_GetNumItemsWithPrices_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_GetNumItemsWithPrices");
    if (!__SteamAPI_ISteamInventory_GetNumItemsWithPrices_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_GetNumItemsWithPrices\n"); complete = false; }
    __SteamAPI_ISteamInventory_GetResultItemProperty_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_GetResultItemProperty");
    if (!__SteamAPI_ISteamInventory_GetResultItemProperty_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_GetResultItemProperty\n"); complete = false; }
    __SteamAPI_ISteamInventory_GetResultItems_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_GetResultItems");
    if (!__SteamAPI_ISteamInventory_GetResultItems_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_GetResultItems\n"); complete = false; }
    __SteamAPI_ISteamInventory_GetResultStatus_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_GetResultStatus");
    if (!__SteamAPI_ISteamInventory_GetResultStatus_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_GetResultStatus\n"); complete = false; }
    __SteamAPI_ISteamInventory_GetResultTimestamp_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_GetResultTimestamp");
    if (!__SteamAPI_ISteamInventory_GetResultTimestamp_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_GetResultTimestamp\n"); complete = false; }
    __SteamAPI_ISteamInventory_GrantPromoItems_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_GrantPromoItems");
    if (!__SteamAPI_ISteamInventory_GrantPromoItems_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_GrantPromoItems\n"); complete = false; }
    __SteamAPI_ISteamInventory_InspectItem_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_InspectItem");
    if (!__SteamAPI_ISteamInventory_InspectItem_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_InspectItem\n"); complete = false; }
    __SteamAPI_ISteamInventory_LoadItemDefinitions_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_LoadItemDefinitions");
    if (!__SteamAPI_ISteamInventory_LoadItemDefinitions_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_LoadItemDefinitions\n"); complete = false; }
    __SteamAPI_ISteamInventory_RemoveProperty_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_RemoveProperty");
    if (!__SteamAPI_ISteamInventory_RemoveProperty_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_RemoveProperty\n"); complete = false; }
    __SteamAPI_ISteamInventory_RequestEligiblePromoItemDefinitionsIDs_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_RequestEligiblePromoItemDefinitionsIDs");
    if (!__SteamAPI_ISteamInventory_RequestEligiblePromoItemDefinitionsIDs_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_RequestEligiblePromoItemDefinitionsIDs\n"); complete = false; }
    __SteamAPI_ISteamInventory_RequestPrices_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_RequestPrices");
    if (!__SteamAPI_ISteamInventory_RequestPrices_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_RequestPrices\n"); complete = false; }
    __SteamAPI_ISteamInventory_SendItemDropHeartbeat_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_SendItemDropHeartbeat");
    if (!__SteamAPI_ISteamInventory_SendItemDropHeartbeat_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_SendItemDropHeartbeat\n"); complete = false; }
    __SteamAPI_ISteamInventory_SerializeResult_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_SerializeResult");
    if (!__SteamAPI_ISteamInventory_SerializeResult_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_SerializeResult\n"); complete = false; }
    __SteamAPI_ISteamInventory_SetPropertyBool_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_SetPropertyBool");
    if (!__SteamAPI_ISteamInventory_SetPropertyBool_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_SetPropertyBool\n"); complete = false; }
    __SteamAPI_ISteamInventory_SetPropertyFloat_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_SetPropertyFloat");
    if (!__SteamAPI_ISteamInventory_SetPropertyFloat_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_SetPropertyFloat\n"); complete = false; }
    __SteamAPI_ISteamInventory_SetPropertyInt64_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_SetPropertyInt64");
    if (!__SteamAPI_ISteamInventory_SetPropertyInt64_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_SetPropertyInt64\n"); complete = false; }
    __SteamAPI_ISteamInventory_SetPropertyString_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_SetPropertyString");
    if (!__SteamAPI_ISteamInventory_SetPropertyString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_SetPropertyString\n"); complete = false; }
    __SteamAPI_ISteamInventory_StartPurchase_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_StartPurchase");
    if (!__SteamAPI_ISteamInventory_StartPurchase_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_StartPurchase\n"); complete = false; }
    __SteamAPI_ISteamInventory_StartUpdateProperties_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_StartUpdateProperties");
    if (!__SteamAPI_ISteamInventory_StartUpdateProperties_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_StartUpdateProperties\n"); complete = false; }
    __SteamAPI_ISteamInventory_SubmitUpdateProperties_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_SubmitUpdateProperties");
    if (!__SteamAPI_ISteamInventory_SubmitUpdateProperties_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_SubmitUpdateProperties\n"); complete = false; }
    __SteamAPI_ISteamInventory_TradeItems_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_TradeItems");
    if (!__SteamAPI_ISteamInventory_TradeItems_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_TradeItems\n"); complete = false; }
    __SteamAPI_ISteamInventory_TransferItemQuantity_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_TransferItemQuantity");
    if (!__SteamAPI_ISteamInventory_TransferItemQuantity_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_TransferItemQuantity\n"); complete = false; }
    __SteamAPI_ISteamInventory_TriggerItemDrop_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamInventory_TriggerItemDrop");
    if (!__SteamAPI_ISteamInventory_TriggerItemDrop_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamInventory_TriggerItemDrop\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingPingResponse_ServerFailedToRespond_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingPingResponse_ServerFailedToRespond");
    if (!__SteamAPI_ISteamMatchmakingPingResponse_ServerFailedToRespond_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingPingResponse_ServerFailedToRespond\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingPingResponse_ServerResponded_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingPingResponse_ServerResponded");
    if (!__SteamAPI_ISteamMatchmakingPingResponse_ServerResponded_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingPingResponse_ServerResponded\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingPlayersResponse_AddPlayerToList_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingPlayersResponse_AddPlayerToList");
    if (!__SteamAPI_ISteamMatchmakingPlayersResponse_AddPlayerToList_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingPlayersResponse_AddPlayerToList\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingPlayersResponse_PlayersFailedToRespond_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingPlayersResponse_PlayersFailedToRespond");
    if (!__SteamAPI_ISteamMatchmakingPlayersResponse_PlayersFailedToRespond_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingPlayersResponse_PlayersFailedToRespond\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingPlayersResponse_PlayersRefreshComplete_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingPlayersResponse_PlayersRefreshComplete");
    if (!__SteamAPI_ISteamMatchmakingPlayersResponse_PlayersRefreshComplete_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingPlayersResponse_PlayersRefreshComplete\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingRulesResponse_RulesFailedToRespond_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingRulesResponse_RulesFailedToRespond");
    if (!__SteamAPI_ISteamMatchmakingRulesResponse_RulesFailedToRespond_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingRulesResponse_RulesFailedToRespond\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingRulesResponse_RulesRefreshComplete_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingRulesResponse_RulesRefreshComplete");
    if (!__SteamAPI_ISteamMatchmakingRulesResponse_RulesRefreshComplete_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingRulesResponse_RulesRefreshComplete\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingRulesResponse_RulesResponded_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingRulesResponse_RulesResponded");
    if (!__SteamAPI_ISteamMatchmakingRulesResponse_RulesResponded_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingRulesResponse_RulesResponded\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServerListResponse_RefreshComplete_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServerListResponse_RefreshComplete");
    if (!__SteamAPI_ISteamMatchmakingServerListResponse_RefreshComplete_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServerListResponse_RefreshComplete\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServerListResponse_ServerFailedToRespond_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServerListResponse_ServerFailedToRespond");
    if (!__SteamAPI_ISteamMatchmakingServerListResponse_ServerFailedToRespond_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServerListResponse_ServerFailedToRespond\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServerListResponse_ServerResponded_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServerListResponse_ServerResponded");
    if (!__SteamAPI_ISteamMatchmakingServerListResponse_ServerResponded_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServerListResponse_ServerResponded\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_CancelQuery_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_CancelQuery");
    if (!__SteamAPI_ISteamMatchmakingServers_CancelQuery_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_CancelQuery\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_CancelServerQuery_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_CancelServerQuery");
    if (!__SteamAPI_ISteamMatchmakingServers_CancelServerQuery_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_CancelServerQuery\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_GetServerCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_GetServerCount");
    if (!__SteamAPI_ISteamMatchmakingServers_GetServerCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_GetServerCount\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_GetServerDetails_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_GetServerDetails");
    if (!__SteamAPI_ISteamMatchmakingServers_GetServerDetails_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_GetServerDetails\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_IsRefreshing_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_IsRefreshing");
    if (!__SteamAPI_ISteamMatchmakingServers_IsRefreshing_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_IsRefreshing\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_PingServer_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_PingServer");
    if (!__SteamAPI_ISteamMatchmakingServers_PingServer_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_PingServer\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_PlayerDetails_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_PlayerDetails");
    if (!__SteamAPI_ISteamMatchmakingServers_PlayerDetails_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_PlayerDetails\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_RefreshQuery_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_RefreshQuery");
    if (!__SteamAPI_ISteamMatchmakingServers_RefreshQuery_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_RefreshQuery\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_RefreshServer_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_RefreshServer");
    if (!__SteamAPI_ISteamMatchmakingServers_RefreshServer_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_RefreshServer\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_ReleaseRequest_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_ReleaseRequest");
    if (!__SteamAPI_ISteamMatchmakingServers_ReleaseRequest_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_ReleaseRequest\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_RequestFavoritesServerList_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_RequestFavoritesServerList");
    if (!__SteamAPI_ISteamMatchmakingServers_RequestFavoritesServerList_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_RequestFavoritesServerList\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_RequestFriendsServerList_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_RequestFriendsServerList");
    if (!__SteamAPI_ISteamMatchmakingServers_RequestFriendsServerList_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_RequestFriendsServerList\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_RequestHistoryServerList_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_RequestHistoryServerList");
    if (!__SteamAPI_ISteamMatchmakingServers_RequestHistoryServerList_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_RequestHistoryServerList\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_RequestInternetServerList_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_RequestInternetServerList");
    if (!__SteamAPI_ISteamMatchmakingServers_RequestInternetServerList_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_RequestInternetServerList\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_RequestLANServerList_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_RequestLANServerList");
    if (!__SteamAPI_ISteamMatchmakingServers_RequestLANServerList_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_RequestLANServerList\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_RequestSpectatorServerList_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_RequestSpectatorServerList");
    if (!__SteamAPI_ISteamMatchmakingServers_RequestSpectatorServerList_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_RequestSpectatorServerList\n"); complete = false; }
    __SteamAPI_ISteamMatchmakingServers_ServerRules_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmakingServers_ServerRules");
    if (!__SteamAPI_ISteamMatchmakingServers_ServerRules_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmakingServers_ServerRules\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_AddFavoriteGame_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_AddFavoriteGame");
    if (!__SteamAPI_ISteamMatchmaking_AddFavoriteGame_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_AddFavoriteGame\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_AddRequestLobbyListCompatibleMembersFilter_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListCompatibleMembersFilter");
    if (!__SteamAPI_ISteamMatchmaking_AddRequestLobbyListCompatibleMembersFilter_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_AddRequestLobbyListCompatibleMembersFilter\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter");
    if (!__SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_AddRequestLobbyListFilterSlotsAvailable_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListFilterSlotsAvailable");
    if (!__SteamAPI_ISteamMatchmaking_AddRequestLobbyListFilterSlotsAvailable_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_AddRequestLobbyListFilterSlotsAvailable\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_AddRequestLobbyListNearValueFilter_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListNearValueFilter");
    if (!__SteamAPI_ISteamMatchmaking_AddRequestLobbyListNearValueFilter_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_AddRequestLobbyListNearValueFilter\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_AddRequestLobbyListNumericalFilter_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListNumericalFilter");
    if (!__SteamAPI_ISteamMatchmaking_AddRequestLobbyListNumericalFilter_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_AddRequestLobbyListNumericalFilter\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter");
    if (!__SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter");
    if (!__SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_CreateLobby_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_CreateLobby");
    if (!__SteamAPI_ISteamMatchmaking_CreateLobby_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_CreateLobby\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_DeleteLobbyData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_DeleteLobbyData");
    if (!__SteamAPI_ISteamMatchmaking_DeleteLobbyData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_DeleteLobbyData\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_GetFavoriteGame_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_GetFavoriteGame");
    if (!__SteamAPI_ISteamMatchmaking_GetFavoriteGame_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_GetFavoriteGame\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_GetFavoriteGameCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_GetFavoriteGameCount");
    if (!__SteamAPI_ISteamMatchmaking_GetFavoriteGameCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_GetFavoriteGameCount\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_GetLobbyByIndex_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_GetLobbyByIndex");
    if (!__SteamAPI_ISteamMatchmaking_GetLobbyByIndex_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_GetLobbyByIndex\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_GetLobbyChatEntry_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_GetLobbyChatEntry");
    if (!__SteamAPI_ISteamMatchmaking_GetLobbyChatEntry_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_GetLobbyChatEntry\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_GetLobbyData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_GetLobbyData");
    if (!__SteamAPI_ISteamMatchmaking_GetLobbyData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_GetLobbyData\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex");
    if (!__SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_GetLobbyDataCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_GetLobbyDataCount");
    if (!__SteamAPI_ISteamMatchmaking_GetLobbyDataCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_GetLobbyDataCount\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_GetLobbyGameServer_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_GetLobbyGameServer");
    if (!__SteamAPI_ISteamMatchmaking_GetLobbyGameServer_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_GetLobbyGameServer\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex");
    if (!__SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_GetLobbyMemberData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_GetLobbyMemberData");
    if (!__SteamAPI_ISteamMatchmaking_GetLobbyMemberData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_GetLobbyMemberData\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit");
    if (!__SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_GetLobbyOwner_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_GetLobbyOwner");
    if (!__SteamAPI_ISteamMatchmaking_GetLobbyOwner_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_GetLobbyOwner\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_GetNumLobbyMembers_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_GetNumLobbyMembers");
    if (!__SteamAPI_ISteamMatchmaking_GetNumLobbyMembers_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_GetNumLobbyMembers\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_InviteUserToLobby_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_InviteUserToLobby");
    if (!__SteamAPI_ISteamMatchmaking_InviteUserToLobby_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_InviteUserToLobby\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_JoinLobby_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_JoinLobby");
    if (!__SteamAPI_ISteamMatchmaking_JoinLobby_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_JoinLobby\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_LeaveLobby_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_LeaveLobby");
    if (!__SteamAPI_ISteamMatchmaking_LeaveLobby_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_LeaveLobby\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_RemoveFavoriteGame_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_RemoveFavoriteGame");
    if (!__SteamAPI_ISteamMatchmaking_RemoveFavoriteGame_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_RemoveFavoriteGame\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_RequestLobbyData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_RequestLobbyData");
    if (!__SteamAPI_ISteamMatchmaking_RequestLobbyData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_RequestLobbyData\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_RequestLobbyList_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_RequestLobbyList");
    if (!__SteamAPI_ISteamMatchmaking_RequestLobbyList_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_RequestLobbyList\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_SendLobbyChatMsg_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_SendLobbyChatMsg");
    if (!__SteamAPI_ISteamMatchmaking_SendLobbyChatMsg_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_SendLobbyChatMsg\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_SetLinkedLobby_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_SetLinkedLobby");
    if (!__SteamAPI_ISteamMatchmaking_SetLinkedLobby_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_SetLinkedLobby\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_SetLobbyData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_SetLobbyData");
    if (!__SteamAPI_ISteamMatchmaking_SetLobbyData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_SetLobbyData\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_SetLobbyGameServer_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_SetLobbyGameServer");
    if (!__SteamAPI_ISteamMatchmaking_SetLobbyGameServer_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_SetLobbyGameServer\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_SetLobbyJoinable_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_SetLobbyJoinable");
    if (!__SteamAPI_ISteamMatchmaking_SetLobbyJoinable_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_SetLobbyJoinable\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_SetLobbyMemberData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_SetLobbyMemberData");
    if (!__SteamAPI_ISteamMatchmaking_SetLobbyMemberData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_SetLobbyMemberData\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_SetLobbyMemberLimit_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_SetLobbyMemberLimit");
    if (!__SteamAPI_ISteamMatchmaking_SetLobbyMemberLimit_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_SetLobbyMemberLimit\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_SetLobbyOwner_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_SetLobbyOwner");
    if (!__SteamAPI_ISteamMatchmaking_SetLobbyOwner_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_SetLobbyOwner\n"); complete = false; }
    __SteamAPI_ISteamMatchmaking_SetLobbyType_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMatchmaking_SetLobbyType");
    if (!__SteamAPI_ISteamMatchmaking_SetLobbyType_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMatchmaking_SetLobbyType\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_BActivationSuccess_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_BActivationSuccess");
    if (!__SteamAPI_ISteamMusicRemote_BActivationSuccess_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_BActivationSuccess\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_BIsCurrentMusicRemote_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_BIsCurrentMusicRemote");
    if (!__SteamAPI_ISteamMusicRemote_BIsCurrentMusicRemote_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_BIsCurrentMusicRemote\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_CurrentEntryDidChange_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_CurrentEntryDidChange");
    if (!__SteamAPI_ISteamMusicRemote_CurrentEntryDidChange_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_CurrentEntryDidChange\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_CurrentEntryIsAvailable_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_CurrentEntryIsAvailable");
    if (!__SteamAPI_ISteamMusicRemote_CurrentEntryIsAvailable_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_CurrentEntryIsAvailable\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_CurrentEntryWillChange_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_CurrentEntryWillChange");
    if (!__SteamAPI_ISteamMusicRemote_CurrentEntryWillChange_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_CurrentEntryWillChange\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_DeregisterSteamMusicRemote_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_DeregisterSteamMusicRemote");
    if (!__SteamAPI_ISteamMusicRemote_DeregisterSteamMusicRemote_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_DeregisterSteamMusicRemote\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_EnableLooped_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_EnableLooped");
    if (!__SteamAPI_ISteamMusicRemote_EnableLooped_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_EnableLooped\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_EnablePlayNext_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_EnablePlayNext");
    if (!__SteamAPI_ISteamMusicRemote_EnablePlayNext_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_EnablePlayNext\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_EnablePlayPrevious_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_EnablePlayPrevious");
    if (!__SteamAPI_ISteamMusicRemote_EnablePlayPrevious_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_EnablePlayPrevious\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_EnablePlaylists_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_EnablePlaylists");
    if (!__SteamAPI_ISteamMusicRemote_EnablePlaylists_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_EnablePlaylists\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_EnableQueue_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_EnableQueue");
    if (!__SteamAPI_ISteamMusicRemote_EnableQueue_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_EnableQueue\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_EnableShuffled_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_EnableShuffled");
    if (!__SteamAPI_ISteamMusicRemote_EnableShuffled_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_EnableShuffled\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_PlaylistDidChange_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_PlaylistDidChange");
    if (!__SteamAPI_ISteamMusicRemote_PlaylistDidChange_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_PlaylistDidChange\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_PlaylistWillChange_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_PlaylistWillChange");
    if (!__SteamAPI_ISteamMusicRemote_PlaylistWillChange_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_PlaylistWillChange\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_QueueDidChange_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_QueueDidChange");
    if (!__SteamAPI_ISteamMusicRemote_QueueDidChange_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_QueueDidChange\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_QueueWillChange_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_QueueWillChange");
    if (!__SteamAPI_ISteamMusicRemote_QueueWillChange_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_QueueWillChange\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_RegisterSteamMusicRemote_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_RegisterSteamMusicRemote");
    if (!__SteamAPI_ISteamMusicRemote_RegisterSteamMusicRemote_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_RegisterSteamMusicRemote\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_ResetPlaylistEntries_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_ResetPlaylistEntries");
    if (!__SteamAPI_ISteamMusicRemote_ResetPlaylistEntries_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_ResetPlaylistEntries\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_ResetQueueEntries_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_ResetQueueEntries");
    if (!__SteamAPI_ISteamMusicRemote_ResetQueueEntries_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_ResetQueueEntries\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_SetCurrentPlaylistEntry_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_SetCurrentPlaylistEntry");
    if (!__SteamAPI_ISteamMusicRemote_SetCurrentPlaylistEntry_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_SetCurrentPlaylistEntry\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_SetCurrentQueueEntry_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_SetCurrentQueueEntry");
    if (!__SteamAPI_ISteamMusicRemote_SetCurrentQueueEntry_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_SetCurrentQueueEntry\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_SetDisplayName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_SetDisplayName");
    if (!__SteamAPI_ISteamMusicRemote_SetDisplayName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_SetDisplayName\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_SetPNGIcon_64x64_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_SetPNGIcon_64x64");
    if (!__SteamAPI_ISteamMusicRemote_SetPNGIcon_64x64_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_SetPNGIcon_64x64\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_SetPlaylistEntry_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_SetPlaylistEntry");
    if (!__SteamAPI_ISteamMusicRemote_SetPlaylistEntry_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_SetPlaylistEntry\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_SetQueueEntry_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_SetQueueEntry");
    if (!__SteamAPI_ISteamMusicRemote_SetQueueEntry_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_SetQueueEntry\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_UpdateCurrentEntryCoverArt_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_UpdateCurrentEntryCoverArt");
    if (!__SteamAPI_ISteamMusicRemote_UpdateCurrentEntryCoverArt_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_UpdateCurrentEntryCoverArt\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_UpdateCurrentEntryElapsedSeconds_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_UpdateCurrentEntryElapsedSeconds");
    if (!__SteamAPI_ISteamMusicRemote_UpdateCurrentEntryElapsedSeconds_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_UpdateCurrentEntryElapsedSeconds\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_UpdateCurrentEntryText_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_UpdateCurrentEntryText");
    if (!__SteamAPI_ISteamMusicRemote_UpdateCurrentEntryText_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_UpdateCurrentEntryText\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_UpdateLooped_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_UpdateLooped");
    if (!__SteamAPI_ISteamMusicRemote_UpdateLooped_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_UpdateLooped\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_UpdatePlaybackStatus_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_UpdatePlaybackStatus");
    if (!__SteamAPI_ISteamMusicRemote_UpdatePlaybackStatus_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_UpdatePlaybackStatus\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_UpdateShuffled_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_UpdateShuffled");
    if (!__SteamAPI_ISteamMusicRemote_UpdateShuffled_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_UpdateShuffled\n"); complete = false; }
    __SteamAPI_ISteamMusicRemote_UpdateVolume_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusicRemote_UpdateVolume");
    if (!__SteamAPI_ISteamMusicRemote_UpdateVolume_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusicRemote_UpdateVolume\n"); complete = false; }
    __SteamAPI_ISteamMusic_BIsEnabled_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusic_BIsEnabled");
    if (!__SteamAPI_ISteamMusic_BIsEnabled_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusic_BIsEnabled\n"); complete = false; }
    __SteamAPI_ISteamMusic_BIsPlaying_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusic_BIsPlaying");
    if (!__SteamAPI_ISteamMusic_BIsPlaying_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusic_BIsPlaying\n"); complete = false; }
    __SteamAPI_ISteamMusic_GetPlaybackStatus_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusic_GetPlaybackStatus");
    if (!__SteamAPI_ISteamMusic_GetPlaybackStatus_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusic_GetPlaybackStatus\n"); complete = false; }
    __SteamAPI_ISteamMusic_GetVolume_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusic_GetVolume");
    if (!__SteamAPI_ISteamMusic_GetVolume_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusic_GetVolume\n"); complete = false; }
    __SteamAPI_ISteamMusic_Pause_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusic_Pause");
    if (!__SteamAPI_ISteamMusic_Pause_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusic_Pause\n"); complete = false; }
    __SteamAPI_ISteamMusic_Play_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusic_Play");
    if (!__SteamAPI_ISteamMusic_Play_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusic_Play\n"); complete = false; }
    __SteamAPI_ISteamMusic_PlayNext_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusic_PlayNext");
    if (!__SteamAPI_ISteamMusic_PlayNext_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusic_PlayNext\n"); complete = false; }
    __SteamAPI_ISteamMusic_PlayPrevious_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusic_PlayPrevious");
    if (!__SteamAPI_ISteamMusic_PlayPrevious_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusic_PlayPrevious\n"); complete = false; }
    __SteamAPI_ISteamMusic_SetVolume_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamMusic_SetVolume");
    if (!__SteamAPI_ISteamMusic_SetVolume_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamMusic_SetVolume\n"); complete = false; }
    __SteamAPI_ISteamNetworkingFakeUDPPort_DestroyFakeUDPPort_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingFakeUDPPort_DestroyFakeUDPPort");
    if (!__SteamAPI_ISteamNetworkingFakeUDPPort_DestroyFakeUDPPort_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingFakeUDPPort_DestroyFakeUDPPort\n"); complete = false; }
    __SteamAPI_ISteamNetworkingFakeUDPPort_ReceiveMessages_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingFakeUDPPort_ReceiveMessages");
    if (!__SteamAPI_ISteamNetworkingFakeUDPPort_ReceiveMessages_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingFakeUDPPort_ReceiveMessages\n"); complete = false; }
    __SteamAPI_ISteamNetworkingFakeUDPPort_ScheduleCleanup_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingFakeUDPPort_ScheduleCleanup");
    if (!__SteamAPI_ISteamNetworkingFakeUDPPort_ScheduleCleanup_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingFakeUDPPort_ScheduleCleanup\n"); complete = false; }
    __SteamAPI_ISteamNetworkingFakeUDPPort_SendMessageToFakeIP_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingFakeUDPPort_SendMessageToFakeIP");
    if (!__SteamAPI_ISteamNetworkingFakeUDPPort_SendMessageToFakeIP_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingFakeUDPPort_SendMessageToFakeIP\n"); complete = false; }
    __SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser");
    if (!__SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser\n"); complete = false; }
    __SteamAPI_ISteamNetworkingMessages_CloseChannelWithUser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingMessages_CloseChannelWithUser");
    if (!__SteamAPI_ISteamNetworkingMessages_CloseChannelWithUser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingMessages_CloseChannelWithUser\n"); complete = false; }
    __SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser");
    if (!__SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser\n"); complete = false; }
    __SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo");
    if (!__SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo\n"); complete = false; }
    __SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel");
    if (!__SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel\n"); complete = false; }
    __SteamAPI_ISteamNetworkingMessages_SendMessageToUser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingMessages_SendMessageToUser");
    if (!__SteamAPI_ISteamNetworkingMessages_SendMessageToUser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingMessages_SendMessageToUser\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_AcceptConnection_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_AcceptConnection");
    if (!__SteamAPI_ISteamNetworkingSockets_AcceptConnection_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_AcceptConnection\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_BeginAsyncRequestFakeIP_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_BeginAsyncRequestFakeIP");
    if (!__SteamAPI_ISteamNetworkingSockets_BeginAsyncRequestFakeIP_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_BeginAsyncRequestFakeIP\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_CloseConnection_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_CloseConnection");
    if (!__SteamAPI_ISteamNetworkingSockets_CloseConnection_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_CloseConnection\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_CloseListenSocket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_CloseListenSocket");
    if (!__SteamAPI_ISteamNetworkingSockets_CloseListenSocket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_CloseListenSocket\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_ConfigureConnectionLanes_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_ConfigureConnectionLanes");
    if (!__SteamAPI_ISteamNetworkingSockets_ConfigureConnectionLanes_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_ConfigureConnectionLanes\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_ConnectByIPAddress_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_ConnectByIPAddress");
    if (!__SteamAPI_ISteamNetworkingSockets_ConnectByIPAddress_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_ConnectByIPAddress\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_ConnectP2P_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_ConnectP2P");
    if (!__SteamAPI_ISteamNetworkingSockets_ConnectP2P_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_ConnectP2P\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_ConnectP2PCustomSignaling_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_ConnectP2PCustomSignaling");
    if (!__SteamAPI_ISteamNetworkingSockets_ConnectP2PCustomSignaling_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_ConnectP2PCustomSignaling\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_ConnectToHostedDedicatedServer_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_ConnectToHostedDedicatedServer");
    if (!__SteamAPI_ISteamNetworkingSockets_ConnectToHostedDedicatedServer_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_ConnectToHostedDedicatedServer\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_CreateFakeUDPPort_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_CreateFakeUDPPort");
    if (!__SteamAPI_ISteamNetworkingSockets_CreateFakeUDPPort_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_CreateFakeUDPPort\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_CreateHostedDedicatedServerListenSocket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_CreateHostedDedicatedServerListenSocket");
    if (!__SteamAPI_ISteamNetworkingSockets_CreateHostedDedicatedServerListenSocket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_CreateHostedDedicatedServerListenSocket\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_CreateListenSocketIP_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_CreateListenSocketIP");
    if (!__SteamAPI_ISteamNetworkingSockets_CreateListenSocketIP_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_CreateListenSocketIP\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P");
    if (!__SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2PFakeIP_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2PFakeIP");
    if (!__SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2PFakeIP_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2PFakeIP\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_CreatePollGroup_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_CreatePollGroup");
    if (!__SteamAPI_ISteamNetworkingSockets_CreatePollGroup_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_CreatePollGroup\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_CreateSocketPair_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_CreateSocketPair");
    if (!__SteamAPI_ISteamNetworkingSockets_CreateSocketPair_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_CreateSocketPair\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_DestroyPollGroup_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_DestroyPollGroup");
    if (!__SteamAPI_ISteamNetworkingSockets_DestroyPollGroup_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_DestroyPollGroup\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_FindRelayAuthTicketForServer_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_FindRelayAuthTicketForServer");
    if (!__SteamAPI_ISteamNetworkingSockets_FindRelayAuthTicketForServer_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_FindRelayAuthTicketForServer\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_FlushMessagesOnConnection_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_FlushMessagesOnConnection");
    if (!__SteamAPI_ISteamNetworkingSockets_FlushMessagesOnConnection_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_FlushMessagesOnConnection\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_GetAuthenticationStatus_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_GetAuthenticationStatus");
    if (!__SteamAPI_ISteamNetworkingSockets_GetAuthenticationStatus_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_GetAuthenticationStatus\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_GetCertificateRequest_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_GetCertificateRequest");
    if (!__SteamAPI_ISteamNetworkingSockets_GetCertificateRequest_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_GetCertificateRequest\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_GetConnectionInfo_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_GetConnectionInfo");
    if (!__SteamAPI_ISteamNetworkingSockets_GetConnectionInfo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_GetConnectionInfo\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_GetConnectionName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_GetConnectionName");
    if (!__SteamAPI_ISteamNetworkingSockets_GetConnectionName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_GetConnectionName\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_GetConnectionRealTimeStatus_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_GetConnectionRealTimeStatus");
    if (!__SteamAPI_ISteamNetworkingSockets_GetConnectionRealTimeStatus_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_GetConnectionRealTimeStatus\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_GetConnectionUserData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_GetConnectionUserData");
    if (!__SteamAPI_ISteamNetworkingSockets_GetConnectionUserData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_GetConnectionUserData\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_GetDetailedConnectionStatus_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_GetDetailedConnectionStatus");
    if (!__SteamAPI_ISteamNetworkingSockets_GetDetailedConnectionStatus_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_GetDetailedConnectionStatus\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_GetFakeIP_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_GetFakeIP");
    if (!__SteamAPI_ISteamNetworkingSockets_GetFakeIP_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_GetFakeIP\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_GetGameCoordinatorServerLogin_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_GetGameCoordinatorServerLogin");
    if (!__SteamAPI_ISteamNetworkingSockets_GetGameCoordinatorServerLogin_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_GetGameCoordinatorServerLogin\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerAddress_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerAddress");
    if (!__SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerAddress_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerAddress\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPOPID_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPOPID");
    if (!__SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPOPID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPOPID\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPort_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPort");
    if (!__SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPort_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPort\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_GetIdentity_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_GetIdentity");
    if (!__SteamAPI_ISteamNetworkingSockets_GetIdentity_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_GetIdentity\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_GetListenSocketAddress_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_GetListenSocketAddress");
    if (!__SteamAPI_ISteamNetworkingSockets_GetListenSocketAddress_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_GetListenSocketAddress\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_GetRemoteFakeIPForConnection_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_GetRemoteFakeIPForConnection");
    if (!__SteamAPI_ISteamNetworkingSockets_GetRemoteFakeIPForConnection_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_GetRemoteFakeIPForConnection\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_InitAuthentication_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_InitAuthentication");
    if (!__SteamAPI_ISteamNetworkingSockets_InitAuthentication_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_InitAuthentication\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection");
    if (!__SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnPollGroup_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnPollGroup");
    if (!__SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnPollGroup_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnPollGroup\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_ReceivedP2PCustomSignal_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_ReceivedP2PCustomSignal");
    if (!__SteamAPI_ISteamNetworkingSockets_ReceivedP2PCustomSignal_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_ReceivedP2PCustomSignal\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_ReceivedRelayAuthTicket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_ReceivedRelayAuthTicket");
    if (!__SteamAPI_ISteamNetworkingSockets_ReceivedRelayAuthTicket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_ReceivedRelayAuthTicket\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_ResetIdentity_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_ResetIdentity");
    if (!__SteamAPI_ISteamNetworkingSockets_ResetIdentity_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_ResetIdentity\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_RunCallbacks_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_RunCallbacks");
    if (!__SteamAPI_ISteamNetworkingSockets_RunCallbacks_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_RunCallbacks\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_SendMessageToConnection_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_SendMessageToConnection");
    if (!__SteamAPI_ISteamNetworkingSockets_SendMessageToConnection_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_SendMessageToConnection\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_SendMessages_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_SendMessages");
    if (!__SteamAPI_ISteamNetworkingSockets_SendMessages_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_SendMessages\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_SetCertificate_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_SetCertificate");
    if (!__SteamAPI_ISteamNetworkingSockets_SetCertificate_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_SetCertificate\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_SetConnectionName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_SetConnectionName");
    if (!__SteamAPI_ISteamNetworkingSockets_SetConnectionName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_SetConnectionName\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_SetConnectionPollGroup_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_SetConnectionPollGroup");
    if (!__SteamAPI_ISteamNetworkingSockets_SetConnectionPollGroup_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_SetConnectionPollGroup\n"); complete = false; }
    __SteamAPI_ISteamNetworkingSockets_SetConnectionUserData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingSockets_SetConnectionUserData");
    if (!__SteamAPI_ISteamNetworkingSockets_SetConnectionUserData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingSockets_SetConnectionUserData\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_AllocateMessage_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_AllocateMessage");
    if (!__SteamAPI_ISteamNetworkingUtils_AllocateMessage_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_AllocateMessage\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_CheckPingDataUpToDate_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_CheckPingDataUpToDate");
    if (!__SteamAPI_ISteamNetworkingUtils_CheckPingDataUpToDate_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_CheckPingDataUpToDate\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_ConvertPingLocationToString_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_ConvertPingLocationToString");
    if (!__SteamAPI_ISteamNetworkingUtils_ConvertPingLocationToString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_ConvertPingLocationToString\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_EstimatePingTimeBetweenTwoLocations_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_EstimatePingTimeBetweenTwoLocations");
    if (!__SteamAPI_ISteamNetworkingUtils_EstimatePingTimeBetweenTwoLocations_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_EstimatePingTimeBetweenTwoLocations\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_EstimatePingTimeFromLocalHost_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_EstimatePingTimeFromLocalHost");
    if (!__SteamAPI_ISteamNetworkingUtils_EstimatePingTimeFromLocalHost_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_EstimatePingTimeFromLocalHost\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_GetConfigValue_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_GetConfigValue");
    if (!__SteamAPI_ISteamNetworkingUtils_GetConfigValue_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_GetConfigValue\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_GetConfigValueInfo_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_GetConfigValueInfo");
    if (!__SteamAPI_ISteamNetworkingUtils_GetConfigValueInfo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_GetConfigValueInfo\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_GetDirectPingToPOP_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_GetDirectPingToPOP");
    if (!__SteamAPI_ISteamNetworkingUtils_GetDirectPingToPOP_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_GetDirectPingToPOP\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_GetIPv4FakeIPType_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_GetIPv4FakeIPType");
    if (!__SteamAPI_ISteamNetworkingUtils_GetIPv4FakeIPType_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_GetIPv4FakeIPType\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_GetLocalPingLocation_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_GetLocalPingLocation");
    if (!__SteamAPI_ISteamNetworkingUtils_GetLocalPingLocation_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_GetLocalPingLocation\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_GetLocalTimestamp_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_GetLocalTimestamp");
    if (!__SteamAPI_ISteamNetworkingUtils_GetLocalTimestamp_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_GetLocalTimestamp\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_GetPOPCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_GetPOPCount");
    if (!__SteamAPI_ISteamNetworkingUtils_GetPOPCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_GetPOPCount\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_GetPOPList_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_GetPOPList");
    if (!__SteamAPI_ISteamNetworkingUtils_GetPOPList_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_GetPOPList\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_GetPingToDataCenter_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_GetPingToDataCenter");
    if (!__SteamAPI_ISteamNetworkingUtils_GetPingToDataCenter_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_GetPingToDataCenter\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_GetRealIdentityForFakeIP_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_GetRealIdentityForFakeIP");
    if (!__SteamAPI_ISteamNetworkingUtils_GetRealIdentityForFakeIP_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_GetRealIdentityForFakeIP\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus");
    if (!__SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess");
    if (!__SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_IsFakeIPv4_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_IsFakeIPv4");
    if (!__SteamAPI_ISteamNetworkingUtils_IsFakeIPv4_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_IsFakeIPv4\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_IterateGenericEditableConfigValues_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_IterateGenericEditableConfigValues");
    if (!__SteamAPI_ISteamNetworkingUtils_IterateGenericEditableConfigValues_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_IterateGenericEditableConfigValues\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_ParsePingLocationString_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_ParsePingLocationString");
    if (!__SteamAPI_ISteamNetworkingUtils_ParsePingLocationString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_ParsePingLocationString\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SetConfigValue_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SetConfigValue");
    if (!__SteamAPI_ISteamNetworkingUtils_SetConfigValue_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SetConfigValue\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SetConfigValueStruct_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SetConfigValueStruct");
    if (!__SteamAPI_ISteamNetworkingUtils_SetConfigValueStruct_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SetConfigValueStruct\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueFloat_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueFloat");
    if (!__SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueFloat_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueFloat\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueInt32_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueInt32");
    if (!__SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueInt32_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueInt32\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueString_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueString");
    if (!__SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueString\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SetDebugOutputFunction_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SetDebugOutputFunction");
    if (!__SteamAPI_ISteamNetworkingUtils_SetDebugOutputFunction_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SetDebugOutputFunction\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_FakeIPResult_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_FakeIPResult");
    if (!__SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_FakeIPResult_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_FakeIPResult\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionFailed_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionFailed");
    if (!__SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionFailed_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionFailed\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionRequest_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionRequest");
    if (!__SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionRequest_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionRequest\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetAuthenticationStatusChanged_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetAuthenticationStatusChanged");
    if (!__SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetAuthenticationStatusChanged_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetAuthenticationStatusChanged\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetConnectionStatusChanged_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetConnectionStatusChanged");
    if (!__SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetConnectionStatusChanged_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetConnectionStatusChanged\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamRelayNetworkStatusChanged_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamRelayNetworkStatusChanged");
    if (!__SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamRelayNetworkStatusChanged_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamRelayNetworkStatusChanged\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueFloat_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueFloat");
    if (!__SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueFloat_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueFloat\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32");
    if (!__SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValuePtr_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValuePtr");
    if (!__SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValuePtr_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValuePtr\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueString_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueString");
    if (!__SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueString\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_GetFakeIPType_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_GetFakeIPType");
    if (!__SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_GetFakeIPType_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_GetFakeIPType\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ParseString_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ParseString");
    if (!__SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ParseString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ParseString\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ToString_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ToString");
    if (!__SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ToString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ToString\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ParseString_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ParseString");
    if (!__SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ParseString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ParseString\n"); complete = false; }
    __SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ToString_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ToString");
    if (!__SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ToString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ToString\n"); complete = false; }
    __SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser");
    if (!__SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser\n"); complete = false; }
    __SteamAPI_ISteamNetworking_AllowP2PPacketRelay_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_AllowP2PPacketRelay");
    if (!__SteamAPI_ISteamNetworking_AllowP2PPacketRelay_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_AllowP2PPacketRelay\n"); complete = false; }
    __SteamAPI_ISteamNetworking_CloseP2PChannelWithUser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_CloseP2PChannelWithUser");
    if (!__SteamAPI_ISteamNetworking_CloseP2PChannelWithUser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_CloseP2PChannelWithUser\n"); complete = false; }
    __SteamAPI_ISteamNetworking_CloseP2PSessionWithUser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_CloseP2PSessionWithUser");
    if (!__SteamAPI_ISteamNetworking_CloseP2PSessionWithUser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_CloseP2PSessionWithUser\n"); complete = false; }
    __SteamAPI_ISteamNetworking_CreateConnectionSocket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_CreateConnectionSocket");
    if (!__SteamAPI_ISteamNetworking_CreateConnectionSocket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_CreateConnectionSocket\n"); complete = false; }
    __SteamAPI_ISteamNetworking_CreateListenSocket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_CreateListenSocket");
    if (!__SteamAPI_ISteamNetworking_CreateListenSocket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_CreateListenSocket\n"); complete = false; }
    __SteamAPI_ISteamNetworking_CreateP2PConnectionSocket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_CreateP2PConnectionSocket");
    if (!__SteamAPI_ISteamNetworking_CreateP2PConnectionSocket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_CreateP2PConnectionSocket\n"); complete = false; }
    __SteamAPI_ISteamNetworking_DestroyListenSocket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_DestroyListenSocket");
    if (!__SteamAPI_ISteamNetworking_DestroyListenSocket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_DestroyListenSocket\n"); complete = false; }
    __SteamAPI_ISteamNetworking_DestroySocket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_DestroySocket");
    if (!__SteamAPI_ISteamNetworking_DestroySocket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_DestroySocket\n"); complete = false; }
    __SteamAPI_ISteamNetworking_GetListenSocketInfo_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_GetListenSocketInfo");
    if (!__SteamAPI_ISteamNetworking_GetListenSocketInfo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_GetListenSocketInfo\n"); complete = false; }
    __SteamAPI_ISteamNetworking_GetMaxPacketSize_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_GetMaxPacketSize");
    if (!__SteamAPI_ISteamNetworking_GetMaxPacketSize_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_GetMaxPacketSize\n"); complete = false; }
    __SteamAPI_ISteamNetworking_GetP2PSessionState_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_GetP2PSessionState");
    if (!__SteamAPI_ISteamNetworking_GetP2PSessionState_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_GetP2PSessionState\n"); complete = false; }
    __SteamAPI_ISteamNetworking_GetSocketConnectionType_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_GetSocketConnectionType");
    if (!__SteamAPI_ISteamNetworking_GetSocketConnectionType_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_GetSocketConnectionType\n"); complete = false; }
    __SteamAPI_ISteamNetworking_GetSocketInfo_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_GetSocketInfo");
    if (!__SteamAPI_ISteamNetworking_GetSocketInfo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_GetSocketInfo\n"); complete = false; }
    __SteamAPI_ISteamNetworking_IsDataAvailable_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_IsDataAvailable");
    if (!__SteamAPI_ISteamNetworking_IsDataAvailable_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_IsDataAvailable\n"); complete = false; }
    __SteamAPI_ISteamNetworking_IsDataAvailableOnSocket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_IsDataAvailableOnSocket");
    if (!__SteamAPI_ISteamNetworking_IsDataAvailableOnSocket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_IsDataAvailableOnSocket\n"); complete = false; }
    __SteamAPI_ISteamNetworking_IsP2PPacketAvailable_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_IsP2PPacketAvailable");
    if (!__SteamAPI_ISteamNetworking_IsP2PPacketAvailable_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_IsP2PPacketAvailable\n"); complete = false; }
    __SteamAPI_ISteamNetworking_ReadP2PPacket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_ReadP2PPacket");
    if (!__SteamAPI_ISteamNetworking_ReadP2PPacket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_ReadP2PPacket\n"); complete = false; }
    __SteamAPI_ISteamNetworking_RetrieveData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_RetrieveData");
    if (!__SteamAPI_ISteamNetworking_RetrieveData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_RetrieveData\n"); complete = false; }
    __SteamAPI_ISteamNetworking_RetrieveDataFromSocket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_RetrieveDataFromSocket");
    if (!__SteamAPI_ISteamNetworking_RetrieveDataFromSocket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_RetrieveDataFromSocket\n"); complete = false; }
    __SteamAPI_ISteamNetworking_SendDataOnSocket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_SendDataOnSocket");
    if (!__SteamAPI_ISteamNetworking_SendDataOnSocket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_SendDataOnSocket\n"); complete = false; }
    __SteamAPI_ISteamNetworking_SendP2PPacket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamNetworking_SendP2PPacket");
    if (!__SteamAPI_ISteamNetworking_SendP2PPacket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamNetworking_SendP2PPacket\n"); complete = false; }
    __SteamAPI_ISteamParentalSettings_BIsAppBlocked_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParentalSettings_BIsAppBlocked");
    if (!__SteamAPI_ISteamParentalSettings_BIsAppBlocked_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParentalSettings_BIsAppBlocked\n"); complete = false; }
    __SteamAPI_ISteamParentalSettings_BIsAppInBlockList_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParentalSettings_BIsAppInBlockList");
    if (!__SteamAPI_ISteamParentalSettings_BIsAppInBlockList_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParentalSettings_BIsAppInBlockList\n"); complete = false; }
    __SteamAPI_ISteamParentalSettings_BIsFeatureBlocked_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParentalSettings_BIsFeatureBlocked");
    if (!__SteamAPI_ISteamParentalSettings_BIsFeatureBlocked_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParentalSettings_BIsFeatureBlocked\n"); complete = false; }
    __SteamAPI_ISteamParentalSettings_BIsFeatureInBlockList_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParentalSettings_BIsFeatureInBlockList");
    if (!__SteamAPI_ISteamParentalSettings_BIsFeatureInBlockList_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParentalSettings_BIsFeatureInBlockList\n"); complete = false; }
    __SteamAPI_ISteamParentalSettings_BIsParentalLockEnabled_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParentalSettings_BIsParentalLockEnabled");
    if (!__SteamAPI_ISteamParentalSettings_BIsParentalLockEnabled_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParentalSettings_BIsParentalLockEnabled\n"); complete = false; }
    __SteamAPI_ISteamParentalSettings_BIsParentalLockLocked_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParentalSettings_BIsParentalLockLocked");
    if (!__SteamAPI_ISteamParentalSettings_BIsParentalLockLocked_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParentalSettings_BIsParentalLockLocked\n"); complete = false; }
    __SteamAPI_ISteamParties_CancelReservation_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParties_CancelReservation");
    if (!__SteamAPI_ISteamParties_CancelReservation_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParties_CancelReservation\n"); complete = false; }
    __SteamAPI_ISteamParties_ChangeNumOpenSlots_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParties_ChangeNumOpenSlots");
    if (!__SteamAPI_ISteamParties_ChangeNumOpenSlots_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParties_ChangeNumOpenSlots\n"); complete = false; }
    __SteamAPI_ISteamParties_CreateBeacon_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParties_CreateBeacon");
    if (!__SteamAPI_ISteamParties_CreateBeacon_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParties_CreateBeacon\n"); complete = false; }
    __SteamAPI_ISteamParties_DestroyBeacon_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParties_DestroyBeacon");
    if (!__SteamAPI_ISteamParties_DestroyBeacon_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParties_DestroyBeacon\n"); complete = false; }
    __SteamAPI_ISteamParties_GetAvailableBeaconLocations_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParties_GetAvailableBeaconLocations");
    if (!__SteamAPI_ISteamParties_GetAvailableBeaconLocations_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParties_GetAvailableBeaconLocations\n"); complete = false; }
    __SteamAPI_ISteamParties_GetBeaconByIndex_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParties_GetBeaconByIndex");
    if (!__SteamAPI_ISteamParties_GetBeaconByIndex_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParties_GetBeaconByIndex\n"); complete = false; }
    __SteamAPI_ISteamParties_GetBeaconDetails_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParties_GetBeaconDetails");
    if (!__SteamAPI_ISteamParties_GetBeaconDetails_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParties_GetBeaconDetails\n"); complete = false; }
    __SteamAPI_ISteamParties_GetBeaconLocationData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParties_GetBeaconLocationData");
    if (!__SteamAPI_ISteamParties_GetBeaconLocationData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParties_GetBeaconLocationData\n"); complete = false; }
    __SteamAPI_ISteamParties_GetNumActiveBeacons_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParties_GetNumActiveBeacons");
    if (!__SteamAPI_ISteamParties_GetNumActiveBeacons_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParties_GetNumActiveBeacons\n"); complete = false; }
    __SteamAPI_ISteamParties_GetNumAvailableBeaconLocations_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParties_GetNumAvailableBeaconLocations");
    if (!__SteamAPI_ISteamParties_GetNumAvailableBeaconLocations_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParties_GetNumAvailableBeaconLocations\n"); complete = false; }
    __SteamAPI_ISteamParties_JoinParty_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParties_JoinParty");
    if (!__SteamAPI_ISteamParties_JoinParty_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParties_JoinParty\n"); complete = false; }
    __SteamAPI_ISteamParties_OnReservationCompleted_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamParties_OnReservationCompleted");
    if (!__SteamAPI_ISteamParties_OnReservationCompleted_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamParties_OnReservationCompleted\n"); complete = false; }
    __SteamAPI_ISteamRemotePlay_BEnableRemotePlayTogetherDirectInput_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemotePlay_BEnableRemotePlayTogetherDirectInput");
    if (!__SteamAPI_ISteamRemotePlay_BEnableRemotePlayTogetherDirectInput_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemotePlay_BEnableRemotePlayTogetherDirectInput\n"); complete = false; }
    __SteamAPI_ISteamRemotePlay_BGetSessionClientResolution_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemotePlay_BGetSessionClientResolution");
    if (!__SteamAPI_ISteamRemotePlay_BGetSessionClientResolution_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemotePlay_BGetSessionClientResolution\n"); complete = false; }
    __SteamAPI_ISteamRemotePlay_BSendRemotePlayTogetherInvite_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemotePlay_BSendRemotePlayTogetherInvite");
    if (!__SteamAPI_ISteamRemotePlay_BSendRemotePlayTogetherInvite_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemotePlay_BSendRemotePlayTogetherInvite\n"); complete = false; }
    __SteamAPI_ISteamRemotePlay_CreateMouseCursor_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemotePlay_CreateMouseCursor");
    if (!__SteamAPI_ISteamRemotePlay_CreateMouseCursor_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemotePlay_CreateMouseCursor\n"); complete = false; }
    __SteamAPI_ISteamRemotePlay_DisableRemotePlayTogetherDirectInput_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemotePlay_DisableRemotePlayTogetherDirectInput");
    if (!__SteamAPI_ISteamRemotePlay_DisableRemotePlayTogetherDirectInput_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemotePlay_DisableRemotePlayTogetherDirectInput\n"); complete = false; }
    __SteamAPI_ISteamRemotePlay_GetInput_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemotePlay_GetInput");
    if (!__SteamAPI_ISteamRemotePlay_GetInput_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemotePlay_GetInput\n"); complete = false; }
    __SteamAPI_ISteamRemotePlay_GetSessionClientFormFactor_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemotePlay_GetSessionClientFormFactor");
    if (!__SteamAPI_ISteamRemotePlay_GetSessionClientFormFactor_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemotePlay_GetSessionClientFormFactor\n"); complete = false; }
    __SteamAPI_ISteamRemotePlay_GetSessionClientName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemotePlay_GetSessionClientName");
    if (!__SteamAPI_ISteamRemotePlay_GetSessionClientName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemotePlay_GetSessionClientName\n"); complete = false; }
    __SteamAPI_ISteamRemotePlay_GetSessionCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemotePlay_GetSessionCount");
    if (!__SteamAPI_ISteamRemotePlay_GetSessionCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemotePlay_GetSessionCount\n"); complete = false; }
    __SteamAPI_ISteamRemotePlay_GetSessionID_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemotePlay_GetSessionID");
    if (!__SteamAPI_ISteamRemotePlay_GetSessionID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemotePlay_GetSessionID\n"); complete = false; }
    __SteamAPI_ISteamRemotePlay_GetSessionSteamID_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemotePlay_GetSessionSteamID");
    if (!__SteamAPI_ISteamRemotePlay_GetSessionSteamID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemotePlay_GetSessionSteamID\n"); complete = false; }
    __SteamAPI_ISteamRemotePlay_SetMouseCursor_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemotePlay_SetMouseCursor");
    if (!__SteamAPI_ISteamRemotePlay_SetMouseCursor_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemotePlay_SetMouseCursor\n"); complete = false; }
    __SteamAPI_ISteamRemotePlay_SetMousePosition_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemotePlay_SetMousePosition");
    if (!__SteamAPI_ISteamRemotePlay_SetMousePosition_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemotePlay_SetMousePosition\n"); complete = false; }
    __SteamAPI_ISteamRemotePlay_SetMouseVisibility_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemotePlay_SetMouseVisibility");
    if (!__SteamAPI_ISteamRemotePlay_SetMouseVisibility_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemotePlay_SetMouseVisibility\n"); complete = false; }
    __SteamAPI_ISteamRemotePlay_ShowRemotePlayTogetherUI_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemotePlay_ShowRemotePlayTogetherUI");
    if (!__SteamAPI_ISteamRemotePlay_ShowRemotePlayTogetherUI_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemotePlay_ShowRemotePlayTogetherUI\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_BeginFileWriteBatch_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_BeginFileWriteBatch");
    if (!__SteamAPI_ISteamRemoteStorage_BeginFileWriteBatch_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_BeginFileWriteBatch\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_CommitPublishedFileUpdate_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_CommitPublishedFileUpdate");
    if (!__SteamAPI_ISteamRemoteStorage_CommitPublishedFileUpdate_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_CommitPublishedFileUpdate\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_CreatePublishedFileUpdateRequest_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_CreatePublishedFileUpdateRequest");
    if (!__SteamAPI_ISteamRemoteStorage_CreatePublishedFileUpdateRequest_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_CreatePublishedFileUpdateRequest\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_DeletePublishedFile_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_DeletePublishedFile");
    if (!__SteamAPI_ISteamRemoteStorage_DeletePublishedFile_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_DeletePublishedFile\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_EndFileWriteBatch_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_EndFileWriteBatch");
    if (!__SteamAPI_ISteamRemoteStorage_EndFileWriteBatch_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_EndFileWriteBatch\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_EnumeratePublishedFilesByUserAction_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_EnumeratePublishedFilesByUserAction");
    if (!__SteamAPI_ISteamRemoteStorage_EnumeratePublishedFilesByUserAction_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_EnumeratePublishedFilesByUserAction\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_EnumeratePublishedWorkshopFiles_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_EnumeratePublishedWorkshopFiles");
    if (!__SteamAPI_ISteamRemoteStorage_EnumeratePublishedWorkshopFiles_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_EnumeratePublishedWorkshopFiles\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_EnumerateUserPublishedFiles_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_EnumerateUserPublishedFiles");
    if (!__SteamAPI_ISteamRemoteStorage_EnumerateUserPublishedFiles_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_EnumerateUserPublishedFiles\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_EnumerateUserSharedWorkshopFiles_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_EnumerateUserSharedWorkshopFiles");
    if (!__SteamAPI_ISteamRemoteStorage_EnumerateUserSharedWorkshopFiles_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_EnumerateUserSharedWorkshopFiles\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_EnumerateUserSubscribedFiles_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_EnumerateUserSubscribedFiles");
    if (!__SteamAPI_ISteamRemoteStorage_EnumerateUserSubscribedFiles_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_EnumerateUserSubscribedFiles\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_FileDelete_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_FileDelete");
    if (!__SteamAPI_ISteamRemoteStorage_FileDelete_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_FileDelete\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_FileExists_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_FileExists");
    if (!__SteamAPI_ISteamRemoteStorage_FileExists_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_FileExists\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_FileForget_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_FileForget");
    if (!__SteamAPI_ISteamRemoteStorage_FileForget_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_FileForget\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_FilePersisted_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_FilePersisted");
    if (!__SteamAPI_ISteamRemoteStorage_FilePersisted_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_FilePersisted\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_FileRead_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_FileRead");
    if (!__SteamAPI_ISteamRemoteStorage_FileRead_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_FileRead\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_FileReadAsync_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_FileReadAsync");
    if (!__SteamAPI_ISteamRemoteStorage_FileReadAsync_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_FileReadAsync\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_FileReadAsyncComplete_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_FileReadAsyncComplete");
    if (!__SteamAPI_ISteamRemoteStorage_FileReadAsyncComplete_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_FileReadAsyncComplete\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_FileShare_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_FileShare");
    if (!__SteamAPI_ISteamRemoteStorage_FileShare_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_FileShare\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_FileWrite_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_FileWrite");
    if (!__SteamAPI_ISteamRemoteStorage_FileWrite_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_FileWrite\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_FileWriteAsync_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_FileWriteAsync");
    if (!__SteamAPI_ISteamRemoteStorage_FileWriteAsync_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_FileWriteAsync\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_FileWriteStreamCancel_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_FileWriteStreamCancel");
    if (!__SteamAPI_ISteamRemoteStorage_FileWriteStreamCancel_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_FileWriteStreamCancel\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_FileWriteStreamClose_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_FileWriteStreamClose");
    if (!__SteamAPI_ISteamRemoteStorage_FileWriteStreamClose_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_FileWriteStreamClose\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_FileWriteStreamOpen_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_FileWriteStreamOpen");
    if (!__SteamAPI_ISteamRemoteStorage_FileWriteStreamOpen_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_FileWriteStreamOpen\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_FileWriteStreamWriteChunk_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_FileWriteStreamWriteChunk");
    if (!__SteamAPI_ISteamRemoteStorage_FileWriteStreamWriteChunk_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_FileWriteStreamWriteChunk\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_GetCachedUGCCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_GetCachedUGCCount");
    if (!__SteamAPI_ISteamRemoteStorage_GetCachedUGCCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_GetCachedUGCCount\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_GetCachedUGCHandle_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_GetCachedUGCHandle");
    if (!__SteamAPI_ISteamRemoteStorage_GetCachedUGCHandle_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_GetCachedUGCHandle\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_GetFileCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_GetFileCount");
    if (!__SteamAPI_ISteamRemoteStorage_GetFileCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_GetFileCount\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_GetFileNameAndSize_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_GetFileNameAndSize");
    if (!__SteamAPI_ISteamRemoteStorage_GetFileNameAndSize_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_GetFileNameAndSize\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_GetFileSize_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_GetFileSize");
    if (!__SteamAPI_ISteamRemoteStorage_GetFileSize_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_GetFileSize\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_GetFileTimestamp_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_GetFileTimestamp");
    if (!__SteamAPI_ISteamRemoteStorage_GetFileTimestamp_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_GetFileTimestamp\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_GetLocalFileChange_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_GetLocalFileChange");
    if (!__SteamAPI_ISteamRemoteStorage_GetLocalFileChange_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_GetLocalFileChange\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_GetLocalFileChangeCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_GetLocalFileChangeCount");
    if (!__SteamAPI_ISteamRemoteStorage_GetLocalFileChangeCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_GetLocalFileChangeCount\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_GetPublishedFileDetails_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_GetPublishedFileDetails");
    if (!__SteamAPI_ISteamRemoteStorage_GetPublishedFileDetails_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_GetPublishedFileDetails\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_GetPublishedItemVoteDetails_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_GetPublishedItemVoteDetails");
    if (!__SteamAPI_ISteamRemoteStorage_GetPublishedItemVoteDetails_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_GetPublishedItemVoteDetails\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_GetQuota_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_GetQuota");
    if (!__SteamAPI_ISteamRemoteStorage_GetQuota_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_GetQuota\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_GetSyncPlatforms_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_GetSyncPlatforms");
    if (!__SteamAPI_ISteamRemoteStorage_GetSyncPlatforms_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_GetSyncPlatforms\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_GetUGCDetails_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_GetUGCDetails");
    if (!__SteamAPI_ISteamRemoteStorage_GetUGCDetails_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_GetUGCDetails\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_GetUGCDownloadProgress_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_GetUGCDownloadProgress");
    if (!__SteamAPI_ISteamRemoteStorage_GetUGCDownloadProgress_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_GetUGCDownloadProgress\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_GetUserPublishedItemVoteDetails_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_GetUserPublishedItemVoteDetails");
    if (!__SteamAPI_ISteamRemoteStorage_GetUserPublishedItemVoteDetails_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_GetUserPublishedItemVoteDetails\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_IsCloudEnabledForAccount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_IsCloudEnabledForAccount");
    if (!__SteamAPI_ISteamRemoteStorage_IsCloudEnabledForAccount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_IsCloudEnabledForAccount\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_IsCloudEnabledForApp_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_IsCloudEnabledForApp");
    if (!__SteamAPI_ISteamRemoteStorage_IsCloudEnabledForApp_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_IsCloudEnabledForApp\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_PublishVideo_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_PublishVideo");
    if (!__SteamAPI_ISteamRemoteStorage_PublishVideo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_PublishVideo\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_PublishWorkshopFile_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_PublishWorkshopFile");
    if (!__SteamAPI_ISteamRemoteStorage_PublishWorkshopFile_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_PublishWorkshopFile\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_SetCloudEnabledForApp_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_SetCloudEnabledForApp");
    if (!__SteamAPI_ISteamRemoteStorage_SetCloudEnabledForApp_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_SetCloudEnabledForApp\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_SetSyncPlatforms_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_SetSyncPlatforms");
    if (!__SteamAPI_ISteamRemoteStorage_SetSyncPlatforms_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_SetSyncPlatforms\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_SetUserPublishedFileAction_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_SetUserPublishedFileAction");
    if (!__SteamAPI_ISteamRemoteStorage_SetUserPublishedFileAction_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_SetUserPublishedFileAction\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_SubscribePublishedFile_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_SubscribePublishedFile");
    if (!__SteamAPI_ISteamRemoteStorage_SubscribePublishedFile_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_SubscribePublishedFile\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_UGCDownload_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_UGCDownload");
    if (!__SteamAPI_ISteamRemoteStorage_UGCDownload_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_UGCDownload\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_UGCDownloadToLocation_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_UGCDownloadToLocation");
    if (!__SteamAPI_ISteamRemoteStorage_UGCDownloadToLocation_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_UGCDownloadToLocation\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_UGCRead_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_UGCRead");
    if (!__SteamAPI_ISteamRemoteStorage_UGCRead_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_UGCRead\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_UnsubscribePublishedFile_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_UnsubscribePublishedFile");
    if (!__SteamAPI_ISteamRemoteStorage_UnsubscribePublishedFile_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_UnsubscribePublishedFile\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_UpdatePublishedFileDescription_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_UpdatePublishedFileDescription");
    if (!__SteamAPI_ISteamRemoteStorage_UpdatePublishedFileDescription_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_UpdatePublishedFileDescription\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_UpdatePublishedFileFile_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_UpdatePublishedFileFile");
    if (!__SteamAPI_ISteamRemoteStorage_UpdatePublishedFileFile_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_UpdatePublishedFileFile\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_UpdatePublishedFilePreviewFile_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_UpdatePublishedFilePreviewFile");
    if (!__SteamAPI_ISteamRemoteStorage_UpdatePublishedFilePreviewFile_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_UpdatePublishedFilePreviewFile\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_UpdatePublishedFileSetChangeDescription_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_UpdatePublishedFileSetChangeDescription");
    if (!__SteamAPI_ISteamRemoteStorage_UpdatePublishedFileSetChangeDescription_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_UpdatePublishedFileSetChangeDescription\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTags_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTags");
    if (!__SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTags_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTags\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTitle_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTitle");
    if (!__SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTitle_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTitle\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_UpdatePublishedFileVisibility_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_UpdatePublishedFileVisibility");
    if (!__SteamAPI_ISteamRemoteStorage_UpdatePublishedFileVisibility_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_UpdatePublishedFileVisibility\n"); complete = false; }
    __SteamAPI_ISteamRemoteStorage_UpdateUserPublishedItemVote_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamRemoteStorage_UpdateUserPublishedItemVote");
    if (!__SteamAPI_ISteamRemoteStorage_UpdateUserPublishedItemVote_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamRemoteStorage_UpdateUserPublishedItemVote\n"); complete = false; }
    __SteamAPI_ISteamScreenshots_AddScreenshotToLibrary_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamScreenshots_AddScreenshotToLibrary");
    if (!__SteamAPI_ISteamScreenshots_AddScreenshotToLibrary_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamScreenshots_AddScreenshotToLibrary\n"); complete = false; }
    __SteamAPI_ISteamScreenshots_AddVRScreenshotToLibrary_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamScreenshots_AddVRScreenshotToLibrary");
    if (!__SteamAPI_ISteamScreenshots_AddVRScreenshotToLibrary_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamScreenshots_AddVRScreenshotToLibrary\n"); complete = false; }
    __SteamAPI_ISteamScreenshots_HookScreenshots_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamScreenshots_HookScreenshots");
    if (!__SteamAPI_ISteamScreenshots_HookScreenshots_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamScreenshots_HookScreenshots\n"); complete = false; }
    __SteamAPI_ISteamScreenshots_IsScreenshotsHooked_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamScreenshots_IsScreenshotsHooked");
    if (!__SteamAPI_ISteamScreenshots_IsScreenshotsHooked_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamScreenshots_IsScreenshotsHooked\n"); complete = false; }
    __SteamAPI_ISteamScreenshots_SetLocation_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamScreenshots_SetLocation");
    if (!__SteamAPI_ISteamScreenshots_SetLocation_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamScreenshots_SetLocation\n"); complete = false; }
    __SteamAPI_ISteamScreenshots_TagPublishedFile_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamScreenshots_TagPublishedFile");
    if (!__SteamAPI_ISteamScreenshots_TagPublishedFile_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamScreenshots_TagPublishedFile\n"); complete = false; }
    __SteamAPI_ISteamScreenshots_TagUser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamScreenshots_TagUser");
    if (!__SteamAPI_ISteamScreenshots_TagUser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamScreenshots_TagUser\n"); complete = false; }
    __SteamAPI_ISteamScreenshots_TriggerScreenshot_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamScreenshots_TriggerScreenshot");
    if (!__SteamAPI_ISteamScreenshots_TriggerScreenshot_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamScreenshots_TriggerScreenshot\n"); complete = false; }
    __SteamAPI_ISteamScreenshots_WriteScreenshot_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamScreenshots_WriteScreenshot");
    if (!__SteamAPI_ISteamScreenshots_WriteScreenshot_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamScreenshots_WriteScreenshot\n"); complete = false; }
    __SteamAPI_ISteamTimeline_AddGamePhaseTag_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_AddGamePhaseTag");
    if (!__SteamAPI_ISteamTimeline_AddGamePhaseTag_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_AddGamePhaseTag\n"); complete = false; }
    __SteamAPI_ISteamTimeline_AddInstantaneousTimelineEvent_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_AddInstantaneousTimelineEvent");
    if (!__SteamAPI_ISteamTimeline_AddInstantaneousTimelineEvent_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_AddInstantaneousTimelineEvent\n"); complete = false; }
    __SteamAPI_ISteamTimeline_AddRangeTimelineEvent_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_AddRangeTimelineEvent");
    if (!__SteamAPI_ISteamTimeline_AddRangeTimelineEvent_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_AddRangeTimelineEvent\n"); complete = false; }
    __SteamAPI_ISteamTimeline_ClearTimelineTooltip_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_ClearTimelineTooltip");
    if (!__SteamAPI_ISteamTimeline_ClearTimelineTooltip_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_ClearTimelineTooltip\n"); complete = false; }
    __SteamAPI_ISteamTimeline_DoesEventRecordingExist_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_DoesEventRecordingExist");
    if (!__SteamAPI_ISteamTimeline_DoesEventRecordingExist_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_DoesEventRecordingExist\n"); complete = false; }
    __SteamAPI_ISteamTimeline_DoesGamePhaseRecordingExist_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_DoesGamePhaseRecordingExist");
    if (!__SteamAPI_ISteamTimeline_DoesGamePhaseRecordingExist_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_DoesGamePhaseRecordingExist\n"); complete = false; }
    __SteamAPI_ISteamTimeline_EndGamePhase_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_EndGamePhase");
    if (!__SteamAPI_ISteamTimeline_EndGamePhase_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_EndGamePhase\n"); complete = false; }
    __SteamAPI_ISteamTimeline_EndRangeTimelineEvent_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_EndRangeTimelineEvent");
    if (!__SteamAPI_ISteamTimeline_EndRangeTimelineEvent_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_EndRangeTimelineEvent\n"); complete = false; }
    __SteamAPI_ISteamTimeline_OpenOverlayToGamePhase_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_OpenOverlayToGamePhase");
    if (!__SteamAPI_ISteamTimeline_OpenOverlayToGamePhase_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_OpenOverlayToGamePhase\n"); complete = false; }
    __SteamAPI_ISteamTimeline_OpenOverlayToTimelineEvent_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_OpenOverlayToTimelineEvent");
    if (!__SteamAPI_ISteamTimeline_OpenOverlayToTimelineEvent_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_OpenOverlayToTimelineEvent\n"); complete = false; }
    __SteamAPI_ISteamTimeline_RemoveTimelineEvent_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_RemoveTimelineEvent");
    if (!__SteamAPI_ISteamTimeline_RemoveTimelineEvent_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_RemoveTimelineEvent\n"); complete = false; }
    __SteamAPI_ISteamTimeline_SetGamePhaseAttribute_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_SetGamePhaseAttribute");
    if (!__SteamAPI_ISteamTimeline_SetGamePhaseAttribute_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_SetGamePhaseAttribute\n"); complete = false; }
    __SteamAPI_ISteamTimeline_SetGamePhaseID_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_SetGamePhaseID");
    if (!__SteamAPI_ISteamTimeline_SetGamePhaseID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_SetGamePhaseID\n"); complete = false; }
    __SteamAPI_ISteamTimeline_SetTimelineGameMode_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_SetTimelineGameMode");
    if (!__SteamAPI_ISteamTimeline_SetTimelineGameMode_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_SetTimelineGameMode\n"); complete = false; }
    __SteamAPI_ISteamTimeline_SetTimelineTooltip_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_SetTimelineTooltip");
    if (!__SteamAPI_ISteamTimeline_SetTimelineTooltip_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_SetTimelineTooltip\n"); complete = false; }
    __SteamAPI_ISteamTimeline_StartGamePhase_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_StartGamePhase");
    if (!__SteamAPI_ISteamTimeline_StartGamePhase_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_StartGamePhase\n"); complete = false; }
    __SteamAPI_ISteamTimeline_StartRangeTimelineEvent_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_StartRangeTimelineEvent");
    if (!__SteamAPI_ISteamTimeline_StartRangeTimelineEvent_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_StartRangeTimelineEvent\n"); complete = false; }
    __SteamAPI_ISteamTimeline_UpdateRangeTimelineEvent_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamTimeline_UpdateRangeTimelineEvent");
    if (!__SteamAPI_ISteamTimeline_UpdateRangeTimelineEvent_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamTimeline_UpdateRangeTimelineEvent\n"); complete = false; }
    __SteamAPI_ISteamUGC_AddAppDependency_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_AddAppDependency");
    if (!__SteamAPI_ISteamUGC_AddAppDependency_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_AddAppDependency\n"); complete = false; }
    __SteamAPI_ISteamUGC_AddContentDescriptor_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_AddContentDescriptor");
    if (!__SteamAPI_ISteamUGC_AddContentDescriptor_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_AddContentDescriptor\n"); complete = false; }
    __SteamAPI_ISteamUGC_AddDependency_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_AddDependency");
    if (!__SteamAPI_ISteamUGC_AddDependency_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_AddDependency\n"); complete = false; }
    __SteamAPI_ISteamUGC_AddExcludedTag_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_AddExcludedTag");
    if (!__SteamAPI_ISteamUGC_AddExcludedTag_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_AddExcludedTag\n"); complete = false; }
    __SteamAPI_ISteamUGC_AddItemKeyValueTag_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_AddItemKeyValueTag");
    if (!__SteamAPI_ISteamUGC_AddItemKeyValueTag_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_AddItemKeyValueTag\n"); complete = false; }
    __SteamAPI_ISteamUGC_AddItemPreviewFile_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_AddItemPreviewFile");
    if (!__SteamAPI_ISteamUGC_AddItemPreviewFile_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_AddItemPreviewFile\n"); complete = false; }
    __SteamAPI_ISteamUGC_AddItemPreviewVideo_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_AddItemPreviewVideo");
    if (!__SteamAPI_ISteamUGC_AddItemPreviewVideo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_AddItemPreviewVideo\n"); complete = false; }
    __SteamAPI_ISteamUGC_AddItemToFavorites_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_AddItemToFavorites");
    if (!__SteamAPI_ISteamUGC_AddItemToFavorites_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_AddItemToFavorites\n"); complete = false; }
    __SteamAPI_ISteamUGC_AddRequiredKeyValueTag_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_AddRequiredKeyValueTag");
    if (!__SteamAPI_ISteamUGC_AddRequiredKeyValueTag_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_AddRequiredKeyValueTag\n"); complete = false; }
    __SteamAPI_ISteamUGC_AddRequiredTag_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_AddRequiredTag");
    if (!__SteamAPI_ISteamUGC_AddRequiredTag_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_AddRequiredTag\n"); complete = false; }
    __SteamAPI_ISteamUGC_AddRequiredTagGroup_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_AddRequiredTagGroup");
    if (!__SteamAPI_ISteamUGC_AddRequiredTagGroup_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_AddRequiredTagGroup\n"); complete = false; }
    __SteamAPI_ISteamUGC_BInitWorkshopForGameServer_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_BInitWorkshopForGameServer");
    if (!__SteamAPI_ISteamUGC_BInitWorkshopForGameServer_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_BInitWorkshopForGameServer\n"); complete = false; }
    __SteamAPI_ISteamUGC_CreateItem_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_CreateItem");
    if (!__SteamAPI_ISteamUGC_CreateItem_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_CreateItem\n"); complete = false; }
    __SteamAPI_ISteamUGC_CreateQueryAllUGCRequestCursor_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_CreateQueryAllUGCRequestCursor");
    if (!__SteamAPI_ISteamUGC_CreateQueryAllUGCRequestCursor_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_CreateQueryAllUGCRequestCursor\n"); complete = false; }
    __SteamAPI_ISteamUGC_CreateQueryAllUGCRequestPage_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_CreateQueryAllUGCRequestPage");
    if (!__SteamAPI_ISteamUGC_CreateQueryAllUGCRequestPage_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_CreateQueryAllUGCRequestPage\n"); complete = false; }
    __SteamAPI_ISteamUGC_CreateQueryUGCDetailsRequest_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_CreateQueryUGCDetailsRequest");
    if (!__SteamAPI_ISteamUGC_CreateQueryUGCDetailsRequest_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_CreateQueryUGCDetailsRequest\n"); complete = false; }
    __SteamAPI_ISteamUGC_CreateQueryUserUGCRequest_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_CreateQueryUserUGCRequest");
    if (!__SteamAPI_ISteamUGC_CreateQueryUserUGCRequest_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_CreateQueryUserUGCRequest\n"); complete = false; }
    __SteamAPI_ISteamUGC_DeleteItem_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_DeleteItem");
    if (!__SteamAPI_ISteamUGC_DeleteItem_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_DeleteItem\n"); complete = false; }
    __SteamAPI_ISteamUGC_DownloadItem_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_DownloadItem");
    if (!__SteamAPI_ISteamUGC_DownloadItem_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_DownloadItem\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetAppDependencies_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetAppDependencies");
    if (!__SteamAPI_ISteamUGC_GetAppDependencies_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetAppDependencies\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetItemDownloadInfo_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetItemDownloadInfo");
    if (!__SteamAPI_ISteamUGC_GetItemDownloadInfo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetItemDownloadInfo\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetItemInstallInfo_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetItemInstallInfo");
    if (!__SteamAPI_ISteamUGC_GetItemInstallInfo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetItemInstallInfo\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetItemState_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetItemState");
    if (!__SteamAPI_ISteamUGC_GetItemState_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetItemState\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetItemUpdateProgress_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetItemUpdateProgress");
    if (!__SteamAPI_ISteamUGC_GetItemUpdateProgress_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetItemUpdateProgress\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetNumSubscribedItems_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetNumSubscribedItems");
    if (!__SteamAPI_ISteamUGC_GetNumSubscribedItems_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetNumSubscribedItems\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetNumSupportedGameVersions_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetNumSupportedGameVersions");
    if (!__SteamAPI_ISteamUGC_GetNumSupportedGameVersions_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetNumSupportedGameVersions\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetQueryFirstUGCKeyValueTag_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetQueryFirstUGCKeyValueTag");
    if (!__SteamAPI_ISteamUGC_GetQueryFirstUGCKeyValueTag_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetQueryFirstUGCKeyValueTag\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetQueryUGCAdditionalPreview_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetQueryUGCAdditionalPreview");
    if (!__SteamAPI_ISteamUGC_GetQueryUGCAdditionalPreview_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetQueryUGCAdditionalPreview\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetQueryUGCChildren_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetQueryUGCChildren");
    if (!__SteamAPI_ISteamUGC_GetQueryUGCChildren_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetQueryUGCChildren\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetQueryUGCContentDescriptors_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetQueryUGCContentDescriptors");
    if (!__SteamAPI_ISteamUGC_GetQueryUGCContentDescriptors_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetQueryUGCContentDescriptors\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetQueryUGCKeyValueTag_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetQueryUGCKeyValueTag");
    if (!__SteamAPI_ISteamUGC_GetQueryUGCKeyValueTag_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetQueryUGCKeyValueTag\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetQueryUGCMetadata_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetQueryUGCMetadata");
    if (!__SteamAPI_ISteamUGC_GetQueryUGCMetadata_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetQueryUGCMetadata\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetQueryUGCNumAdditionalPreviews_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetQueryUGCNumAdditionalPreviews");
    if (!__SteamAPI_ISteamUGC_GetQueryUGCNumAdditionalPreviews_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetQueryUGCNumAdditionalPreviews\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetQueryUGCNumKeyValueTags_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetQueryUGCNumKeyValueTags");
    if (!__SteamAPI_ISteamUGC_GetQueryUGCNumKeyValueTags_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetQueryUGCNumKeyValueTags\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetQueryUGCNumTags_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetQueryUGCNumTags");
    if (!__SteamAPI_ISteamUGC_GetQueryUGCNumTags_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetQueryUGCNumTags\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetQueryUGCPreviewURL_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetQueryUGCPreviewURL");
    if (!__SteamAPI_ISteamUGC_GetQueryUGCPreviewURL_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetQueryUGCPreviewURL\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetQueryUGCResult_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetQueryUGCResult");
    if (!__SteamAPI_ISteamUGC_GetQueryUGCResult_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetQueryUGCResult\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetQueryUGCStatistic_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetQueryUGCStatistic");
    if (!__SteamAPI_ISteamUGC_GetQueryUGCStatistic_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetQueryUGCStatistic\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetQueryUGCTag_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetQueryUGCTag");
    if (!__SteamAPI_ISteamUGC_GetQueryUGCTag_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetQueryUGCTag\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetQueryUGCTagDisplayName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetQueryUGCTagDisplayName");
    if (!__SteamAPI_ISteamUGC_GetQueryUGCTagDisplayName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetQueryUGCTagDisplayName\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetSubscribedItems_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetSubscribedItems");
    if (!__SteamAPI_ISteamUGC_GetSubscribedItems_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetSubscribedItems\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetSupportedGameVersionData_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetSupportedGameVersionData");
    if (!__SteamAPI_ISteamUGC_GetSupportedGameVersionData_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetSupportedGameVersionData\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetUserContentDescriptorPreferences_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetUserContentDescriptorPreferences");
    if (!__SteamAPI_ISteamUGC_GetUserContentDescriptorPreferences_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetUserContentDescriptorPreferences\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetUserItemVote_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetUserItemVote");
    if (!__SteamAPI_ISteamUGC_GetUserItemVote_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetUserItemVote\n"); complete = false; }
    __SteamAPI_ISteamUGC_GetWorkshopEULAStatus_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_GetWorkshopEULAStatus");
    if (!__SteamAPI_ISteamUGC_GetWorkshopEULAStatus_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_GetWorkshopEULAStatus\n"); complete = false; }
    __SteamAPI_ISteamUGC_ReleaseQueryUGCRequest_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_ReleaseQueryUGCRequest");
    if (!__SteamAPI_ISteamUGC_ReleaseQueryUGCRequest_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_ReleaseQueryUGCRequest\n"); complete = false; }
    __SteamAPI_ISteamUGC_RemoveAllItemKeyValueTags_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_RemoveAllItemKeyValueTags");
    if (!__SteamAPI_ISteamUGC_RemoveAllItemKeyValueTags_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_RemoveAllItemKeyValueTags\n"); complete = false; }
    __SteamAPI_ISteamUGC_RemoveAppDependency_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_RemoveAppDependency");
    if (!__SteamAPI_ISteamUGC_RemoveAppDependency_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_RemoveAppDependency\n"); complete = false; }
    __SteamAPI_ISteamUGC_RemoveContentDescriptor_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_RemoveContentDescriptor");
    if (!__SteamAPI_ISteamUGC_RemoveContentDescriptor_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_RemoveContentDescriptor\n"); complete = false; }
    __SteamAPI_ISteamUGC_RemoveDependency_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_RemoveDependency");
    if (!__SteamAPI_ISteamUGC_RemoveDependency_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_RemoveDependency\n"); complete = false; }
    __SteamAPI_ISteamUGC_RemoveItemFromFavorites_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_RemoveItemFromFavorites");
    if (!__SteamAPI_ISteamUGC_RemoveItemFromFavorites_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_RemoveItemFromFavorites\n"); complete = false; }
    __SteamAPI_ISteamUGC_RemoveItemKeyValueTags_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_RemoveItemKeyValueTags");
    if (!__SteamAPI_ISteamUGC_RemoveItemKeyValueTags_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_RemoveItemKeyValueTags\n"); complete = false; }
    __SteamAPI_ISteamUGC_RemoveItemPreview_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_RemoveItemPreview");
    if (!__SteamAPI_ISteamUGC_RemoveItemPreview_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_RemoveItemPreview\n"); complete = false; }
    __SteamAPI_ISteamUGC_RequestUGCDetails_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_RequestUGCDetails");
    if (!__SteamAPI_ISteamUGC_RequestUGCDetails_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_RequestUGCDetails\n"); complete = false; }
    __SteamAPI_ISteamUGC_SendQueryUGCRequest_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SendQueryUGCRequest");
    if (!__SteamAPI_ISteamUGC_SendQueryUGCRequest_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SendQueryUGCRequest\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetAdminQuery_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetAdminQuery");
    if (!__SteamAPI_ISteamUGC_SetAdminQuery_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetAdminQuery\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetAllowCachedResponse_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetAllowCachedResponse");
    if (!__SteamAPI_ISteamUGC_SetAllowCachedResponse_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetAllowCachedResponse\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetAllowLegacyUpload_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetAllowLegacyUpload");
    if (!__SteamAPI_ISteamUGC_SetAllowLegacyUpload_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetAllowLegacyUpload\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetCloudFileNameFilter_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetCloudFileNameFilter");
    if (!__SteamAPI_ISteamUGC_SetCloudFileNameFilter_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetCloudFileNameFilter\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetItemContent_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetItemContent");
    if (!__SteamAPI_ISteamUGC_SetItemContent_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetItemContent\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetItemDescription_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetItemDescription");
    if (!__SteamAPI_ISteamUGC_SetItemDescription_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetItemDescription\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetItemMetadata_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetItemMetadata");
    if (!__SteamAPI_ISteamUGC_SetItemMetadata_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetItemMetadata\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetItemPreview_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetItemPreview");
    if (!__SteamAPI_ISteamUGC_SetItemPreview_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetItemPreview\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetItemTags_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetItemTags");
    if (!__SteamAPI_ISteamUGC_SetItemTags_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetItemTags\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetItemTitle_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetItemTitle");
    if (!__SteamAPI_ISteamUGC_SetItemTitle_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetItemTitle\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetItemUpdateLanguage_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetItemUpdateLanguage");
    if (!__SteamAPI_ISteamUGC_SetItemUpdateLanguage_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetItemUpdateLanguage\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetItemVisibility_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetItemVisibility");
    if (!__SteamAPI_ISteamUGC_SetItemVisibility_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetItemVisibility\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetItemsDisabledLocally_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetItemsDisabledLocally");
    if (!__SteamAPI_ISteamUGC_SetItemsDisabledLocally_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetItemsDisabledLocally\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetLanguage_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetLanguage");
    if (!__SteamAPI_ISteamUGC_SetLanguage_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetLanguage\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetMatchAnyTag_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetMatchAnyTag");
    if (!__SteamAPI_ISteamUGC_SetMatchAnyTag_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetMatchAnyTag\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetRankedByTrendDays_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetRankedByTrendDays");
    if (!__SteamAPI_ISteamUGC_SetRankedByTrendDays_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetRankedByTrendDays\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetRequiredGameVersions_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetRequiredGameVersions");
    if (!__SteamAPI_ISteamUGC_SetRequiredGameVersions_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetRequiredGameVersions\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetReturnAdditionalPreviews_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetReturnAdditionalPreviews");
    if (!__SteamAPI_ISteamUGC_SetReturnAdditionalPreviews_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetReturnAdditionalPreviews\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetReturnChildren_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetReturnChildren");
    if (!__SteamAPI_ISteamUGC_SetReturnChildren_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetReturnChildren\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetReturnKeyValueTags_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetReturnKeyValueTags");
    if (!__SteamAPI_ISteamUGC_SetReturnKeyValueTags_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetReturnKeyValueTags\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetReturnLongDescription_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetReturnLongDescription");
    if (!__SteamAPI_ISteamUGC_SetReturnLongDescription_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetReturnLongDescription\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetReturnMetadata_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetReturnMetadata");
    if (!__SteamAPI_ISteamUGC_SetReturnMetadata_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetReturnMetadata\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetReturnOnlyIDs_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetReturnOnlyIDs");
    if (!__SteamAPI_ISteamUGC_SetReturnOnlyIDs_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetReturnOnlyIDs\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetReturnPlaytimeStats_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetReturnPlaytimeStats");
    if (!__SteamAPI_ISteamUGC_SetReturnPlaytimeStats_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetReturnPlaytimeStats\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetReturnTotalOnly_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetReturnTotalOnly");
    if (!__SteamAPI_ISteamUGC_SetReturnTotalOnly_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetReturnTotalOnly\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetSearchText_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetSearchText");
    if (!__SteamAPI_ISteamUGC_SetSearchText_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetSearchText\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetSubscriptionsLoadOrder_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetSubscriptionsLoadOrder");
    if (!__SteamAPI_ISteamUGC_SetSubscriptionsLoadOrder_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetSubscriptionsLoadOrder\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetTimeCreatedDateRange_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetTimeCreatedDateRange");
    if (!__SteamAPI_ISteamUGC_SetTimeCreatedDateRange_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetTimeCreatedDateRange\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetTimeUpdatedDateRange_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetTimeUpdatedDateRange");
    if (!__SteamAPI_ISteamUGC_SetTimeUpdatedDateRange_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetTimeUpdatedDateRange\n"); complete = false; }
    __SteamAPI_ISteamUGC_SetUserItemVote_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SetUserItemVote");
    if (!__SteamAPI_ISteamUGC_SetUserItemVote_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SetUserItemVote\n"); complete = false; }
    __SteamAPI_ISteamUGC_ShowWorkshopEULA_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_ShowWorkshopEULA");
    if (!__SteamAPI_ISteamUGC_ShowWorkshopEULA_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_ShowWorkshopEULA\n"); complete = false; }
    __SteamAPI_ISteamUGC_StartItemUpdate_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_StartItemUpdate");
    if (!__SteamAPI_ISteamUGC_StartItemUpdate_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_StartItemUpdate\n"); complete = false; }
    __SteamAPI_ISteamUGC_StartPlaytimeTracking_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_StartPlaytimeTracking");
    if (!__SteamAPI_ISteamUGC_StartPlaytimeTracking_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_StartPlaytimeTracking\n"); complete = false; }
    __SteamAPI_ISteamUGC_StopPlaytimeTracking_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_StopPlaytimeTracking");
    if (!__SteamAPI_ISteamUGC_StopPlaytimeTracking_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_StopPlaytimeTracking\n"); complete = false; }
    __SteamAPI_ISteamUGC_StopPlaytimeTrackingForAllItems_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_StopPlaytimeTrackingForAllItems");
    if (!__SteamAPI_ISteamUGC_StopPlaytimeTrackingForAllItems_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_StopPlaytimeTrackingForAllItems\n"); complete = false; }
    __SteamAPI_ISteamUGC_SubmitItemUpdate_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SubmitItemUpdate");
    if (!__SteamAPI_ISteamUGC_SubmitItemUpdate_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SubmitItemUpdate\n"); complete = false; }
    __SteamAPI_ISteamUGC_SubscribeItem_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SubscribeItem");
    if (!__SteamAPI_ISteamUGC_SubscribeItem_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SubscribeItem\n"); complete = false; }
    __SteamAPI_ISteamUGC_SuspendDownloads_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_SuspendDownloads");
    if (!__SteamAPI_ISteamUGC_SuspendDownloads_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_SuspendDownloads\n"); complete = false; }
    __SteamAPI_ISteamUGC_UnsubscribeItem_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_UnsubscribeItem");
    if (!__SteamAPI_ISteamUGC_UnsubscribeItem_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_UnsubscribeItem\n"); complete = false; }
    __SteamAPI_ISteamUGC_UpdateItemPreviewFile_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_UpdateItemPreviewFile");
    if (!__SteamAPI_ISteamUGC_UpdateItemPreviewFile_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_UpdateItemPreviewFile\n"); complete = false; }
    __SteamAPI_ISteamUGC_UpdateItemPreviewVideo_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUGC_UpdateItemPreviewVideo");
    if (!__SteamAPI_ISteamUGC_UpdateItemPreviewVideo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUGC_UpdateItemPreviewVideo\n"); complete = false; }
    __SteamAPI_ISteamUserStats_AttachLeaderboardUGC_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_AttachLeaderboardUGC");
    if (!__SteamAPI_ISteamUserStats_AttachLeaderboardUGC_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_AttachLeaderboardUGC\n"); complete = false; }
    __SteamAPI_ISteamUserStats_ClearAchievement_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_ClearAchievement");
    if (!__SteamAPI_ISteamUserStats_ClearAchievement_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_ClearAchievement\n"); complete = false; }
    __SteamAPI_ISteamUserStats_DownloadLeaderboardEntries_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_DownloadLeaderboardEntries");
    if (!__SteamAPI_ISteamUserStats_DownloadLeaderboardEntries_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_DownloadLeaderboardEntries\n"); complete = false; }
    __SteamAPI_ISteamUserStats_DownloadLeaderboardEntriesForUsers_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_DownloadLeaderboardEntriesForUsers");
    if (!__SteamAPI_ISteamUserStats_DownloadLeaderboardEntriesForUsers_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_DownloadLeaderboardEntriesForUsers\n"); complete = false; }
    __SteamAPI_ISteamUserStats_FindLeaderboard_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_FindLeaderboard");
    if (!__SteamAPI_ISteamUserStats_FindLeaderboard_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_FindLeaderboard\n"); complete = false; }
    __SteamAPI_ISteamUserStats_FindOrCreateLeaderboard_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_FindOrCreateLeaderboard");
    if (!__SteamAPI_ISteamUserStats_FindOrCreateLeaderboard_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_FindOrCreateLeaderboard\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetAchievement_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetAchievement");
    if (!__SteamAPI_ISteamUserStats_GetAchievement_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetAchievement\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetAchievementAchievedPercent_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetAchievementAchievedPercent");
    if (!__SteamAPI_ISteamUserStats_GetAchievementAchievedPercent_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetAchievementAchievedPercent\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetAchievementAndUnlockTime_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetAchievementAndUnlockTime");
    if (!__SteamAPI_ISteamUserStats_GetAchievementAndUnlockTime_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetAchievementAndUnlockTime\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute");
    if (!__SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetAchievementIcon_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetAchievementIcon");
    if (!__SteamAPI_ISteamUserStats_GetAchievementIcon_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetAchievementIcon\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetAchievementName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetAchievementName");
    if (!__SteamAPI_ISteamUserStats_GetAchievementName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetAchievementName\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetAchievementProgressLimitsFloat_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetAchievementProgressLimitsFloat");
    if (!__SteamAPI_ISteamUserStats_GetAchievementProgressLimitsFloat_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetAchievementProgressLimitsFloat\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetAchievementProgressLimitsInt32_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetAchievementProgressLimitsInt32");
    if (!__SteamAPI_ISteamUserStats_GetAchievementProgressLimitsInt32_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetAchievementProgressLimitsInt32\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry");
    if (!__SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetGlobalStatDouble_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetGlobalStatDouble");
    if (!__SteamAPI_ISteamUserStats_GetGlobalStatDouble_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetGlobalStatDouble\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetGlobalStatHistoryDouble_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetGlobalStatHistoryDouble");
    if (!__SteamAPI_ISteamUserStats_GetGlobalStatHistoryDouble_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetGlobalStatHistoryDouble\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetGlobalStatHistoryInt64_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetGlobalStatHistoryInt64");
    if (!__SteamAPI_ISteamUserStats_GetGlobalStatHistoryInt64_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetGlobalStatHistoryInt64\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetGlobalStatInt64_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetGlobalStatInt64");
    if (!__SteamAPI_ISteamUserStats_GetGlobalStatInt64_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetGlobalStatInt64\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetLeaderboardDisplayType_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetLeaderboardDisplayType");
    if (!__SteamAPI_ISteamUserStats_GetLeaderboardDisplayType_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetLeaderboardDisplayType\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetLeaderboardEntryCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetLeaderboardEntryCount");
    if (!__SteamAPI_ISteamUserStats_GetLeaderboardEntryCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetLeaderboardEntryCount\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetLeaderboardName_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetLeaderboardName");
    if (!__SteamAPI_ISteamUserStats_GetLeaderboardName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetLeaderboardName\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetLeaderboardSortMethod_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetLeaderboardSortMethod");
    if (!__SteamAPI_ISteamUserStats_GetLeaderboardSortMethod_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetLeaderboardSortMethod\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetMostAchievedAchievementInfo_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetMostAchievedAchievementInfo");
    if (!__SteamAPI_ISteamUserStats_GetMostAchievedAchievementInfo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetMostAchievedAchievementInfo\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetNextMostAchievedAchievementInfo_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetNextMostAchievedAchievementInfo");
    if (!__SteamAPI_ISteamUserStats_GetNextMostAchievedAchievementInfo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetNextMostAchievedAchievementInfo\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetNumAchievements_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetNumAchievements");
    if (!__SteamAPI_ISteamUserStats_GetNumAchievements_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetNumAchievements\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetNumberOfCurrentPlayers_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetNumberOfCurrentPlayers");
    if (!__SteamAPI_ISteamUserStats_GetNumberOfCurrentPlayers_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetNumberOfCurrentPlayers\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetStatFloat_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetStatFloat");
    if (!__SteamAPI_ISteamUserStats_GetStatFloat_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetStatFloat\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetStatInt32_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetStatInt32");
    if (!__SteamAPI_ISteamUserStats_GetStatInt32_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetStatInt32\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetUserAchievement_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetUserAchievement");
    if (!__SteamAPI_ISteamUserStats_GetUserAchievement_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetUserAchievement\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetUserAchievementAndUnlockTime_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetUserAchievementAndUnlockTime");
    if (!__SteamAPI_ISteamUserStats_GetUserAchievementAndUnlockTime_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetUserAchievementAndUnlockTime\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetUserStatFloat_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetUserStatFloat");
    if (!__SteamAPI_ISteamUserStats_GetUserStatFloat_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetUserStatFloat\n"); complete = false; }
    __SteamAPI_ISteamUserStats_GetUserStatInt32_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_GetUserStatInt32");
    if (!__SteamAPI_ISteamUserStats_GetUserStatInt32_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_GetUserStatInt32\n"); complete = false; }
    __SteamAPI_ISteamUserStats_IndicateAchievementProgress_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_IndicateAchievementProgress");
    if (!__SteamAPI_ISteamUserStats_IndicateAchievementProgress_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_IndicateAchievementProgress\n"); complete = false; }
    __SteamAPI_ISteamUserStats_RequestGlobalAchievementPercentages_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_RequestGlobalAchievementPercentages");
    if (!__SteamAPI_ISteamUserStats_RequestGlobalAchievementPercentages_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_RequestGlobalAchievementPercentages\n"); complete = false; }
    __SteamAPI_ISteamUserStats_RequestGlobalStats_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_RequestGlobalStats");
    if (!__SteamAPI_ISteamUserStats_RequestGlobalStats_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_RequestGlobalStats\n"); complete = false; }
    __SteamAPI_ISteamUserStats_RequestUserStats_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_RequestUserStats");
    if (!__SteamAPI_ISteamUserStats_RequestUserStats_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_RequestUserStats\n"); complete = false; }
    __SteamAPI_ISteamUserStats_ResetAllStats_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_ResetAllStats");
    if (!__SteamAPI_ISteamUserStats_ResetAllStats_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_ResetAllStats\n"); complete = false; }
    __SteamAPI_ISteamUserStats_SetAchievement_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_SetAchievement");
    if (!__SteamAPI_ISteamUserStats_SetAchievement_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_SetAchievement\n"); complete = false; }
    __SteamAPI_ISteamUserStats_SetStatFloat_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_SetStatFloat");
    if (!__SteamAPI_ISteamUserStats_SetStatFloat_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_SetStatFloat\n"); complete = false; }
    __SteamAPI_ISteamUserStats_SetStatInt32_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_SetStatInt32");
    if (!__SteamAPI_ISteamUserStats_SetStatInt32_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_SetStatInt32\n"); complete = false; }
    __SteamAPI_ISteamUserStats_StoreStats_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_StoreStats");
    if (!__SteamAPI_ISteamUserStats_StoreStats_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_StoreStats\n"); complete = false; }
    __SteamAPI_ISteamUserStats_UpdateAvgRateStat_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_UpdateAvgRateStat");
    if (!__SteamAPI_ISteamUserStats_UpdateAvgRateStat_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_UpdateAvgRateStat\n"); complete = false; }
    __SteamAPI_ISteamUserStats_UploadLeaderboardScore_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUserStats_UploadLeaderboardScore");
    if (!__SteamAPI_ISteamUserStats_UploadLeaderboardScore_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUserStats_UploadLeaderboardScore\n"); complete = false; }
    __SteamAPI_ISteamUser_AdvertiseGame_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_AdvertiseGame");
    if (!__SteamAPI_ISteamUser_AdvertiseGame_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_AdvertiseGame\n"); complete = false; }
    __SteamAPI_ISteamUser_BIsBehindNAT_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_BIsBehindNAT");
    if (!__SteamAPI_ISteamUser_BIsBehindNAT_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_BIsBehindNAT\n"); complete = false; }
    __SteamAPI_ISteamUser_BIsPhoneIdentifying_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_BIsPhoneIdentifying");
    if (!__SteamAPI_ISteamUser_BIsPhoneIdentifying_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_BIsPhoneIdentifying\n"); complete = false; }
    __SteamAPI_ISteamUser_BIsPhoneRequiringVerification_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_BIsPhoneRequiringVerification");
    if (!__SteamAPI_ISteamUser_BIsPhoneRequiringVerification_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_BIsPhoneRequiringVerification\n"); complete = false; }
    __SteamAPI_ISteamUser_BIsPhoneVerified_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_BIsPhoneVerified");
    if (!__SteamAPI_ISteamUser_BIsPhoneVerified_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_BIsPhoneVerified\n"); complete = false; }
    __SteamAPI_ISteamUser_BIsTwoFactorEnabled_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_BIsTwoFactorEnabled");
    if (!__SteamAPI_ISteamUser_BIsTwoFactorEnabled_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_BIsTwoFactorEnabled\n"); complete = false; }
    __SteamAPI_ISteamUser_BLoggedOn_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_BLoggedOn");
    if (!__SteamAPI_ISteamUser_BLoggedOn_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_BLoggedOn\n"); complete = false; }
    __SteamAPI_ISteamUser_BSetDurationControlOnlineState_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_BSetDurationControlOnlineState");
    if (!__SteamAPI_ISteamUser_BSetDurationControlOnlineState_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_BSetDurationControlOnlineState\n"); complete = false; }
    __SteamAPI_ISteamUser_BeginAuthSession_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_BeginAuthSession");
    if (!__SteamAPI_ISteamUser_BeginAuthSession_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_BeginAuthSession\n"); complete = false; }
    __SteamAPI_ISteamUser_CancelAuthTicket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_CancelAuthTicket");
    if (!__SteamAPI_ISteamUser_CancelAuthTicket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_CancelAuthTicket\n"); complete = false; }
    __SteamAPI_ISteamUser_DecompressVoice_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_DecompressVoice");
    if (!__SteamAPI_ISteamUser_DecompressVoice_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_DecompressVoice\n"); complete = false; }
    __SteamAPI_ISteamUser_EndAuthSession_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_EndAuthSession");
    if (!__SteamAPI_ISteamUser_EndAuthSession_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_EndAuthSession\n"); complete = false; }
    __SteamAPI_ISteamUser_GetAuthSessionTicket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_GetAuthSessionTicket");
    if (!__SteamAPI_ISteamUser_GetAuthSessionTicket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_GetAuthSessionTicket\n"); complete = false; }
    __SteamAPI_ISteamUser_GetAuthTicketForWebApi_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_GetAuthTicketForWebApi");
    if (!__SteamAPI_ISteamUser_GetAuthTicketForWebApi_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_GetAuthTicketForWebApi\n"); complete = false; }
    __SteamAPI_ISteamUser_GetAvailableVoice_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_GetAvailableVoice");
    if (!__SteamAPI_ISteamUser_GetAvailableVoice_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_GetAvailableVoice\n"); complete = false; }
    __SteamAPI_ISteamUser_GetDurationControl_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_GetDurationControl");
    if (!__SteamAPI_ISteamUser_GetDurationControl_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_GetDurationControl\n"); complete = false; }
    __SteamAPI_ISteamUser_GetEncryptedAppTicket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_GetEncryptedAppTicket");
    if (!__SteamAPI_ISteamUser_GetEncryptedAppTicket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_GetEncryptedAppTicket\n"); complete = false; }
    __SteamAPI_ISteamUser_GetGameBadgeLevel_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_GetGameBadgeLevel");
    if (!__SteamAPI_ISteamUser_GetGameBadgeLevel_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_GetGameBadgeLevel\n"); complete = false; }
    __SteamAPI_ISteamUser_GetHSteamUser_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_GetHSteamUser");
    if (!__SteamAPI_ISteamUser_GetHSteamUser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_GetHSteamUser\n"); complete = false; }
    __SteamAPI_ISteamUser_GetMarketEligibility_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_GetMarketEligibility");
    if (!__SteamAPI_ISteamUser_GetMarketEligibility_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_GetMarketEligibility\n"); complete = false; }
    __SteamAPI_ISteamUser_GetPlayerSteamLevel_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_GetPlayerSteamLevel");
    if (!__SteamAPI_ISteamUser_GetPlayerSteamLevel_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_GetPlayerSteamLevel\n"); complete = false; }
    __SteamAPI_ISteamUser_GetSteamID_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_GetSteamID");
    if (!__SteamAPI_ISteamUser_GetSteamID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_GetSteamID\n"); complete = false; }
    __SteamAPI_ISteamUser_GetUserDataFolder_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_GetUserDataFolder");
    if (!__SteamAPI_ISteamUser_GetUserDataFolder_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_GetUserDataFolder\n"); complete = false; }
    __SteamAPI_ISteamUser_GetVoice_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_GetVoice");
    if (!__SteamAPI_ISteamUser_GetVoice_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_GetVoice\n"); complete = false; }
    __SteamAPI_ISteamUser_GetVoiceOptimalSampleRate_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_GetVoiceOptimalSampleRate");
    if (!__SteamAPI_ISteamUser_GetVoiceOptimalSampleRate_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_GetVoiceOptimalSampleRate\n"); complete = false; }
    __SteamAPI_ISteamUser_InitiateGameConnection_DEPRECATED_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_InitiateGameConnection_DEPRECATED");
    if (!__SteamAPI_ISteamUser_InitiateGameConnection_DEPRECATED_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_InitiateGameConnection_DEPRECATED\n"); complete = false; }
    __SteamAPI_ISteamUser_RequestEncryptedAppTicket_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_RequestEncryptedAppTicket");
    if (!__SteamAPI_ISteamUser_RequestEncryptedAppTicket_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_RequestEncryptedAppTicket\n"); complete = false; }
    __SteamAPI_ISteamUser_RequestStoreAuthURL_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_RequestStoreAuthURL");
    if (!__SteamAPI_ISteamUser_RequestStoreAuthURL_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_RequestStoreAuthURL\n"); complete = false; }
    __SteamAPI_ISteamUser_StartVoiceRecording_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_StartVoiceRecording");
    if (!__SteamAPI_ISteamUser_StartVoiceRecording_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_StartVoiceRecording\n"); complete = false; }
    __SteamAPI_ISteamUser_StopVoiceRecording_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_StopVoiceRecording");
    if (!__SteamAPI_ISteamUser_StopVoiceRecording_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_StopVoiceRecording\n"); complete = false; }
    __SteamAPI_ISteamUser_TerminateGameConnection_DEPRECATED_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_TerminateGameConnection_DEPRECATED");
    if (!__SteamAPI_ISteamUser_TerminateGameConnection_DEPRECATED_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_TerminateGameConnection_DEPRECATED\n"); complete = false; }
    __SteamAPI_ISteamUser_TrackAppUsageEvent_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_TrackAppUsageEvent");
    if (!__SteamAPI_ISteamUser_TrackAppUsageEvent_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_TrackAppUsageEvent\n"); complete = false; }
    __SteamAPI_ISteamUser_UserHasLicenseForApp_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUser_UserHasLicenseForApp");
    if (!__SteamAPI_ISteamUser_UserHasLicenseForApp_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUser_UserHasLicenseForApp\n"); complete = false; }
    __SteamAPI_ISteamUtils_BOverlayNeedsPresent_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_BOverlayNeedsPresent");
    if (!__SteamAPI_ISteamUtils_BOverlayNeedsPresent_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_BOverlayNeedsPresent\n"); complete = false; }
    __SteamAPI_ISteamUtils_CheckFileSignature_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_CheckFileSignature");
    if (!__SteamAPI_ISteamUtils_CheckFileSignature_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_CheckFileSignature\n"); complete = false; }
    __SteamAPI_ISteamUtils_DismissFloatingGamepadTextInput_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_DismissFloatingGamepadTextInput");
    if (!__SteamAPI_ISteamUtils_DismissFloatingGamepadTextInput_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_DismissFloatingGamepadTextInput\n"); complete = false; }
    __SteamAPI_ISteamUtils_DismissGamepadTextInput_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_DismissGamepadTextInput");
    if (!__SteamAPI_ISteamUtils_DismissGamepadTextInput_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_DismissGamepadTextInput\n"); complete = false; }
    __SteamAPI_ISteamUtils_FilterText_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_FilterText");
    if (!__SteamAPI_ISteamUtils_FilterText_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_FilterText\n"); complete = false; }
    __SteamAPI_ISteamUtils_GetAPICallFailureReason_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_GetAPICallFailureReason");
    if (!__SteamAPI_ISteamUtils_GetAPICallFailureReason_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_GetAPICallFailureReason\n"); complete = false; }
    __SteamAPI_ISteamUtils_GetAPICallResult_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_GetAPICallResult");
    if (!__SteamAPI_ISteamUtils_GetAPICallResult_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_GetAPICallResult\n"); complete = false; }
    __SteamAPI_ISteamUtils_GetAppID_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_GetAppID");
    if (!__SteamAPI_ISteamUtils_GetAppID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_GetAppID\n"); complete = false; }
    __SteamAPI_ISteamUtils_GetConnectedUniverse_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_GetConnectedUniverse");
    if (!__SteamAPI_ISteamUtils_GetConnectedUniverse_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_GetConnectedUniverse\n"); complete = false; }
    __SteamAPI_ISteamUtils_GetCurrentBatteryPower_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_GetCurrentBatteryPower");
    if (!__SteamAPI_ISteamUtils_GetCurrentBatteryPower_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_GetCurrentBatteryPower\n"); complete = false; }
    __SteamAPI_ISteamUtils_GetEnteredGamepadTextInput_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_GetEnteredGamepadTextInput");
    if (!__SteamAPI_ISteamUtils_GetEnteredGamepadTextInput_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_GetEnteredGamepadTextInput\n"); complete = false; }
    __SteamAPI_ISteamUtils_GetEnteredGamepadTextLength_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_GetEnteredGamepadTextLength");
    if (!__SteamAPI_ISteamUtils_GetEnteredGamepadTextLength_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_GetEnteredGamepadTextLength\n"); complete = false; }
    __SteamAPI_ISteamUtils_GetIPCCallCount_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_GetIPCCallCount");
    if (!__SteamAPI_ISteamUtils_GetIPCCallCount_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_GetIPCCallCount\n"); complete = false; }
    __SteamAPI_ISteamUtils_GetIPCountry_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_GetIPCountry");
    if (!__SteamAPI_ISteamUtils_GetIPCountry_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_GetIPCountry\n"); complete = false; }
    __SteamAPI_ISteamUtils_GetIPv6ConnectivityState_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_GetIPv6ConnectivityState");
    if (!__SteamAPI_ISteamUtils_GetIPv6ConnectivityState_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_GetIPv6ConnectivityState\n"); complete = false; }
    __SteamAPI_ISteamUtils_GetImageRGBA_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_GetImageRGBA");
    if (!__SteamAPI_ISteamUtils_GetImageRGBA_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_GetImageRGBA\n"); complete = false; }
    __SteamAPI_ISteamUtils_GetImageSize_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_GetImageSize");
    if (!__SteamAPI_ISteamUtils_GetImageSize_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_GetImageSize\n"); complete = false; }
    __SteamAPI_ISteamUtils_GetSecondsSinceAppActive_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_GetSecondsSinceAppActive");
    if (!__SteamAPI_ISteamUtils_GetSecondsSinceAppActive_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_GetSecondsSinceAppActive\n"); complete = false; }
    __SteamAPI_ISteamUtils_GetSecondsSinceComputerActive_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_GetSecondsSinceComputerActive");
    if (!__SteamAPI_ISteamUtils_GetSecondsSinceComputerActive_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_GetSecondsSinceComputerActive\n"); complete = false; }
    __SteamAPI_ISteamUtils_GetServerRealTime_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_GetServerRealTime");
    if (!__SteamAPI_ISteamUtils_GetServerRealTime_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_GetServerRealTime\n"); complete = false; }
    __SteamAPI_ISteamUtils_GetSteamUILanguage_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_GetSteamUILanguage");
    if (!__SteamAPI_ISteamUtils_GetSteamUILanguage_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_GetSteamUILanguage\n"); complete = false; }
    __SteamAPI_ISteamUtils_InitFilterText_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_InitFilterText");
    if (!__SteamAPI_ISteamUtils_InitFilterText_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_InitFilterText\n"); complete = false; }
    __SteamAPI_ISteamUtils_IsAPICallCompleted_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_IsAPICallCompleted");
    if (!__SteamAPI_ISteamUtils_IsAPICallCompleted_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_IsAPICallCompleted\n"); complete = false; }
    __SteamAPI_ISteamUtils_IsOverlayEnabled_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_IsOverlayEnabled");
    if (!__SteamAPI_ISteamUtils_IsOverlayEnabled_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_IsOverlayEnabled\n"); complete = false; }
    __SteamAPI_ISteamUtils_IsSteamChinaLauncher_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_IsSteamChinaLauncher");
    if (!__SteamAPI_ISteamUtils_IsSteamChinaLauncher_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_IsSteamChinaLauncher\n"); complete = false; }
    __SteamAPI_ISteamUtils_IsSteamInBigPictureMode_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_IsSteamInBigPictureMode");
    if (!__SteamAPI_ISteamUtils_IsSteamInBigPictureMode_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_IsSteamInBigPictureMode\n"); complete = false; }
    __SteamAPI_ISteamUtils_IsSteamRunningInVR_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_IsSteamRunningInVR");
    if (!__SteamAPI_ISteamUtils_IsSteamRunningInVR_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_IsSteamRunningInVR\n"); complete = false; }
    __SteamAPI_ISteamUtils_IsSteamRunningOnSteamDeck_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_IsSteamRunningOnSteamDeck");
    if (!__SteamAPI_ISteamUtils_IsSteamRunningOnSteamDeck_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_IsSteamRunningOnSteamDeck\n"); complete = false; }
    __SteamAPI_ISteamUtils_IsVRHeadsetStreamingEnabled_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_IsVRHeadsetStreamingEnabled");
    if (!__SteamAPI_ISteamUtils_IsVRHeadsetStreamingEnabled_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_IsVRHeadsetStreamingEnabled\n"); complete = false; }
    __SteamAPI_ISteamUtils_SetGameLauncherMode_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_SetGameLauncherMode");
    if (!__SteamAPI_ISteamUtils_SetGameLauncherMode_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_SetGameLauncherMode\n"); complete = false; }
    __SteamAPI_ISteamUtils_SetOverlayNotificationInset_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_SetOverlayNotificationInset");
    if (!__SteamAPI_ISteamUtils_SetOverlayNotificationInset_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_SetOverlayNotificationInset\n"); complete = false; }
    __SteamAPI_ISteamUtils_SetOverlayNotificationPosition_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_SetOverlayNotificationPosition");
    if (!__SteamAPI_ISteamUtils_SetOverlayNotificationPosition_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_SetOverlayNotificationPosition\n"); complete = false; }
    __SteamAPI_ISteamUtils_SetVRHeadsetStreamingEnabled_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_SetVRHeadsetStreamingEnabled");
    if (!__SteamAPI_ISteamUtils_SetVRHeadsetStreamingEnabled_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_SetVRHeadsetStreamingEnabled\n"); complete = false; }
    __SteamAPI_ISteamUtils_SetWarningMessageHook_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_SetWarningMessageHook");
    if (!__SteamAPI_ISteamUtils_SetWarningMessageHook_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_SetWarningMessageHook\n"); complete = false; }
    __SteamAPI_ISteamUtils_ShowFloatingGamepadTextInput_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_ShowFloatingGamepadTextInput");
    if (!__SteamAPI_ISteamUtils_ShowFloatingGamepadTextInput_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_ShowFloatingGamepadTextInput\n"); complete = false; }
    __SteamAPI_ISteamUtils_ShowGamepadTextInput_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_ShowGamepadTextInput");
    if (!__SteamAPI_ISteamUtils_ShowGamepadTextInput_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_ShowGamepadTextInput\n"); complete = false; }
    __SteamAPI_ISteamUtils_StartVRDashboard_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamUtils_StartVRDashboard");
    if (!__SteamAPI_ISteamUtils_StartVRDashboard_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamUtils_StartVRDashboard\n"); complete = false; }
    __SteamAPI_ISteamVideo_GetOPFSettings_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamVideo_GetOPFSettings");
    if (!__SteamAPI_ISteamVideo_GetOPFSettings_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamVideo_GetOPFSettings\n"); complete = false; }
    __SteamAPI_ISteamVideo_GetOPFStringForApp_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamVideo_GetOPFStringForApp");
    if (!__SteamAPI_ISteamVideo_GetOPFStringForApp_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamVideo_GetOPFStringForApp\n"); complete = false; }
    __SteamAPI_ISteamVideo_GetVideoURL_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamVideo_GetVideoURL");
    if (!__SteamAPI_ISteamVideo_GetVideoURL_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamVideo_GetVideoURL\n"); complete = false; }
    __SteamAPI_ISteamVideo_IsBroadcasting_ptr = (void*)GetProcAddress(original, "SteamAPI_ISteamVideo_IsBroadcasting");
    if (!__SteamAPI_ISteamVideo_IsBroadcasting_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ISteamVideo_IsBroadcasting\n"); complete = false; }
    __SteamAPI_InitAnonymousUser_ptr = (void*)GetProcAddress(original, "SteamAPI_InitAnonymousUser");
    if (!__SteamAPI_InitAnonymousUser_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_InitAnonymousUser\n"); complete = false; }
    __SteamAPI_InitFlat_ptr = (void*)GetProcAddress(original, "SteamAPI_InitFlat");
    if (!__SteamAPI_InitFlat_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_InitFlat\n"); complete = false; }
    __SteamAPI_InitSafe_ptr = (void*)GetProcAddress(original, "SteamAPI_InitSafe");
    if (!__SteamAPI_InitSafe_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_InitSafe\n"); complete = false; }
    __SteamAPI_IsSteamRunning_ptr = (void*)GetProcAddress(original, "SteamAPI_IsSteamRunning");
    if (!__SteamAPI_IsSteamRunning_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_IsSteamRunning\n"); complete = false; }
    __SteamAPI_ManualDispatch_FreeLastCallback_ptr = (void*)GetProcAddress(original, "SteamAPI_ManualDispatch_FreeLastCallback");
    if (!__SteamAPI_ManualDispatch_FreeLastCallback_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ManualDispatch_FreeLastCallback\n"); complete = false; }
    __SteamAPI_ManualDispatch_GetAPICallResult_ptr = (void*)GetProcAddress(original, "SteamAPI_ManualDispatch_GetAPICallResult");
    if (!__SteamAPI_ManualDispatch_GetAPICallResult_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ManualDispatch_GetAPICallResult\n"); complete = false; }
    __SteamAPI_ManualDispatch_GetNextCallback_ptr = (void*)GetProcAddress(original, "SteamAPI_ManualDispatch_GetNextCallback");
    if (!__SteamAPI_ManualDispatch_GetNextCallback_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ManualDispatch_GetNextCallback\n"); complete = false; }
    __SteamAPI_ManualDispatch_Init_ptr = (void*)GetProcAddress(original, "SteamAPI_ManualDispatch_Init");
    if (!__SteamAPI_ManualDispatch_Init_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ManualDispatch_Init\n"); complete = false; }
    __SteamAPI_ManualDispatch_RunFrame_ptr = (void*)GetProcAddress(original, "SteamAPI_ManualDispatch_RunFrame");
    if (!__SteamAPI_ManualDispatch_RunFrame_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ManualDispatch_RunFrame\n"); complete = false; }
    __SteamAPI_MatchMakingKeyValuePair_t_Construct_ptr = (void*)GetProcAddress(original, "SteamAPI_MatchMakingKeyValuePair_t_Construct");
    if (!__SteamAPI_MatchMakingKeyValuePair_t_Construct_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_MatchMakingKeyValuePair_t_Construct\n"); complete = false; }
    __SteamAPI_RegisterCallResult_ptr = (void*)GetProcAddress(original, "SteamAPI_RegisterCallResult");
    if (!__SteamAPI_RegisterCallResult_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_RegisterCallResult\n"); complete = false; }
    __SteamAPI_RegisterCallback_ptr = (void*)GetProcAddress(original, "SteamAPI_RegisterCallback");
    if (!__SteamAPI_RegisterCallback_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_RegisterCallback\n"); complete = false; }
    __SteamAPI_ReleaseCurrentThreadMemory_ptr = (void*)GetProcAddress(original, "SteamAPI_ReleaseCurrentThreadMemory");
    if (!__SteamAPI_ReleaseCurrentThreadMemory_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_ReleaseCurrentThreadMemory\n"); complete = false; }
    __SteamAPI_RestartAppIfNecessary_ptr = (void*)GetProcAddress(original, "SteamAPI_RestartAppIfNecessary");
    if (!__SteamAPI_RestartAppIfNecessary_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_RestartAppIfNecessary\n"); complete = false; }
    __SteamAPI_SetBreakpadAppID_ptr = (void*)GetProcAddress(original, "SteamAPI_SetBreakpadAppID");
    if (!__SteamAPI_SetBreakpadAppID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SetBreakpadAppID\n"); complete = false; }
    __SteamAPI_SetMiniDumpComment_ptr = (void*)GetProcAddress(original, "SteamAPI_SetMiniDumpComment");
    if (!__SteamAPI_SetMiniDumpComment_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SetMiniDumpComment\n"); complete = false; }
    __SteamAPI_SetTryCatchCallbacks_ptr = (void*)GetProcAddress(original, "SteamAPI_SetTryCatchCallbacks");
    if (!__SteamAPI_SetTryCatchCallbacks_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SetTryCatchCallbacks\n"); complete = false; }
    __SteamAPI_Shutdown_ptr = (void*)GetProcAddress(original, "SteamAPI_Shutdown");
    if (!__SteamAPI_Shutdown_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_Shutdown\n"); complete = false; }
    __SteamAPI_SteamApps_v008_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamApps_v008");
    if (!__SteamAPI_SteamApps_v008_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamApps_v008\n"); complete = false; }
    __SteamAPI_SteamController_v008_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamController_v008");
    if (!__SteamAPI_SteamController_v008_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamController_v008\n"); complete = false; }
    __SteamAPI_SteamDatagramHostedAddress_Clear_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamDatagramHostedAddress_Clear");
    if (!__SteamAPI_SteamDatagramHostedAddress_Clear_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamDatagramHostedAddress_Clear\n"); complete = false; }
    __SteamAPI_SteamDatagramHostedAddress_GetPopID_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamDatagramHostedAddress_GetPopID");
    if (!__SteamAPI_SteamDatagramHostedAddress_GetPopID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamDatagramHostedAddress_GetPopID\n"); complete = false; }
    __SteamAPI_SteamDatagramHostedAddress_SetDevAddress_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamDatagramHostedAddress_SetDevAddress");
    if (!__SteamAPI_SteamDatagramHostedAddress_SetDevAddress_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamDatagramHostedAddress_SetDevAddress\n"); complete = false; }
    __SteamAPI_SteamFriends_v018_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamFriends_v018");
    if (!__SteamAPI_SteamFriends_v018_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamFriends_v018\n"); complete = false; }
    __SteamAPI_SteamGameSearch_v001_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamGameSearch_v001");
    if (!__SteamAPI_SteamGameSearch_v001_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamGameSearch_v001\n"); complete = false; }
    __SteamAPI_SteamGameServerHTTP_v003_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamGameServerHTTP_v003");
    if (!__SteamAPI_SteamGameServerHTTP_v003_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamGameServerHTTP_v003\n"); complete = false; }
    __SteamAPI_SteamGameServerInventory_v003_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamGameServerInventory_v003");
    if (!__SteamAPI_SteamGameServerInventory_v003_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamGameServerInventory_v003\n"); complete = false; }
    __SteamAPI_SteamGameServerNetworkingMessages_SteamAPI_v002_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamGameServerNetworkingMessages_SteamAPI_v002");
    if (!__SteamAPI_SteamGameServerNetworkingMessages_SteamAPI_v002_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamGameServerNetworkingMessages_SteamAPI_v002\n"); complete = false; }
    __SteamAPI_SteamGameServerNetworkingSockets_SteamAPI_v012_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamGameServerNetworkingSockets_SteamAPI_v012");
    if (!__SteamAPI_SteamGameServerNetworkingSockets_SteamAPI_v012_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamGameServerNetworkingSockets_SteamAPI_v012\n"); complete = false; }
    __SteamAPI_SteamGameServerNetworking_v006_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamGameServerNetworking_v006");
    if (!__SteamAPI_SteamGameServerNetworking_v006_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamGameServerNetworking_v006\n"); complete = false; }
    __SteamAPI_SteamGameServerStats_v001_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamGameServerStats_v001");
    if (!__SteamAPI_SteamGameServerStats_v001_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamGameServerStats_v001\n"); complete = false; }
    __SteamAPI_SteamGameServerUGC_v021_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamGameServerUGC_v021");
    if (!__SteamAPI_SteamGameServerUGC_v021_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamGameServerUGC_v021\n"); complete = false; }
    __SteamAPI_SteamGameServerUtils_v010_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamGameServerUtils_v010");
    if (!__SteamAPI_SteamGameServerUtils_v010_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamGameServerUtils_v010\n"); complete = false; }
    __SteamAPI_SteamGameServer_v015_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamGameServer_v015");
    if (!__SteamAPI_SteamGameServer_v015_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamGameServer_v015\n"); complete = false; }
    __SteamAPI_SteamHTMLSurface_v005_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamHTMLSurface_v005");
    if (!__SteamAPI_SteamHTMLSurface_v005_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamHTMLSurface_v005\n"); complete = false; }
    __SteamAPI_SteamHTTP_v003_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamHTTP_v003");
    if (!__SteamAPI_SteamHTTP_v003_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamHTTP_v003\n"); complete = false; }
    __SteamAPI_SteamIPAddress_t_IsSet_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamIPAddress_t_IsSet");
    if (!__SteamAPI_SteamIPAddress_t_IsSet_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamIPAddress_t_IsSet\n"); complete = false; }
    __SteamAPI_SteamInput_v006_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamInput_v006");
    if (!__SteamAPI_SteamInput_v006_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamInput_v006\n"); complete = false; }
    __SteamAPI_SteamInventory_v003_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamInventory_v003");
    if (!__SteamAPI_SteamInventory_v003_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamInventory_v003\n"); complete = false; }
    __SteamAPI_SteamMatchmakingServers_v002_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamMatchmakingServers_v002");
    if (!__SteamAPI_SteamMatchmakingServers_v002_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamMatchmakingServers_v002\n"); complete = false; }
    __SteamAPI_SteamMatchmaking_v009_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamMatchmaking_v009");
    if (!__SteamAPI_SteamMatchmaking_v009_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamMatchmaking_v009\n"); complete = false; }
    __SteamAPI_SteamMusicRemote_v001_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamMusicRemote_v001");
    if (!__SteamAPI_SteamMusicRemote_v001_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamMusicRemote_v001\n"); complete = false; }
    __SteamAPI_SteamMusic_v001_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamMusic_v001");
    if (!__SteamAPI_SteamMusic_v001_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamMusic_v001\n"); complete = false; }
    __SteamAPI_SteamNetworkingConfigValue_t_SetFloat_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingConfigValue_t_SetFloat");
    if (!__SteamAPI_SteamNetworkingConfigValue_t_SetFloat_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingConfigValue_t_SetFloat\n"); complete = false; }
    __SteamAPI_SteamNetworkingConfigValue_t_SetInt32_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingConfigValue_t_SetInt32");
    if (!__SteamAPI_SteamNetworkingConfigValue_t_SetInt32_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingConfigValue_t_SetInt32\n"); complete = false; }
    __SteamAPI_SteamNetworkingConfigValue_t_SetInt64_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingConfigValue_t_SetInt64");
    if (!__SteamAPI_SteamNetworkingConfigValue_t_SetInt64_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingConfigValue_t_SetInt64\n"); complete = false; }
    __SteamAPI_SteamNetworkingConfigValue_t_SetPtr_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingConfigValue_t_SetPtr");
    if (!__SteamAPI_SteamNetworkingConfigValue_t_SetPtr_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingConfigValue_t_SetPtr\n"); complete = false; }
    __SteamAPI_SteamNetworkingConfigValue_t_SetString_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingConfigValue_t_SetString");
    if (!__SteamAPI_SteamNetworkingConfigValue_t_SetString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingConfigValue_t_SetString\n"); complete = false; }
    __SteamAPI_SteamNetworkingIPAddr_Clear_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIPAddr_Clear");
    if (!__SteamAPI_SteamNetworkingIPAddr_Clear_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIPAddr_Clear\n"); complete = false; }
    __SteamAPI_SteamNetworkingIPAddr_GetFakeIPType_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIPAddr_GetFakeIPType");
    if (!__SteamAPI_SteamNetworkingIPAddr_GetFakeIPType_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIPAddr_GetFakeIPType\n"); complete = false; }
    __SteamAPI_SteamNetworkingIPAddr_GetIPv4_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIPAddr_GetIPv4");
    if (!__SteamAPI_SteamNetworkingIPAddr_GetIPv4_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIPAddr_GetIPv4\n"); complete = false; }
    __SteamAPI_SteamNetworkingIPAddr_IsEqualTo_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIPAddr_IsEqualTo");
    if (!__SteamAPI_SteamNetworkingIPAddr_IsEqualTo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIPAddr_IsEqualTo\n"); complete = false; }
    __SteamAPI_SteamNetworkingIPAddr_IsFakeIP_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIPAddr_IsFakeIP");
    if (!__SteamAPI_SteamNetworkingIPAddr_IsFakeIP_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIPAddr_IsFakeIP\n"); complete = false; }
    __SteamAPI_SteamNetworkingIPAddr_IsIPv4_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIPAddr_IsIPv4");
    if (!__SteamAPI_SteamNetworkingIPAddr_IsIPv4_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIPAddr_IsIPv4\n"); complete = false; }
    __SteamAPI_SteamNetworkingIPAddr_IsIPv6AllZeros_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIPAddr_IsIPv6AllZeros");
    if (!__SteamAPI_SteamNetworkingIPAddr_IsIPv6AllZeros_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIPAddr_IsIPv6AllZeros\n"); complete = false; }
    __SteamAPI_SteamNetworkingIPAddr_IsLocalHost_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIPAddr_IsLocalHost");
    if (!__SteamAPI_SteamNetworkingIPAddr_IsLocalHost_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIPAddr_IsLocalHost\n"); complete = false; }
    __SteamAPI_SteamNetworkingIPAddr_ParseString_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIPAddr_ParseString");
    if (!__SteamAPI_SteamNetworkingIPAddr_ParseString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIPAddr_ParseString\n"); complete = false; }
    __SteamAPI_SteamNetworkingIPAddr_SetIPv4_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIPAddr_SetIPv4");
    if (!__SteamAPI_SteamNetworkingIPAddr_SetIPv4_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIPAddr_SetIPv4\n"); complete = false; }
    __SteamAPI_SteamNetworkingIPAddr_SetIPv6_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIPAddr_SetIPv6");
    if (!__SteamAPI_SteamNetworkingIPAddr_SetIPv6_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIPAddr_SetIPv6\n"); complete = false; }
    __SteamAPI_SteamNetworkingIPAddr_SetIPv6LocalHost_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIPAddr_SetIPv6LocalHost");
    if (!__SteamAPI_SteamNetworkingIPAddr_SetIPv6LocalHost_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIPAddr_SetIPv6LocalHost\n"); complete = false; }
    __SteamAPI_SteamNetworkingIPAddr_ToString_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIPAddr_ToString");
    if (!__SteamAPI_SteamNetworkingIPAddr_ToString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIPAddr_ToString\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_Clear_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_Clear");
    if (!__SteamAPI_SteamNetworkingIdentity_Clear_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_Clear\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_GetFakeIPType_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_GetFakeIPType");
    if (!__SteamAPI_SteamNetworkingIdentity_GetFakeIPType_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_GetFakeIPType\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_GetGenericBytes_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_GetGenericBytes");
    if (!__SteamAPI_SteamNetworkingIdentity_GetGenericBytes_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_GetGenericBytes\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_GetGenericString_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_GetGenericString");
    if (!__SteamAPI_SteamNetworkingIdentity_GetGenericString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_GetGenericString\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_GetIPAddr_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_GetIPAddr");
    if (!__SteamAPI_SteamNetworkingIdentity_GetIPAddr_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_GetIPAddr\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_GetIPv4_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_GetIPv4");
    if (!__SteamAPI_SteamNetworkingIdentity_GetIPv4_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_GetIPv4\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_GetPSNID_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_GetPSNID");
    if (!__SteamAPI_SteamNetworkingIdentity_GetPSNID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_GetPSNID\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_GetSteamID_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_GetSteamID");
    if (!__SteamAPI_SteamNetworkingIdentity_GetSteamID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_GetSteamID\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_GetSteamID64_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_GetSteamID64");
    if (!__SteamAPI_SteamNetworkingIdentity_GetSteamID64_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_GetSteamID64\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_GetXboxPairwiseID_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_GetXboxPairwiseID");
    if (!__SteamAPI_SteamNetworkingIdentity_GetXboxPairwiseID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_GetXboxPairwiseID\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_IsEqualTo_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_IsEqualTo");
    if (!__SteamAPI_SteamNetworkingIdentity_IsEqualTo_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_IsEqualTo\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_IsFakeIP_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_IsFakeIP");
    if (!__SteamAPI_SteamNetworkingIdentity_IsFakeIP_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_IsFakeIP\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_IsInvalid_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_IsInvalid");
    if (!__SteamAPI_SteamNetworkingIdentity_IsInvalid_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_IsInvalid\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_IsLocalHost_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_IsLocalHost");
    if (!__SteamAPI_SteamNetworkingIdentity_IsLocalHost_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_IsLocalHost\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_ParseString_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_ParseString");
    if (!__SteamAPI_SteamNetworkingIdentity_ParseString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_ParseString\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_SetGenericBytes_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_SetGenericBytes");
    if (!__SteamAPI_SteamNetworkingIdentity_SetGenericBytes_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_SetGenericBytes\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_SetGenericString_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_SetGenericString");
    if (!__SteamAPI_SteamNetworkingIdentity_SetGenericString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_SetGenericString\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_SetIPAddr_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_SetIPAddr");
    if (!__SteamAPI_SteamNetworkingIdentity_SetIPAddr_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_SetIPAddr\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_SetIPv4Addr_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_SetIPv4Addr");
    if (!__SteamAPI_SteamNetworkingIdentity_SetIPv4Addr_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_SetIPv4Addr\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_SetLocalHost_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_SetLocalHost");
    if (!__SteamAPI_SteamNetworkingIdentity_SetLocalHost_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_SetLocalHost\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_SetPSNID_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_SetPSNID");
    if (!__SteamAPI_SteamNetworkingIdentity_SetPSNID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_SetPSNID\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_SetSteamID_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_SetSteamID");
    if (!__SteamAPI_SteamNetworkingIdentity_SetSteamID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_SetSteamID\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_SetSteamID64_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_SetSteamID64");
    if (!__SteamAPI_SteamNetworkingIdentity_SetSteamID64_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_SetSteamID64\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_SetXboxPairwiseID_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_SetXboxPairwiseID");
    if (!__SteamAPI_SteamNetworkingIdentity_SetXboxPairwiseID_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_SetXboxPairwiseID\n"); complete = false; }
    __SteamAPI_SteamNetworkingIdentity_ToString_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingIdentity_ToString");
    if (!__SteamAPI_SteamNetworkingIdentity_ToString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingIdentity_ToString\n"); complete = false; }
    __SteamAPI_SteamNetworkingMessage_t_Release_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingMessage_t_Release");
    if (!__SteamAPI_SteamNetworkingMessage_t_Release_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingMessage_t_Release\n"); complete = false; }
    __SteamAPI_SteamNetworkingMessages_SteamAPI_v002_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingMessages_SteamAPI_v002");
    if (!__SteamAPI_SteamNetworkingMessages_SteamAPI_v002_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingMessages_SteamAPI_v002\n"); complete = false; }
    __SteamAPI_SteamNetworkingSockets_SteamAPI_v012_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingSockets_SteamAPI_v012");
    if (!__SteamAPI_SteamNetworkingSockets_SteamAPI_v012_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingSockets_SteamAPI_v012\n"); complete = false; }
    __SteamAPI_SteamNetworkingUtils_SteamAPI_v004_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworkingUtils_SteamAPI_v004");
    if (!__SteamAPI_SteamNetworkingUtils_SteamAPI_v004_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworkingUtils_SteamAPI_v004\n"); complete = false; }
    __SteamAPI_SteamNetworking_v006_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamNetworking_v006");
    if (!__SteamAPI_SteamNetworking_v006_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamNetworking_v006\n"); complete = false; }
    __SteamAPI_SteamParentalSettings_v001_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamParentalSettings_v001");
    if (!__SteamAPI_SteamParentalSettings_v001_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamParentalSettings_v001\n"); complete = false; }
    __SteamAPI_SteamParties_v002_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamParties_v002");
    if (!__SteamAPI_SteamParties_v002_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamParties_v002\n"); complete = false; }
    __SteamAPI_SteamRemotePlay_v003_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamRemotePlay_v003");
    if (!__SteamAPI_SteamRemotePlay_v003_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamRemotePlay_v003\n"); complete = false; }
    __SteamAPI_SteamRemoteStorage_v016_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamRemoteStorage_v016");
    if (!__SteamAPI_SteamRemoteStorage_v016_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamRemoteStorage_v016\n"); complete = false; }
    __SteamAPI_SteamScreenshots_v003_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamScreenshots_v003");
    if (!__SteamAPI_SteamScreenshots_v003_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamScreenshots_v003\n"); complete = false; }
    __SteamAPI_SteamTimeline_v004_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamTimeline_v004");
    if (!__SteamAPI_SteamTimeline_v004_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamTimeline_v004\n"); complete = false; }
    __SteamAPI_SteamUGC_v021_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamUGC_v021");
    if (!__SteamAPI_SteamUGC_v021_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamUGC_v021\n"); complete = false; }
    __SteamAPI_SteamUserStats_v013_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamUserStats_v013");
    if (!__SteamAPI_SteamUserStats_v013_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamUserStats_v013\n"); complete = false; }
    __SteamAPI_SteamUser_v023_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamUser_v023");
    if (!__SteamAPI_SteamUser_v023_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamUser_v023\n"); complete = false; }
    __SteamAPI_SteamUtils_v010_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamUtils_v010");
    if (!__SteamAPI_SteamUtils_v010_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamUtils_v010\n"); complete = false; }
    __SteamAPI_SteamVideo_v007_ptr = (void*)GetProcAddress(original, "SteamAPI_SteamVideo_v007");
    if (!__SteamAPI_SteamVideo_v007_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_SteamVideo_v007\n"); complete = false; }
    __SteamAPI_UnregisterCallResult_ptr = (void*)GetProcAddress(original, "SteamAPI_UnregisterCallResult");
    if (!__SteamAPI_UnregisterCallResult_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_UnregisterCallResult\n"); complete = false; }
    __SteamAPI_UnregisterCallback_ptr = (void*)GetProcAddress(original, "SteamAPI_UnregisterCallback");
    if (!__SteamAPI_UnregisterCallback_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_UnregisterCallback\n"); complete = false; }
    __SteamAPI_UseBreakpadCrashHandler_ptr = (void*)GetProcAddress(original, "SteamAPI_UseBreakpadCrashHandler");
    if (!__SteamAPI_UseBreakpadCrashHandler_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_UseBreakpadCrashHandler\n"); complete = false; }
    __SteamAPI_WriteMiniDump_ptr = (void*)GetProcAddress(original, "SteamAPI_WriteMiniDump");
    if (!__SteamAPI_WriteMiniDump_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_WriteMiniDump\n"); complete = false; }
    __SteamAPI_gameserveritem_t_Construct_ptr = (void*)GetProcAddress(original, "SteamAPI_gameserveritem_t_Construct");
    if (!__SteamAPI_gameserveritem_t_Construct_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_gameserveritem_t_Construct\n"); complete = false; }
    __SteamAPI_gameserveritem_t_GetName_ptr = (void*)GetProcAddress(original, "SteamAPI_gameserveritem_t_GetName");
    if (!__SteamAPI_gameserveritem_t_GetName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_gameserveritem_t_GetName\n"); complete = false; }
    __SteamAPI_gameserveritem_t_SetName_ptr = (void*)GetProcAddress(original, "SteamAPI_gameserveritem_t_SetName");
    if (!__SteamAPI_gameserveritem_t_SetName_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_gameserveritem_t_SetName\n"); complete = false; }
    __SteamAPI_servernetadr_t_Assign_ptr = (void*)GetProcAddress(original, "SteamAPI_servernetadr_t_Assign");
    if (!__SteamAPI_servernetadr_t_Assign_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_servernetadr_t_Assign\n"); complete = false; }
    __SteamAPI_servernetadr_t_Construct_ptr = (void*)GetProcAddress(original, "SteamAPI_servernetadr_t_Construct");
    if (!__SteamAPI_servernetadr_t_Construct_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_servernetadr_t_Construct\n"); complete = false; }
    __SteamAPI_servernetadr_t_GetConnectionAddressString_ptr = (void*)GetProcAddress(original, "SteamAPI_servernetadr_t_GetConnectionAddressString");
    if (!__SteamAPI_servernetadr_t_GetConnectionAddressString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_servernetadr_t_GetConnectionAddressString\n"); complete = false; }
    __SteamAPI_servernetadr_t_GetConnectionPort_ptr = (void*)GetProcAddress(original, "SteamAPI_servernetadr_t_GetConnectionPort");
    if (!__SteamAPI_servernetadr_t_GetConnectionPort_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_servernetadr_t_GetConnectionPort\n"); complete = false; }
    __SteamAPI_servernetadr_t_GetIP_ptr = (void*)GetProcAddress(original, "SteamAPI_servernetadr_t_GetIP");
    if (!__SteamAPI_servernetadr_t_GetIP_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_servernetadr_t_GetIP\n"); complete = false; }
    __SteamAPI_servernetadr_t_GetQueryAddressString_ptr = (void*)GetProcAddress(original, "SteamAPI_servernetadr_t_GetQueryAddressString");
    if (!__SteamAPI_servernetadr_t_GetQueryAddressString_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_servernetadr_t_GetQueryAddressString\n"); complete = false; }
    __SteamAPI_servernetadr_t_GetQueryPort_ptr = (void*)GetProcAddress(original, "SteamAPI_servernetadr_t_GetQueryPort");
    if (!__SteamAPI_servernetadr_t_GetQueryPort_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_servernetadr_t_GetQueryPort\n"); complete = false; }
    __SteamAPI_servernetadr_t_Init_ptr = (void*)GetProcAddress(original, "SteamAPI_servernetadr_t_Init");
    if (!__SteamAPI_servernetadr_t_Init_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_servernetadr_t_Init\n"); complete = false; }
    __SteamAPI_servernetadr_t_IsLessThan_ptr = (void*)GetProcAddress(original, "SteamAPI_servernetadr_t_IsLessThan");
    if (!__SteamAPI_servernetadr_t_IsLessThan_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_servernetadr_t_IsLessThan\n"); complete = false; }
    __SteamAPI_servernetadr_t_SetConnectionPort_ptr = (void*)GetProcAddress(original, "SteamAPI_servernetadr_t_SetConnectionPort");
    if (!__SteamAPI_servernetadr_t_SetConnectionPort_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_servernetadr_t_SetConnectionPort\n"); complete = false; }
    __SteamAPI_servernetadr_t_SetIP_ptr = (void*)GetProcAddress(original, "SteamAPI_servernetadr_t_SetIP");
    if (!__SteamAPI_servernetadr_t_SetIP_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_servernetadr_t_SetIP\n"); complete = false; }
    __SteamAPI_servernetadr_t_SetQueryPort_ptr = (void*)GetProcAddress(original, "SteamAPI_servernetadr_t_SetQueryPort");
    if (!__SteamAPI_servernetadr_t_SetQueryPort_ptr) { fprintf(stderr, "error: missing Steam export SteamAPI_servernetadr_t_SetQueryPort\n"); complete = false; }
    __SteamClient_ptr = (void*)GetProcAddress(original, "SteamClient");
    if (!__SteamClient_ptr) { fprintf(stderr, "error: missing Steam export SteamClient\n"); complete = false; }
    __SteamGameServer_BSecure_ptr = (void*)GetProcAddress(original, "SteamGameServer_BSecure");
    if (!__SteamGameServer_BSecure_ptr) { fprintf(stderr, "error: missing Steam export SteamGameServer_BSecure\n"); complete = false; }
    __SteamGameServer_GetHSteamPipe_ptr = (void*)GetProcAddress(original, "SteamGameServer_GetHSteamPipe");
    if (!__SteamGameServer_GetHSteamPipe_ptr) { fprintf(stderr, "error: missing Steam export SteamGameServer_GetHSteamPipe\n"); complete = false; }
    __SteamGameServer_GetHSteamUser_ptr = (void*)GetProcAddress(original, "SteamGameServer_GetHSteamUser");
    if (!__SteamGameServer_GetHSteamUser_ptr) { fprintf(stderr, "error: missing Steam export SteamGameServer_GetHSteamUser\n"); complete = false; }
    __SteamGameServer_GetIPCCallCount_ptr = (void*)GetProcAddress(original, "SteamGameServer_GetIPCCallCount");
    if (!__SteamGameServer_GetIPCCallCount_ptr) { fprintf(stderr, "error: missing Steam export SteamGameServer_GetIPCCallCount\n"); complete = false; }
    __SteamGameServer_GetSteamID_ptr = (void*)GetProcAddress(original, "SteamGameServer_GetSteamID");
    if (!__SteamGameServer_GetSteamID_ptr) { fprintf(stderr, "error: missing Steam export SteamGameServer_GetSteamID\n"); complete = false; }
    __SteamGameServer_InitSafe_ptr = (void*)GetProcAddress(original, "SteamGameServer_InitSafe");
    if (!__SteamGameServer_InitSafe_ptr) { fprintf(stderr, "error: missing Steam export SteamGameServer_InitSafe\n"); complete = false; }
    __SteamGameServer_RunCallbacks_ptr = (void*)GetProcAddress(original, "SteamGameServer_RunCallbacks");
    if (!__SteamGameServer_RunCallbacks_ptr) { fprintf(stderr, "error: missing Steam export SteamGameServer_RunCallbacks\n"); complete = false; }
    __SteamGameServer_Shutdown_ptr = (void*)GetProcAddress(original, "SteamGameServer_Shutdown");
    if (!__SteamGameServer_Shutdown_ptr) { fprintf(stderr, "error: missing Steam export SteamGameServer_Shutdown\n"); complete = false; }
    __SteamInternal_ContextInit_ptr = (void*)GetProcAddress(original, "SteamInternal_ContextInit");
    if (!__SteamInternal_ContextInit_ptr) { fprintf(stderr, "error: missing Steam export SteamInternal_ContextInit\n"); complete = false; }
    __SteamInternal_CreateInterface_ptr = (void*)GetProcAddress(original, "SteamInternal_CreateInterface");
    if (!__SteamInternal_CreateInterface_ptr) { fprintf(stderr, "error: missing Steam export SteamInternal_CreateInterface\n"); complete = false; }
    __SteamInternal_FindOrCreateGameServerInterface_ptr = (void*)GetProcAddress(original, "SteamInternal_FindOrCreateGameServerInterface");
    if (!__SteamInternal_FindOrCreateGameServerInterface_ptr) { fprintf(stderr, "error: missing Steam export SteamInternal_FindOrCreateGameServerInterface\n"); complete = false; }
    __SteamInternal_FindOrCreateUserInterface_ptr = (void*)GetProcAddress(original, "SteamInternal_FindOrCreateUserInterface");
    if (!__SteamInternal_FindOrCreateUserInterface_ptr) { fprintf(stderr, "error: missing Steam export SteamInternal_FindOrCreateUserInterface\n"); complete = false; }
    __SteamInternal_GameServer_Init_V2_ptr = (void*)GetProcAddress(original, "SteamInternal_GameServer_Init_V2");
    if (!__SteamInternal_GameServer_Init_V2_ptr) { fprintf(stderr, "error: missing Steam export SteamInternal_GameServer_Init_V2\n"); complete = false; }
    __SteamInternal_SteamAPI_Init_ptr = (void*)GetProcAddress(original, "SteamInternal_SteamAPI_Init");
    if (!__SteamInternal_SteamAPI_Init_ptr) { fprintf(stderr, "error: missing Steam export SteamInternal_SteamAPI_Init\n"); complete = false; }
    __g_pSteamClientGameServer_ptr = (void*)GetProcAddress(original, "g_pSteamClientGameServer");
    if (!__g_pSteamClientGameServer_ptr) { fprintf(stderr, "error: missing Steam export g_pSteamClientGameServer\n"); complete = false; }
    return complete;
}