/* weak_stubs.c — C-linkage stubs for linking (compiled as C, no C++ issues) */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Dummy no-op stub for all C-linkage functions */
void AudioChannel_Pause(void) {}
void AudioChannel_Play(void) {}
void ButtonSprite_Ctor(void) {}
void CGWND_SetMode(void) {}
void Config_GetIniString(void) {}
void DDRAW_RestoreSurfaces(void) {}
void DDRAW_SetSurfaceFormat(void) {}
void DDRAW_UnlockPrimary(void) {}
void FormatResourceString(void) {}
void PlaySound(void) {}
void RESDATA_IsBuildingTile(void) {}
void RESDATA_IsRoadTile(void) {}
void RESDATA_Lock(void) {}
void RESDATA_Unlock(void) {}
void RESMGR_LoadSoundResource(void) {}
void RESMGR_ReleaseSoundResource(void) {}
void ResourceManager_GetById(void) {}
void Sprite_Init(void) {}
void Sprite_SetState(void) {}
void TileMap_InvalidateRect(void) {}
void UI_CreateChildWindow(void) {}
void UIEntity_Ctor(void) {}
void UIPANEL_BeginPaint(void) {}
void UIPANEL_Blit(void) {}
void UIPANEL_CreateSurface(void) {}
void UIPANEL_EndPaintEx(void) {}
void UI_WindowBase_BaseDtor(void) {}
void UI_WindowBase_Ctor(void) {}
void VehicleEditor_CheckEditBounds1(void) {}
void WIN32_StreamOpen(void) {}
void WIN32_StreamOpenFile(void) {}
void WNDPROC_EnterCriticalSection(void) {}
void WNDPROC_LeaveCriticalSection(void) {}
void WNDPROC_StreamFromMemory(void) {}

/* Globals */
void* g_active_panel = 0;
void* g_allow_building_placement = 0;
void* g_asset_archive = 0;
void* g_asset_base_path = 0;
void* g_asset_mgr = 0;
void* g_audio = 0;
void* g_audio_mgr = 0;
void* g_backbuffer = 0;
void* g_building_mgr = 0;
void* g_build_mode = 0;
void* g_cgwnd = 0;
void* g_clean_exit = 0;
void* g_click_on_building = 0;
void* g_click_on_town = 0;
void* g_client_height = 0;
void* g_client_offset_x = 0;
void* g_client_offset_y = 0;
void* g_client_rect = 0;
void* g_client_width = 0;
void* g_config_ini = 0;
void* g_cursor = 0;
void* g_cursor_back = 0;
void* g_cursor_refcount = 0;
void* g_cursor_surface = 0;
void* g_cursor_world_x = 0;
void* g_cursor_world_y = 0;
void* g_ddraw = 0;
void* g_ddraw_active = 0;
void* g_ddraw_building = 0;
void* g_demo_mode = 0;
void* g_disable_input = 0;
void* g_dplay = 0;
void* g_dplay_config = 0;
void* g_dplay_peer = 0;
void* g_drag_start_x = 0;
void* g_drag_start_y = 0;
void* g_dsound_object = 0;
void* g_empty_string = 0;
void* g_font_normal = 0;
void* g_font_small = 0;
void* g_frame_event = 0;
void* g_fullscreen_rect = 0;
void* g_game = 0;
void* g_game_config = 0;
void* g_game_difficulty = 0;
void* g_game_mode = 0;
void* g_game_time = 0;
void* g_in_build_mode = 0;
void* g_input_mgr = 0;
void* g_install_path = 0;
void* g_IntersectRect = 0;
void* g_is_fullscreen = 0;
void* g_IsRectEmpty = 0;
void* g_is_town_mode = 0;
void* g_last_cursor_pos = 0;
void* g_listener_x = 0;
void* g_listener_y = 0;
void* g_main_window = 0;
void* g_nameEntryPanel = 0;
void* g_netman = 0;
void* g_netSettings = 0;
void* g_network_queue = 0;
void* g_network_thread = 0;
void* g_object_count = 0;
void* g_OffsetRect = 0;
void* g_OutputDebugStringA = 0;
void* g_pixel_cache = 0;
void* g_pixel_data_cache = 0;
void* g_pixel_format_mask = 0;
void* g_placement_resource_id = 0;
void* g_player_config = 0;
void* g_player_id = 0;
void* g_postcard = 0;
void* g_postcard_send = 0;
void* g_primary_surface = 0;
void* g_PtInRect = 0;
void* g_ref_count = 0;
void* g_resmgr = 0;
void* g_resource_mgr = 0;
void* g_road_build_mode = 0;
void* g_screen_bpp = 0;
void* g_screen_center_x = 0;
void* g_screen_center_y = 0;
void* g_screen_height = 0;
void* g_screen_width = 0;
void* g_script_engine = 0;
void* g_selected_building = 0;
void* g_show_scrollbars = 0;
void* g_sound_cache = 0;
void* g_surface_bpp = 0;
void* g_surface_bshift = 0;
void* g_surface_channel1 = 0;
void* g_surface_channel2 = 0;
void* g_surface_lost = 0;
void* g_tilemap = 0;
void* g_tile_occupied_bitmap = 0;
void* g_timer_event_id = 0;
void* g_timer_id = 0;
void* g_title_font = 0;
void* g_tooltip_mgr = 0;
void* g_town = 0;
void* g_town_mode = 0;
void* g_town_view = 0;
void* g_trackSegmentOffsets = 0;
void* g_train = 0;
void* g_train_resources = 0;
void* g_trainstation_window = 0;
void* g_ui_main = 0;
void* g_viewport_rect_bottom = 0;
void* g_viewport_rect_left = 0;
void* g_viewport_rect_right = 0;
void* g_viewport_rect_top = 0;
void* g_viewport_x = 0;
void* g_viewport_y = 0;
void* g_window_bottom = 0;
void* g_window_left = 0;
void* g_window_mode = 0;
void* g_window_right = 0;
void* g_window_top = 0;
void* g_world = 0;
void* g_world_height = 0;
void* g_world_width = 0;
void* DAT_0047e0f4 = 0;
void* DAT_0047e220 = 0;
void* DAT_0047e224 = 0;
void* _DAT_00481170 = 0;
void* DAT_00481170 = 0;
void* DAT_00481218 = 0;
void* DAT_00485268 = 0;
void* DAT_0048526c = 0;
void* DAT_004a97a0 = 0;
void* DAT_004a9994 = 0;
void* DAT_004aad34 = 0;
void* DAT_004aad38 = 0;
void* _DAT_004fd3a8 = 0;
void* ATTR_0047f108 = 0;
void* s_AW_Blit_failure_reported_0047e0d8 = 0;
void* s_BALANCING_0047e164 = 0;
void* s_CleanExit_0047e128 = 0;
void* s_LEGO_LOCO_0047e1c0 = 0;
void* s_measure_test_char = 0;
void* s_MinBuildingFPS_0047e154 = 0;
void* s_MinFlyingFPS_0047e134 = 0;
void* s_MinMinifigFPS_0047e144 = 0;
void* s_MinVehicleFPS_0047e170 = 0;
void* s_PROCESS_0047e120 = 0;
void* s_RectBottom_0047e180 = 0;
void* s_RectLeft_0047e1b4 = 0;
void* s_RectRight_0047e18c = 0;
void* s_RectTop_0047e198 = 0;
void* s_StringFileInfo_080904B0_FileVer_0047e0f8 = 0;
void* s_WINDOW_ATTRIBUTES_0047e1a0 = 0;
void* __imp_SystemParametersInfoA = 0;

void* _g_cursor_back = 0;
int   _g_cursor_refcount = 0;
