/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2017 - Brad Parker
 *  Copyright (C) 2018      - Alfredo Monclús
 *  Copyright (C) 2018      - natinusala
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <file/file_path.h>
#include <string/stdstring.h>
#include <encodings/utf.h>
#include <streams/file_stream.h>
#include <features/features_cpu.h>
#include <formats/image.h>

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu_animation.h"
#include "../menu_input.h"

#include "../widgets/menu_input_dialog.h"
#include "../widgets/menu_osk.h"

#include "../../configuration.h"
#include "../../cheevos/badges.h"
#include "../../content.h"
#include "../../core_info.h"
#include "../../core.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../tasks/tasks_internal.h"

#define FONT_SIZE_FOOTER 18
#define FONT_SIZE_TITLE 36
#define FONT_SIZE_TIME 22
#define FONT_SIZE_ENTRIES_LABEL 24
#define FONT_SIZE_ENTRIES_SUBLABEL 18
#define FONT_SIZE_SIDEBAR 24

#define ANIMATION_PUSH_ENTRY_DURATION 10
#define ANIMATION_CURSOR_DURATION 8
#define ANIMATION_CURSOR_PULSE 30

#define ENTRIES_START_Y 127

#define INTERVAL_BATTERY_LEVEL_CHECK (30 * 1000000)
#define INTERVAL_OSK_CURSOR (0.5f * 1000000)

static float ozone_pure_white[16] = {
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
};

static float ozone_backdrop[16] = {
      0.00, 0.00, 0.00, 0.75,
      0.00, 0.00, 0.00, 0.75,
      0.00, 0.00, 0.00, 0.75,
      0.00, 0.00, 0.00, 0.75,
};

static float ozone_osk_backdrop[16] = {
      0.00, 0.00, 0.00, 0.15,
      0.00, 0.00, 0.00, 0.15,
      0.00, 0.00, 0.00, 0.15,
      0.00, 0.00, 0.00, 0.15,
};

enum OZONE_TEXTURE {
   OZONE_TEXTURE_RETROARCH = 0,
   OZONE_TEXTURE_CURSOR_BORDER,

   OZONE_TEXTURE_LAST
};

static char *OZONE_TEXTURES_FILES[OZONE_TEXTURE_LAST] = {
   "retroarch",
   "cursor_border"
};

enum OZONE_THEME_TEXTURES {
   OZONE_THEME_TEXTURE_BUTTON_A = 0,
   OZONE_THEME_TEXTURE_BUTTON_B,
   OZONE_THEME_TEXTURE_SWITCH,
   OZONE_THEME_TEXTURE_CHECK,

   OZONE_THEME_TEXTURE_CURSOR_NO_BORDER,
   OZONE_THEME_TEXTURE_CURSOR_STATIC,

   OZONE_THEME_TEXTURE_LAST
};

static char *OZONE_THEME_TEXTURES_FILES[OZONE_THEME_TEXTURE_LAST] = {
   "button_a",
   "button_b",
   "switch",
   "check",
   "cursor_noborder",
   "cursor_static"
};

enum OZONE_TAB_TEXTURES {
   OZONE_TAB_TEXTURE_MAIN_MENU = 0,
   OZONE_TAB_TEXTURE_SETTINGS,
   OZONE_TAB_TEXTURE_HISTORY,
   OZONE_TAB_TEXTURE_FAVORITES,
   OZONE_TAB_TEXTURE_MUSIC,
   OZONE_TAB_TEXTURE_VIDEO,
   OZONE_TAB_TEXTURE_IMAGE,
   OZONE_TAB_TEXTURE_NETWORK,
   OZONE_TAB_TEXTURE_SCAN_CONTENT,

   OZONE_TAB_TEXTURE_LAST
};

static char *OZONE_TAB_TEXTURES_FILES[OZONE_TAB_TEXTURE_LAST] = {
   "retroarch",
   "settings",
   "history",
   "favorites",
   "music",
   "video",
   "image",
   "netplay",
   "add"
};

enum
{
   OZONE_SYSTEM_TAB_MAIN = 0,
   OZONE_SYSTEM_TAB_SETTINGS,
   OZONE_SYSTEM_TAB_HISTORY,
   OZONE_SYSTEM_TAB_FAVORITES,
   OZONE_SYSTEM_TAB_MUSIC,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   OZONE_SYSTEM_TAB_VIDEO,
#endif
#ifdef HAVE_IMAGEVIEWER
   OZONE_SYSTEM_TAB_IMAGES,
#endif
#ifdef HAVE_NETWORKING
   OZONE_SYSTEM_TAB_NETPLAY,
#endif
   OZONE_SYSTEM_TAB_ADD,

   /* End of this enum - use the last one to determine num of possible tabs */
   OZONE_SYSTEM_TAB_LAST
};

static enum msg_hash_enums ozone_system_tabs_value[OZONE_SYSTEM_TAB_LAST] = {
   MENU_ENUM_LABEL_VALUE_MAIN_MENU,
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
#endif
#ifdef HAVE_IMAGEVIEWER
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
#endif
#ifdef HAVE_NETWORKING
   MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
#endif
   MENU_ENUM_LABEL_VALUE_ADD_TAB
};

static enum menu_settings_type ozone_system_tabs_type[OZONE_SYSTEM_TAB_LAST] = {
   MENU_SETTINGS,
   MENU_SETTINGS_TAB,
   MENU_HISTORY_TAB,
   MENU_FAVORITES_TAB,
   MENU_MUSIC_TAB,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   MENU_VIDEO_TAB,
#endif
#ifdef HAVE_IMAGEVIEWER
   MENU_IMAGES_TAB,
#endif
#ifdef HAVE_NETWORKING
   MENU_NETPLAY_TAB,
#endif
   MENU_ADD_TAB
};

static enum msg_hash_enums ozone_system_tabs_idx[OZONE_SYSTEM_TAB_LAST] = {
   MENU_ENUM_LABEL_MAIN_MENU,
   MENU_ENUM_LABEL_SETTINGS_TAB,
   MENU_ENUM_LABEL_HISTORY_TAB,
   MENU_ENUM_LABEL_FAVORITES_TAB,
   MENU_ENUM_LABEL_MUSIC_TAB,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   MENU_ENUM_LABEL_VIDEO_TAB,
#endif
#ifdef HAVE_IMAGEVIEWER
   MENU_ENUM_LABEL_IMAGES_TAB,
#endif
#ifdef HAVE_NETWORKING
   MENU_ENUM_LABEL_NETPLAY_TAB,
#endif
   MENU_ENUM_LABEL_ADD_TAB
};

static unsigned ozone_system_tabs_icons[OZONE_SYSTEM_TAB_LAST] = {
   OZONE_TAB_TEXTURE_MAIN_MENU,
   OZONE_TAB_TEXTURE_SETTINGS,
   OZONE_TAB_TEXTURE_HISTORY,
   OZONE_TAB_TEXTURE_FAVORITES,
   OZONE_TAB_TEXTURE_MUSIC,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   OZONE_TAB_TEXTURE_VIDEO,
#endif
#ifdef HAVE_IMAGEVIEWER
   OZONE_TAB_TEXTURE_IMAGE,
#endif
#ifdef HAVE_NETWORKING
   OZONE_TAB_TEXTURE_NETWORK,
#endif
   OZONE_TAB_TEXTURE_SCAN_CONTENT
};

enum
{
   OZONE_ENTRIES_ICONS_TEXTURE_MAIN_MENU = 0,
   OZONE_ENTRIES_ICONS_TEXTURE_SETTINGS,
   OZONE_ENTRIES_ICONS_TEXTURE_HISTORY,
   OZONE_ENTRIES_ICONS_TEXTURE_FAVORITES,
   OZONE_ENTRIES_ICONS_TEXTURE_MUSICS,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   OZONE_ENTRIES_ICONS_TEXTURE_MOVIES,
#endif
#ifdef HAVE_NETWORKING
   OZONE_ENTRIES_ICONS_TEXTURE_NETPLAY,
   OZONE_ENTRIES_ICONS_TEXTURE_ROOM,
   OZONE_ENTRIES_ICONS_TEXTURE_ROOM_LAN,
   OZONE_ENTRIES_ICONS_TEXTURE_ROOM_RELAY,
#endif
#ifdef HAVE_IMAGEVIEWER
   OZONE_ENTRIES_ICONS_TEXTURE_IMAGES,
#endif
   OZONE_ENTRIES_ICONS_TEXTURE_SETTING,
   OZONE_ENTRIES_ICONS_TEXTURE_SUBSETTING,
   OZONE_ENTRIES_ICONS_TEXTURE_ARROW,
   OZONE_ENTRIES_ICONS_TEXTURE_RUN,
   OZONE_ENTRIES_ICONS_TEXTURE_CLOSE,
   OZONE_ENTRIES_ICONS_TEXTURE_RESUME,
   OZONE_ENTRIES_ICONS_TEXTURE_SAVESTATE,
   OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE,
   OZONE_ENTRIES_ICONS_TEXTURE_UNDO,
   OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO,
   OZONE_ENTRIES_ICONS_TEXTURE_WIFI,
   OZONE_ENTRIES_ICONS_TEXTURE_CORE_OPTIONS,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_REMAPPING_OPTIONS,
   OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS,
   OZONE_ENTRIES_ICONS_TEXTURE_DISK_OPTIONS,
   OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS,
   OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENT_LIST,
   OZONE_ENTRIES_ICONS_TEXTURE_SCREENSHOT,
   OZONE_ENTRIES_ICONS_TEXTURE_RELOAD,
   OZONE_ENTRIES_ICONS_TEXTURE_RENAME,
   OZONE_ENTRIES_ICONS_TEXTURE_FILE,
   OZONE_ENTRIES_ICONS_TEXTURE_FOLDER,
   OZONE_ENTRIES_ICONS_TEXTURE_ZIP,
   OZONE_ENTRIES_ICONS_TEXTURE_FAVORITE,
   OZONE_ENTRIES_ICONS_TEXTURE_ADD_FAVORITE,
   OZONE_ENTRIES_ICONS_TEXTURE_MUSIC,
   OZONE_ENTRIES_ICONS_TEXTURE_IMAGE,
   OZONE_ENTRIES_ICONS_TEXTURE_MOVIE,
   OZONE_ENTRIES_ICONS_TEXTURE_CORE,
   OZONE_ENTRIES_ICONS_TEXTURE_RDB,
   OZONE_ENTRIES_ICONS_TEXTURE_CURSOR,
   OZONE_ENTRIES_ICONS_TEXTURE_SWITCH_ON,
   OZONE_ENTRIES_ICONS_TEXTURE_SWITCH_OFF,
   OZONE_ENTRIES_ICONS_TEXTURE_CLOCK,
   OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_FULL,
   OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_CHARGING,
   OZONE_ENTRIES_ICONS_TEXTURE_POINTER,
   OZONE_ENTRIES_ICONS_TEXTURE_ADD,
   OZONE_ENTRIES_ICONS_TEXTURE_KEY,
   OZONE_ENTRIES_ICONS_TEXTURE_KEY_HOVER,
   OZONE_ENTRIES_ICONS_TEXTURE_DIALOG_SLICE,
   OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENTS,
   OZONE_ENTRIES_ICONS_TEXTURE_AUDIO,
   OZONE_ENTRIES_ICONS_TEXTURE_EXIT,
   OZONE_ENTRIES_ICONS_TEXTURE_FRAMESKIP,
   OZONE_ENTRIES_ICONS_TEXTURE_INFO,
   OZONE_ENTRIES_ICONS_TEXTURE_HELP,
   OZONE_ENTRIES_ICONS_TEXTURE_NETWORK,
   OZONE_ENTRIES_ICONS_TEXTURE_POWER,
   OZONE_ENTRIES_ICONS_TEXTURE_SAVING,
   OZONE_ENTRIES_ICONS_TEXTURE_UPDATER,
   OZONE_ENTRIES_ICONS_TEXTURE_VIDEO,
   OZONE_ENTRIES_ICONS_TEXTURE_RECORD,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS,
   OZONE_ENTRIES_ICONS_TEXTURE_MIXER,
   OZONE_ENTRIES_ICONS_TEXTURE_LOG,
   OZONE_ENTRIES_ICONS_TEXTURE_OSD,
   OZONE_ENTRIES_ICONS_TEXTURE_UI,
   OZONE_ENTRIES_ICONS_TEXTURE_USER,
   OZONE_ENTRIES_ICONS_TEXTURE_PRIVACY,
   OZONE_ENTRIES_ICONS_TEXTURE_LATENCY,
   OZONE_ENTRIES_ICONS_TEXTURE_DRIVERS,
   OZONE_ENTRIES_ICONS_TEXTURE_PLAYLIST,
   OZONE_ENTRIES_ICONS_TEXTURE_QUICKMENU,
   OZONE_ENTRIES_ICONS_TEXTURE_REWIND,
   OZONE_ENTRIES_ICONS_TEXTURE_OVERLAY,
   OZONE_ENTRIES_ICONS_TEXTURE_OVERRIDE,
   OZONE_ENTRIES_ICONS_TEXTURE_NOTIFICATIONS,
   OZONE_ENTRIES_ICONS_TEXTURE_STREAM,
   OZONE_ENTRIES_ICONS_TEXTURE_SHUTDOWN,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_U,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_D,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_L,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_R,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_U,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_D,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_L,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_R,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_P,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SELECT,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_START,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_U,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_L,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LB,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RB,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LT,
   OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RT,
   OZONE_ENTRIES_ICONS_TEXTURE_CHECKMARK,
   OZONE_ENTRIES_ICONS_TEXTURE_LAST
};

#define HEX_R(hex) ((hex >> 16) & 0xFF) * (1.0f / 255.0f)
#define HEX_G(hex) ((hex >> 8 ) & 0xFF) * (1.0f / 255.0f)
#define HEX_B(hex) ((hex >> 0 ) & 0xFF) * (1.0f / 255.0f)

#define COLOR_HEX_TO_FLOAT(hex, alpha) { \
   HEX_R(hex), HEX_G(hex), HEX_B(hex), alpha, \
   HEX_R(hex), HEX_G(hex), HEX_B(hex), alpha, \
   HEX_R(hex), HEX_G(hex), HEX_B(hex), alpha, \
   HEX_R(hex), HEX_G(hex), HEX_B(hex), alpha  \
}

#define COLOR_TEXT_ALPHA(color, alpha) (color & 0xFFFFFF00) | alpha

static float ozone_sidebar_background_light[16] = {
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
};

static float ozone_sidebar_gradient_top_light[16] = {
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
      0.922, 0.922, 0.922, 1.00,
      0.922, 0.922, 0.922, 1.00,
};

static float ozone_sidebar_gradient_bottom_light[16] = {
      0.922, 0.922, 0.922, 1.00,
      0.922, 0.922, 0.922, 1.00,
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
};

static float ozone_sidebar_background_dark[16] = {
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
};

static float ozone_sidebar_gradient_top_dark[16] = {
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,      
      0.18, 0.18, 0.18, 1.00,
      0.18, 0.18, 0.18, 1.00,
};

static float ozone_sidebar_gradient_bottom_dark[16] = {
      0.18, 0.18, 0.18, 1.00,
      0.18, 0.18, 0.18, 1.00,
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
};

static float ozone_border_0_light[16] = COLOR_HEX_TO_FLOAT(0x50EFD9, 1.00);
static float ozone_border_1_light[16] = COLOR_HEX_TO_FLOAT(0x0DB6D5, 1.00);

static float ozone_border_0_dark[16] = COLOR_HEX_TO_FLOAT(0x198AC6, 1.00);
static float ozone_border_1_dark[16] = COLOR_HEX_TO_FLOAT(0x89F1F2, 1.00);

typedef struct ozone_theme
{
   /* Background color */
   float background[16];

   /* Float colors for quads and icons */
   float header_footer_separator[16];
   float text[16];
   float selection[16];
   float selection_border[16];
   float entries_border[16];
   float entries_icon[16];
   float text_selected[16];
   float message_background[16];

   /* RGBA colors for text */
   uint32_t text_rgba;
   uint32_t text_selected_rgba;
   uint32_t text_sublabel_rgba;

   /* Sidebar color */
   float *sidebar_background;
   float *sidebar_top_gradient;
   float *sidebar_bottom_gradient;

   /* 
      Fancy cursor colors
   */
   float *cursor_border_0;
   float *cursor_border_1;

   menu_texture_item textures[OZONE_THEME_TEXTURE_LAST];

   const char *name;
} ozone_theme_t;

ozone_theme_t ozone_theme_light = {
   COLOR_HEX_TO_FLOAT(0xEBEBEB, 1.00),

   COLOR_HEX_TO_FLOAT(0x2B2B2B, 1.00),
   COLOR_HEX_TO_FLOAT(0x333333, 1.00),
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00),
   COLOR_HEX_TO_FLOAT(0x10BEC5, 1.00),
   COLOR_HEX_TO_FLOAT(0xCDCDCD, 1.00),
   COLOR_HEX_TO_FLOAT(0x333333, 1.00),
   COLOR_HEX_TO_FLOAT(0x374CFF, 1.00),
   COLOR_HEX_TO_FLOAT(0xF0F0F0, 1.00),

   0x333333FF,
   0x374CFFFF,
   0x878787FF,

   ozone_sidebar_background_light,
   ozone_sidebar_gradient_top_light,
   ozone_sidebar_gradient_bottom_light,

   ozone_border_0_light,
   ozone_border_1_light,

   {0},

   "light"
};

ozone_theme_t ozone_theme_dark = {
   COLOR_HEX_TO_FLOAT(0x2D2D2D, 1.00),

   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00),
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00),
   COLOR_HEX_TO_FLOAT(0x212227, 1.00),
   COLOR_HEX_TO_FLOAT(0x2DA3CB, 1.00),
   COLOR_HEX_TO_FLOAT(0x51514F, 1.00),
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00),
   COLOR_HEX_TO_FLOAT(0x00D9AE, 1.00),
   COLOR_HEX_TO_FLOAT(0x464646, 1.00),

   0xFFFFFFFF,
   0x00FFC5FF,
   0x9F9FA1FF,

   ozone_sidebar_background_dark,
   ozone_sidebar_gradient_top_dark,
   ozone_sidebar_gradient_bottom_dark,

   ozone_border_0_dark,
   ozone_border_1_dark,

   {0},

   "dark"
};

ozone_theme_t *ozone_themes[] = {
   &ozone_theme_light,
   &ozone_theme_dark
};

static unsigned ozone_themes_count                 = sizeof(ozone_themes) / sizeof(ozone_themes[0]);
static unsigned last_color_theme                   = 0;
static bool last_use_preferred_system_color_theme  = false;
static ozone_theme_t *ozone_default_theme          = &ozone_theme_light; /* also used as a tag for cursor animation */

typedef struct ozone_handle
{
   uint64_t frame_count;

   struct 
   {
      font_data_t *footer;
      font_data_t *title;
      font_data_t *time;
      font_data_t *entries_label;
      font_data_t *entries_sublabel;
      font_data_t *sidebar;
   } fonts;

   struct
   {
      video_font_raster_block_t footer;
      video_font_raster_block_t title;
      video_font_raster_block_t time;
      video_font_raster_block_t entries_label;
      video_font_raster_block_t entries_sublabel;
      video_font_raster_block_t sidebar;
   } raster_blocks;

   menu_texture_item textures[OZONE_THEME_TEXTURE_LAST];
   menu_texture_item icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LAST];
   menu_texture_item tab_textures[OZONE_TAB_TEXTURE_LAST];

   char title[PATH_MAX_LENGTH];

   char assets_path[PATH_MAX_LENGTH];
   char png_path[PATH_MAX_LENGTH];
   char icons_path[PATH_MAX_LENGTH];
   char tab_path[PATH_MAX_LENGTH];

   uint8_t system_tab_end;
   uint8_t tabs[OZONE_SYSTEM_TAB_LAST];

   size_t categories_selection_ptr; /* active tab id  */
   size_t categories_active_idx_old;

   bool cursor_in_sidebar;
   bool cursor_in_sidebar_old;

   struct
   {
      float cursor_alpha;
      float scroll_y;
      float scroll_y_sidebar;

      float list_alpha;

      float messagebox_alpha;
   } animations;

   bool fade_direction; /* false = left to right, true = right to left */

   size_t selection; /* currently selected entry */
   size_t selection_old; /* previously selected entry (for fancy animation) */
   size_t selection_old_list;

   unsigned entries_height;

   int depth;

   bool draw_sidebar;
   float sidebar_offset;

   unsigned title_font_glyph_width;
   unsigned entry_font_glyph_width;
   unsigned sublabel_font_glyph_width;

   ozone_theme_t *theme;

   struct {
      float selection_border[16];
      float selection[16];
      float entries_border[16];
      float entries_icon[16];
      float entries_checkmark[16];
      float cursor_alpha[16];

      unsigned cursor_state; /* 0 -> 1 -> 0 -> 1 [...] */
      float cursor_border[16];
      float message_background[16];
   } theme_dynamic;

   bool need_compute;

   file_list_t *selection_buf_old;

   bool draw_old_list;
   float scroll_old;

   char *pending_message;
   bool has_all_assets;

   bool is_playlist;
   bool is_playlist_old;

   bool empty_playlist;

   bool osk_cursor; /* true = display it, false = don't */
   bool messagebox_state;
   bool messagebox_state_old;
   bool should_draw_messagebox;

   unsigned old_list_offset_y;

   file_list_t *horizontal_list; /* console tabs */
} ozone_handle_t;

/* If you change this struct, also
   change ozone_alloc_node and
   ozone_copy_node */
typedef struct ozone_node
{
   /* Entries */
   unsigned height;
   unsigned position_y;
   bool wrap;

   /* Console tabs */
   char *console_name;
   uintptr_t icon;
   uintptr_t content_icon;
} ozone_node_t;

static const char *ozone_entries_icon_texture_path(ozone_handle_t *ozone, unsigned id)
{
   char icon_fullpath[255];
   char *icon_name         = NULL;

switch (id)
   {
      case OZONE_ENTRIES_ICONS_TEXTURE_MAIN_MENU:
#if defined(HAVE_LAKKA)
         icon_name = "lakka.png";
         break;
#else
         icon_name = "retroarch.png";
         break;
#endif
      case OZONE_ENTRIES_ICONS_TEXTURE_SETTINGS:
         icon_name = "settings.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_HISTORY:
         icon_name = "history.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_FAVORITES:
         icon_name = "favorites.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ADD_FAVORITE:
         icon_name = "add-favorite.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_MUSICS:
         icon_name = "musics.png";
         break;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
      case OZONE_ENTRIES_ICONS_TEXTURE_MOVIES:
         icon_name = "movies.png";
         break;
#endif
#ifdef HAVE_IMAGEVIEWER
      case OZONE_ENTRIES_ICONS_TEXTURE_IMAGES:
         icon_name = "images.png";
         break;
#endif
      case OZONE_ENTRIES_ICONS_TEXTURE_SETTING:
         icon_name = "setting.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_SUBSETTING:
         icon_name = "subsetting.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ARROW:
         icon_name = "arrow.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_RUN:
         icon_name = "run.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_CLOSE:
         icon_name = "close.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_RESUME:
         icon_name = "resume.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_CLOCK:
         icon_name = "clock.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_FULL:
         icon_name = "battery-full.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_CHARGING:
         icon_name = "battery-charging.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_POINTER:
         icon_name = "pointer.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_SAVESTATE:
         icon_name = "savestate.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE:
         icon_name = "loadstate.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_UNDO:
         icon_name = "undo.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO:
         icon_name = "core-infos.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_WIFI:
         icon_name = "wifi.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_CORE_OPTIONS:
         icon_name = "core-options.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_REMAPPING_OPTIONS:
         icon_name = "core-input-remapping-options.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS:
         icon_name = "core-cheat-options.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_DISK_OPTIONS:
         icon_name = "core-disk-options.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS:
         icon_name = "core-shader-options.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENT_LIST:
         icon_name = "achievement-list.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_SCREENSHOT:
         icon_name = "screenshot.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_RELOAD:
         icon_name = "reload.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_RENAME:
         icon_name = "rename.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_FILE:
         icon_name = "file.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_FOLDER:
         icon_name = "folder.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ZIP:
         icon_name = "zip.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_MUSIC:
         icon_name = "music.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_FAVORITE:
         icon_name = "favorites-content.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_IMAGE:
         icon_name = "image.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_MOVIE:
         icon_name = "movie.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_CORE:
         icon_name = "core.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_RDB:
         icon_name = "database.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_CURSOR:
         icon_name = "cursor.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_SWITCH_ON:
         icon_name = "on.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_SWITCH_OFF:
         icon_name = "off.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ADD:
         icon_name = "add.png";
         break;
#ifdef HAVE_NETWORKING
      case OZONE_ENTRIES_ICONS_TEXTURE_NETPLAY:
         icon_name = "netplay.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ROOM:
         icon_name = "menu_room.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ROOM_LAN:
         icon_name = "menu_room_lan.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ROOM_RELAY:
         icon_name = "menu_room_relay.png";
         break;
#endif
      case OZONE_ENTRIES_ICONS_TEXTURE_KEY:
         icon_name = "key.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_KEY_HOVER:
         icon_name = "key-hover.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_DIALOG_SLICE:
         icon_name = "dialog-slice.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENTS:
         icon_name = "menu_achievements.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_AUDIO:
         icon_name = "menu_audio.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_DRIVERS:
         icon_name = "menu_drivers.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_EXIT:
         icon_name = "menu_exit.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_FRAMESKIP:
         icon_name = "menu_frameskip.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_HELP:
         icon_name = "menu_help.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INFO:
         icon_name = "menu_info.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS:
         icon_name = "Libretro - Pad.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_LATENCY:
         icon_name = "menu_latency.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_NETWORK:
         icon_name = "menu_network.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_POWER:
         icon_name = "menu_power.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_RECORD:
         icon_name = "menu_record.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_SAVING:
         icon_name = "menu_saving.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_UPDATER:
         icon_name = "menu_updater.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_VIDEO:
         icon_name = "menu_video.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_MIXER:
         icon_name = "menu_mixer.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_LOG:
         icon_name = "menu_log.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_OSD:
         icon_name = "menu_osd.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_UI:
         icon_name = "menu_ui.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_USER:
         icon_name = "menu_user.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_PRIVACY:
         icon_name = "menu_privacy.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_PLAYLIST:
         icon_name = "menu_playlist.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_QUICKMENU:
         icon_name = "menu_quickmenu.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_REWIND:
         icon_name = "menu_rewind.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_OVERLAY:
         icon_name = "menu_overlay.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_OVERRIDE:
         icon_name = "menu_override.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_NOTIFICATIONS:
         icon_name = "menu_notifications.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_STREAM:
         icon_name = "menu_stream.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_SHUTDOWN:
         icon_name = "menu_shutdown.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_U:
         icon_name = "input_DPAD-U.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_D:
         icon_name = "input_DPAD-D.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_L:
         icon_name = "input_DPAD-L.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_R:
         icon_name = "input_DPAD-R.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_U:
         icon_name = "input_STCK-U.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_D:
         icon_name = "input_STCK-D.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_L:
         icon_name = "input_STCK-L.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_R:
         icon_name = "input_STCK-R.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_P:
         icon_name = "input_STCK-P.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_U:
         icon_name = "input_BTN-U.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D:
         icon_name = "input_BTN-D.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_L:
         icon_name = "input_BTN-L.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R:
         icon_name = "input_BTN-R.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LB:
         icon_name = "input_LB.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RB:
         icon_name = "input_RB.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LT:
         icon_name = "input_LT.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RT:
         icon_name = "input_RT.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SELECT:
         icon_name = "input_SELECT.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_START:
         icon_name = "input_START.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_CHECKMARK:
         icon_name = "menu_check.png";
         break;
   }

   fill_pathname_join(
      icon_fullpath,
      ozone->icons_path,
      icon_name,
      sizeof(icon_fullpath)
   );

   if (!filestream_exists(icon_fullpath))
   {
      return "subsetting.png";
   }
   else
      return  icon_name;
}

static unsigned ozone_entries_icon_get_id(ozone_handle_t *ozone,
      enum msg_hash_enums enum_idx, unsigned type, bool active)
{
   switch (enum_idx)
   {
      case MENU_ENUM_LABEL_CORE_OPTIONS:
      case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return OZONE_ENTRIES_ICONS_TEXTURE_CORE_OPTIONS;
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST:
         return OZONE_ENTRIES_ICONS_TEXTURE_ADD_FAVORITE;
      case MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION:
         return OZONE_ENTRIES_ICONS_TEXTURE_UNDO;
      case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_REMAPPING_OPTIONS;
      case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
         return OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS;
      case MENU_ENUM_LABEL_DISK_OPTIONS:
         return OZONE_ENTRIES_ICONS_TEXTURE_DISK_OPTIONS;
      case MENU_ENUM_LABEL_SHADER_OPTIONS:
         return OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS;
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
         return OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENT_LIST;
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST_HARDCORE:
         return OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENT_LIST;
      case MENU_ENUM_LABEL_SAVE_STATE:
         return OZONE_ENTRIES_ICONS_TEXTURE_SAVESTATE;
      case MENU_ENUM_LABEL_LOAD_STATE:
         return OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE;
      case MENU_ENUM_LABEL_PARENT_DIRECTORY:
      case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
      case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
         return OZONE_ENTRIES_ICONS_TEXTURE_UNDO;
      case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
         return OZONE_ENTRIES_ICONS_TEXTURE_SCREENSHOT;
      case MENU_ENUM_LABEL_DELETE_ENTRY:
         return OZONE_ENTRIES_ICONS_TEXTURE_CLOSE;
      case MENU_ENUM_LABEL_RESTART_CONTENT:
         return OZONE_ENTRIES_ICONS_TEXTURE_RELOAD;
      case MENU_ENUM_LABEL_RENAME_ENTRY:
         return OZONE_ENTRIES_ICONS_TEXTURE_RENAME;
      case MENU_ENUM_LABEL_RESUME_CONTENT:
         return OZONE_ENTRIES_ICONS_TEXTURE_RESUME;
      case MENU_ENUM_LABEL_FAVORITES:
      case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return OZONE_ENTRIES_ICONS_TEXTURE_FOLDER;
      case MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR:
         return OZONE_ENTRIES_ICONS_TEXTURE_RDB;


      /* Menu collection submenus*/
      case MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST:
         return OZONE_ENTRIES_ICONS_TEXTURE_ZIP;
      case MENU_ENUM_LABEL_GOTO_FAVORITES:
         return OZONE_ENTRIES_ICONS_TEXTURE_FAVORITE;
      case MENU_ENUM_LABEL_GOTO_IMAGES:
         return OZONE_ENTRIES_ICONS_TEXTURE_IMAGE;
      case MENU_ENUM_LABEL_GOTO_VIDEO:
         return OZONE_ENTRIES_ICONS_TEXTURE_MOVIE;
      case MENU_ENUM_LABEL_GOTO_MUSIC:
         return OZONE_ENTRIES_ICONS_TEXTURE_MUSIC;

      /* Menu icons */
      case MENU_ENUM_LABEL_CONTENT_SETTINGS:
      case MENU_ENUM_LABEL_UPDATE_ASSETS:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME:
            return OZONE_ENTRIES_ICONS_TEXTURE_QUICKMENU;
      case MENU_ENUM_LABEL_START_CORE:
      case MENU_ENUM_LABEL_CHEAT_START_OR_CONT:
            return OZONE_ENTRIES_ICONS_TEXTURE_RUN;
      case MENU_ENUM_LABEL_CORE_LIST:
      case MENU_ENUM_LABEL_CORE_SETTINGS:
      case MENU_ENUM_LABEL_CORE_UPDATER_LIST:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE:
            return OZONE_ENTRIES_ICONS_TEXTURE_CORE;
      case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
      case MENU_ENUM_LABEL_SCAN_FILE:
            return OZONE_ENTRIES_ICONS_TEXTURE_FILE;
      case MENU_ENUM_LABEL_ONLINE_UPDATER:
      case MENU_ENUM_LABEL_UPDATER_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_UPDATER;
      case MENU_ENUM_LABEL_UPDATE_LAKKA:
            return OZONE_ENTRIES_ICONS_TEXTURE_MAIN_MENU;
      case MENU_ENUM_LABEL_UPDATE_CHEATS:
            return OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS;
      case MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST:
            return OZONE_ENTRIES_ICONS_TEXTURE_IMAGE;
      case MENU_ENUM_LABEL_UPDATE_OVERLAYS:
      case MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_OVERLAY;
      case MENU_ENUM_LABEL_UPDATE_CG_SHADERS:
      case MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS:
      case MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS:
      case MENU_ENUM_LABEL_AUTO_SHADERS_ENABLE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
            return OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS;
      case MENU_ENUM_LABEL_INFORMATION:
      case MENU_ENUM_LABEL_INFORMATION_LIST:
      case MENU_ENUM_LABEL_SYSTEM_INFORMATION:
      case MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES:
            return OZONE_ENTRIES_ICONS_TEXTURE_INFO;
      case MENU_ENUM_LABEL_UPDATE_DATABASES:
      case MENU_ENUM_LABEL_DATABASE_MANAGER_LIST:
            return OZONE_ENTRIES_ICONS_TEXTURE_RDB;
      case MENU_ENUM_LABEL_CURSOR_MANAGER_LIST:
            return OZONE_ENTRIES_ICONS_TEXTURE_CURSOR;
      case MENU_ENUM_LABEL_HELP_LIST:
      case MENU_ENUM_LABEL_HELP_CONTROLS:
      case MENU_ENUM_LABEL_HELP_LOADING_CONTENT:
      case MENU_ENUM_LABEL_HELP_SCANNING_CONTENT:
      case MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE:
      case MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD:
      case MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
            return OZONE_ENTRIES_ICONS_TEXTURE_HELP;
      case MENU_ENUM_LABEL_QUIT_RETROARCH:
            return OZONE_ENTRIES_ICONS_TEXTURE_EXIT;
      /* Settings icons*/
      case MENU_ENUM_LABEL_DRIVER_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_DRIVERS;
      case MENU_ENUM_LABEL_VIDEO_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_VIDEO;
      case MENU_ENUM_LABEL_AUDIO_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_AUDIO;
      case MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_MIXER;
      case MENU_ENUM_LABEL_INPUT_SETTINGS:
      case MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES:
      case MENU_ENUM_LABEL_INPUT_USER_1_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_2_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_3_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_4_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_5_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_6_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_7_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_8_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_9_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_10_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_11_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_12_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_13_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_14_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_15_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_16_BINDS:
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS;
      case MENU_ENUM_LABEL_LATENCY_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_LATENCY;
      case MENU_ENUM_LABEL_SAVING_SETTINGS:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG:
      case MENU_ENUM_LABEL_SAVE_NEW_CONFIG:
      case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
      case MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS:
            return OZONE_ENTRIES_ICONS_TEXTURE_SAVING;
      case MENU_ENUM_LABEL_LOGGING_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_LOG;
      case MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_FRAMESKIP;
      case MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING:
      case MENU_ENUM_LABEL_RECORDING_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_RECORD;
      case MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING:
            return OZONE_ENTRIES_ICONS_TEXTURE_STREAM;
      case MENU_ENUM_LABEL_QUICK_MENU_STOP_STREAMING:
      case MENU_ENUM_LABEL_QUICK_MENU_STOP_RECORDING:
      case MENU_ENUM_LABEL_CHEAT_DELETE_ALL:
      case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CORE:
      case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_GAME:
      case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CONTENT_DIR:
      case MENU_ENUM_LABEL_CORE_DELETE:
            return OZONE_ENTRIES_ICONS_TEXTURE_CLOSE;
      case MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_OSD;
      case MENU_ENUM_LABEL_SHOW_WIMP:
      case MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_UI;
#ifdef HAVE_LAKKA_SWITCH
      case MENU_ENUM_LABEL_SWITCH_GPU_PROFILE:
      case MENU_ENUM_LABEL_SWITCH_CPU_PROFILE:
#endif
      case MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_POWER;
      case MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENTS;
      case MENU_ENUM_LABEL_NETWORK_INFORMATION:
      case MENU_ENUM_LABEL_NETWORK_SETTINGS:
      case MENU_ENUM_LABEL_WIFI_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_NETWORK;
      case MENU_ENUM_LABEL_PLAYLIST_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_PLAYLIST;
      case MENU_ENUM_LABEL_USER_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_USER;
      case MENU_ENUM_LABEL_DIRECTORY_SETTINGS:
      case MENU_ENUM_LABEL_SCAN_DIRECTORY:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT:
            return OZONE_ENTRIES_ICONS_TEXTURE_FOLDER;
      case MENU_ENUM_LABEL_PRIVACY_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_PRIVACY;

      case MENU_ENUM_LABEL_REWIND_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_REWIND;
      case MENU_ENUM_LABEL_QUICK_MENU_OVERRIDE_OPTIONS:
            return OZONE_ENTRIES_ICONS_TEXTURE_OVERRIDE;
      case MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS:
            return OZONE_ENTRIES_ICONS_TEXTURE_NOTIFICATIONS;
#ifdef HAVE_NETWORKING
      case MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST:
            return OZONE_ENTRIES_ICONS_TEXTURE_RUN;
      case MENU_ENUM_LABEL_NETPLAY_DISCONNECT:
            return OZONE_ENTRIES_ICONS_TEXTURE_CLOSE;
      case MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT:
            return OZONE_ENTRIES_ICONS_TEXTURE_ROOM;
      case MENU_ENUM_LABEL_NETPLAY_REFRESH_ROOMS:
            return OZONE_ENTRIES_ICONS_TEXTURE_RELOAD;
#endif
      case MENU_ENUM_LABEL_REBOOT:
      case MENU_ENUM_LABEL_RESET_TO_DEFAULT_CONFIG:
      case MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS:
      case MENU_ENUM_LABEL_RESTART_RETROARCH:
            return OZONE_ENTRIES_ICONS_TEXTURE_RELOAD;
      case MENU_ENUM_LABEL_SHUTDOWN:
            return OZONE_ENTRIES_ICONS_TEXTURE_SHUTDOWN;
      case MENU_ENUM_LABEL_CONFIGURATIONS:
      case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
      case MENU_ENUM_LABEL_REMAP_FILE_LOAD:
      case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
      case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
      case MENU_ENUM_LABEL_CHEAT_FILE_LOAD:
      case MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND:
         return OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE;
      case MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES:
      case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
         return OZONE_ENTRIES_ICONS_TEXTURE_CHECKMARK;
      default:
            break;
   }

   switch(type)
   {
      case FILE_TYPE_DIRECTORY:
         return OZONE_ENTRIES_ICONS_TEXTURE_FOLDER;
      case FILE_TYPE_PLAIN:
      case FILE_TYPE_IN_CARCHIVE:
      case FILE_TYPE_RPL_ENTRY:
         return OZONE_ENTRIES_ICONS_TEXTURE_FILE;
      case FILE_TYPE_SHADER:
      case FILE_TYPE_SHADER_PRESET:
         return OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS;
      case FILE_TYPE_CARCHIVE:
         return OZONE_ENTRIES_ICONS_TEXTURE_ZIP;
      case FILE_TYPE_MUSIC:
         return OZONE_ENTRIES_ICONS_TEXTURE_MUSIC;
      case FILE_TYPE_IMAGE:
      case FILE_TYPE_IMAGEVIEWER:
         return OZONE_ENTRIES_ICONS_TEXTURE_IMAGE;
      case FILE_TYPE_MOVIE:
         return OZONE_ENTRIES_ICONS_TEXTURE_MOVIE;
      case FILE_TYPE_CORE:
      case FILE_TYPE_DIRECT_LOAD:
         return OZONE_ENTRIES_ICONS_TEXTURE_CORE;
      case FILE_TYPE_RDB:
         return OZONE_ENTRIES_ICONS_TEXTURE_RDB;
      case FILE_TYPE_CURSOR:
         return OZONE_ENTRIES_ICONS_TEXTURE_CURSOR;
      case FILE_TYPE_PLAYLIST_ENTRY:
      case MENU_SETTING_ACTION_RUN:
      case MENU_SETTING_ACTION_RESUME_ACHIEVEMENTS:
         return OZONE_ENTRIES_ICONS_TEXTURE_RUN;
      case MENU_SETTING_ACTION_CLOSE:
      case MENU_SETTING_ACTION_DELETE_ENTRY:
         return OZONE_ENTRIES_ICONS_TEXTURE_CLOSE;
      case MENU_SETTING_ACTION_SAVESTATE:
         return OZONE_ENTRIES_ICONS_TEXTURE_SAVESTATE;
      case MENU_SETTING_ACTION_LOADSTATE:
         return OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE;
      case FILE_TYPE_RDB_ENTRY:
      case MENU_SETTING_ACTION_CORE_INFORMATION:
         return OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO;
      case MENU_SETTING_ACTION_CORE_OPTIONS:
         return OZONE_ENTRIES_ICONS_TEXTURE_CORE_OPTIONS;
      case MENU_SETTING_ACTION_CORE_INPUT_REMAPPING_OPTIONS:
         return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_REMAPPING_OPTIONS;
      case MENU_SETTING_ACTION_CORE_CHEAT_OPTIONS:
         return OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS;
      case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
         return OZONE_ENTRIES_ICONS_TEXTURE_DISK_OPTIONS;
      case MENU_SETTING_ACTION_CORE_SHADER_OPTIONS:
         return OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS;
      case MENU_SETTING_ACTION_SCREENSHOT:
         return OZONE_ENTRIES_ICONS_TEXTURE_SCREENSHOT;
      case MENU_SETTING_ACTION_RESET:
         return OZONE_ENTRIES_ICONS_TEXTURE_RELOAD;
      case MENU_SETTING_ACTION_PAUSE_ACHIEVEMENTS:
         return OZONE_ENTRIES_ICONS_TEXTURE_RESUME;

      case MENU_SETTING_GROUP:
#ifdef HAVE_LAKKA_SWITCH
      case MENU_SET_SWITCH_BRIGHTNESS:
#endif
         return OZONE_ENTRIES_ICONS_TEXTURE_SETTING;
      case MENU_INFO_MESSAGE:
         return OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO;
      case MENU_WIFI:
         return OZONE_ENTRIES_ICONS_TEXTURE_WIFI;
#ifdef HAVE_NETWORKING
      case MENU_ROOM:
         return OZONE_ENTRIES_ICONS_TEXTURE_ROOM;
      case MENU_ROOM_LAN:
         return OZONE_ENTRIES_ICONS_TEXTURE_ROOM_LAN;
      case MENU_ROOM_RELAY:
         return OZONE_ENTRIES_ICONS_TEXTURE_ROOM_RELAY;
#endif
      case MENU_SETTING_ACTION:
         return OZONE_ENTRIES_ICONS_TEXTURE_SETTING;
   }

#ifdef HAVE_CHEEVOS
   if (
         (type >= MENU_SETTINGS_CHEEVOS_START) &&
         (type < MENU_SETTINGS_NETPLAY_ROOMS_START)
      )
   {
      int new_id = type - MENU_SETTINGS_CHEEVOS_START;
      if (get_badge_texture(new_id) != 0)
         return get_badge_texture(new_id);
      /* Should be replaced with placeholder badge icon. */
      return OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENTS;
   }
#endif

   if (
         (type >= MENU_SETTINGS_INPUT_BEGIN) &&
         (type <= MENU_SETTINGS_INPUT_DESC_END)
      )
      {
         unsigned input_id;
         if (type < MENU_SETTINGS_INPUT_DESC_BEGIN)
         {
            input_id = MENU_SETTINGS_INPUT_BEGIN;
            if ( type == input_id + 2)
               return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS;
            if ( type == input_id + 4)
               return OZONE_ENTRIES_ICONS_TEXTURE_RELOAD;
            if ( type == input_id + 5)
               return OZONE_ENTRIES_ICONS_TEXTURE_SAVING;
            input_id = input_id + 7;
         }
         else
         {
            input_id = MENU_SETTINGS_INPUT_DESC_BEGIN;
            while (type > (input_id + 23))
            {
               input_id = (input_id + 24) ;
            }
         }
         if ( type == input_id )
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D;
         if ( type == (input_id + 1))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_L;
         if ( type == (input_id + 2))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SELECT;
         if ( type == (input_id + 3))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_START;
         if ( type == (input_id + 4))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_U;
         if ( type == (input_id + 5))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_D;
         if ( type == (input_id + 6))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_L;
         if ( type == (input_id + 7))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_R;
         if ( type == (input_id + 8))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R;
         if ( type == (input_id + 9))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_U;
         if ( type == (input_id + 10))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LB;
         if ( type == (input_id + 11))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RB;
         if ( type == (input_id + 12))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LT;
         if ( type == (input_id + 13))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RT;
         if ( type == (input_id + 14))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_P;
         if ( type == (input_id + 15))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_P;
         if ( type == (input_id + 16))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_R;
         if ( type == (input_id + 17))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_L;
         if ( type == (input_id + 18))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_D;
         if ( type == (input_id + 19))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_U;
         if ( type == (input_id + 20))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_R;
         if ( type == (input_id + 21))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_L;
         if ( type == (input_id + 22))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_D;
         if ( type == (input_id + 23))
            return OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_U;
      }
   return OZONE_ENTRIES_ICONS_TEXTURE_SUBSETTING;
}

static void ozone_draw_text(
      video_frame_info_t *video_info,
      ozone_handle_t *ozone,
      const char *str, float x,
      float y,
      enum text_alignment text_align,
      unsigned width, unsigned height, font_data_t* font,
      uint32_t color,
      bool draw_outside)
{
   if ((color & 0x000000FF) == 0)
      return;

   menu_display_draw_text(font, str, x, y,
         width, height, color, text_align, 1.0f,
         false,
         1.0, draw_outside);
}

static void ozone_unload_theme_textures(ozone_handle_t *ozone)
{
   int i;
   int j;

   for (j = 0; j < ozone_themes_count; j++)
   {
      ozone_theme_t *theme = ozone_themes[j];
      for (i = 0; i < OZONE_THEME_TEXTURE_LAST; i++)
            video_driver_texture_unload(&theme->textures[i]);
   }
}

static bool ozone_reset_theme_textures(ozone_handle_t *ozone)
{
   int i;
   int j;
   char theme_path[255];
   bool result = true;

   for (j = 0; j < ozone_themes_count; j++)
   {
      ozone_theme_t *theme = ozone_themes[j];

      fill_pathname_join(
         theme_path,
         ozone->png_path,
         theme->name,
         sizeof(theme_path)
      );

      for (i = 0; i < OZONE_THEME_TEXTURE_LAST; i++)
      {
         char filename[PATH_MAX_LENGTH];
         strlcpy(filename, OZONE_THEME_TEXTURES_FILES[i], sizeof(filename));
         strlcat(filename, ".png", sizeof(filename));

         if (!menu_display_reset_textures_list(filename, theme_path, &theme->textures[i], TEXTURE_FILTER_MIPMAP_LINEAR))
            result = false;
      }
   }

   return result;
}

static void ozone_cursor_animation_cb(void *userdata);

static void ozone_animate_cursor(ozone_handle_t *ozone, float *dst, float *target)
{
   menu_animation_ctx_entry_t entry;
   int i;

   entry.easing_enum = EASING_OUT_QUAD;
   entry.tag = (uintptr_t) &ozone_default_theme;
   entry.duration = ANIMATION_CURSOR_PULSE;
   entry.userdata = ozone;

   for (i = 0; i < 16; i++)
   {
      if (i == 3 || i == 7 || i == 11 || i == 15)
         continue;

      if (i == 14)
         entry.cb = ozone_cursor_animation_cb;
      else
         entry.cb = NULL;

      entry.subject        = &dst[i];
      entry.target_value   = target[i];

      menu_animation_push(&entry);
   }
}

static void ozone_cursor_animation_cb(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   float *target = NULL;

   switch (ozone->theme_dynamic.cursor_state)
   {
      case 0:
         target = ozone->theme->cursor_border_1;
         break;
      case 1:
         target = ozone->theme->cursor_border_0;
         break;
   }

   ozone->theme_dynamic.cursor_state = (ozone->theme_dynamic.cursor_state + 1) % 2;

   ozone_animate_cursor(ozone, ozone->theme_dynamic.cursor_border, target);
}

static void ozone_restart_cursor_animation(ozone_handle_t *ozone)
{
   menu_animation_ctx_tag tag = (uintptr_t) &ozone_default_theme;

   if (!ozone->has_all_assets)
      return;

   ozone->theme_dynamic.cursor_state = 1;
   memcpy(ozone->theme_dynamic.cursor_border, ozone->theme->cursor_border_0, sizeof(ozone->theme_dynamic.cursor_border));
   menu_animation_kill_by_tag(&tag);

   ozone_animate_cursor(ozone, ozone->theme_dynamic.cursor_border, ozone->theme->cursor_border_1);
}

static void ozone_set_color_theme(ozone_handle_t *ozone, unsigned color_theme)
{
   ozone_theme_t *theme = ozone_default_theme;

   if (!ozone)
      return;

   switch (color_theme)
   {
      case 1:
         theme = &ozone_theme_dark;
         break;
      case 0:
      default:
         break;
   }

   ozone->theme = theme;

   memcpy(ozone->theme_dynamic.selection_border, ozone->theme->selection_border, sizeof(ozone->theme_dynamic.selection_border));
   memcpy(ozone->theme_dynamic.selection, ozone->theme->selection, sizeof(ozone->theme_dynamic.selection));
   memcpy(ozone->theme_dynamic.entries_border, ozone->theme->entries_border, sizeof(ozone->theme_dynamic.entries_border));
   memcpy(ozone->theme_dynamic.entries_icon, ozone->theme->entries_icon, sizeof(ozone->theme_dynamic.entries_icon));
   memcpy(ozone->theme_dynamic.entries_checkmark, ozone_pure_white, sizeof(ozone->theme_dynamic.entries_checkmark));
   memcpy(ozone->theme_dynamic.cursor_alpha, ozone_pure_white, sizeof(ozone->theme_dynamic.cursor_alpha));
   memcpy(ozone->theme_dynamic.message_background, ozone->theme->message_background, sizeof(ozone->theme_dynamic.message_background));

   ozone_restart_cursor_animation(ozone);

   last_color_theme = color_theme;
}

static ozone_node_t *ozone_alloc_node()
{
   ozone_node_t *node = (ozone_node_t*)malloc(sizeof(*node));

   node->height         = 0;
   node->position_y     = 0;
   node->console_name   = NULL;
   node->icon           = 0;
   node->content_icon   = 0;

   return node;
}

static size_t ozone_list_get_size(void *data, enum menu_list_type type)
{
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (!ozone)
      return 0;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         return menu_entries_get_stack_size(0);
      case MENU_LIST_HORIZONTAL:
         if (ozone && ozone->horizontal_list)
            return file_list_get_size(ozone->horizontal_list);
         break;
      case MENU_LIST_TABS:
         return ozone->system_tab_end;
   }

   return 0;
}

static void ozone_context_reset_horizontal_list(ozone_handle_t *ozone)
{
   unsigned i;
   const char *title;
   char title_noext[255];

   size_t list_size  = ozone_list_get_size(ozone, MENU_LIST_HORIZONTAL);

   for (i = 0; i < list_size; i++)
   {
      const char *path     = NULL;
      ozone_node_t *node   = (ozone_node_t*)file_list_get_userdata_at_offset(ozone->horizontal_list, i);

      if (!node)
      {
         node = ozone_alloc_node();
         if (!node)
            continue;
      }

      file_list_get_at_offset(ozone->horizontal_list, i,
            &path, NULL, NULL, NULL);

      if (!path)
         continue;

      if (!strstr(path, file_path_str(FILE_PATH_LPL_EXTENSION)))
         continue;

      {
         struct texture_image ti;
         char *sysname             = (char*)
            malloc(PATH_MAX_LENGTH * sizeof(char));
         char *texturepath         = (char*)
            malloc(PATH_MAX_LENGTH * sizeof(char));
         char *content_texturepath = (char*)
            malloc(PATH_MAX_LENGTH * sizeof(char));

         sysname[0] = texturepath[0] = content_texturepath[0] = '\0';

         fill_pathname_base_noext(sysname, path,
               PATH_MAX_LENGTH * sizeof(char));

         fill_pathname_join_concat(texturepath, ozone->icons_path, sysname,
               file_path_str(FILE_PATH_PNG_EXTENSION),
               PATH_MAX_LENGTH * sizeof(char));

         /* If the playlist icon doesn't exist return default */

         if (!filestream_exists(texturepath))
               fill_pathname_join_concat(texturepath, ozone->icons_path, "default",
               file_path_str(FILE_PATH_PNG_EXTENSION),
               PATH_MAX_LENGTH * sizeof(char));

         ti.width         = 0;
         ti.height        = 0;
         ti.pixels        = NULL;
         ti.supports_rgba = video_driver_supports_rgba();

         if (image_texture_load(&ti, texturepath))
         {
            if(ti.pixels)
            {
               video_driver_texture_unload(&node->icon);
               video_driver_texture_load(&ti,
                     TEXTURE_FILTER_MIPMAP_LINEAR, &node->icon);
            }

            image_texture_free(&ti);
         }

         fill_pathname_join_delim(sysname, sysname,
               file_path_str(FILE_PATH_CONTENT_BASENAME), '-',
               PATH_MAX_LENGTH * sizeof(char));
         strlcat(content_texturepath, ozone->icons_path, PATH_MAX_LENGTH * sizeof(char));

         strlcat(content_texturepath, path_default_slash(), PATH_MAX_LENGTH * sizeof(char));
         strlcat(content_texturepath, sysname, PATH_MAX_LENGTH * sizeof(char));

         /* If the content icon doesn't exist return default-content */
         if (!filestream_exists(content_texturepath))
         {
            strlcat(ozone->icons_path, "default", PATH_MAX_LENGTH * sizeof(char));
            fill_pathname_join_delim(content_texturepath, ozone->icons_path,
                  file_path_str(FILE_PATH_CONTENT_BASENAME), '-',
                  PATH_MAX_LENGTH * sizeof(char));
         }

         if (image_texture_load(&ti, content_texturepath))
         {
            if(ti.pixels)
            {
               video_driver_texture_unload(&node->content_icon);
               video_driver_texture_load(&ti,
                     TEXTURE_FILTER_MIPMAP_LINEAR, &node->content_icon);
            }

            image_texture_free(&ti);
         }

         /* Console name */
         menu_entries_get_at_offset(
            ozone->horizontal_list,
            i,
            &title, NULL, NULL, NULL, NULL);

         fill_pathname_base_noext(title_noext, title, sizeof(title_noext));

         /* Format : "Vendor - Console"
            Remove everything before the hyphen
            and the subsequent space */
         char *chr         = title_noext;
         bool hyphen_found = false;

         while (true)
         {
            if (*chr == '-')
            {
               hyphen_found = true;
               break;
            }
            else if (*chr == '\0')
               break;

            chr++;
         }

         if (hyphen_found)
            chr += 2;
         else
            chr = title_noext;

         node->console_name = strdup(chr);

         free(sysname);
         free(texturepath);
         free(content_texturepath);
      }
   }
}

static void ozone_context_destroy_horizontal_list(ozone_handle_t *ozone)
{
   unsigned i;
   size_t list_size = ozone_list_get_size(ozone, MENU_LIST_HORIZONTAL);

   for (i = 0; i < list_size; i++)
   {
      const char *path = NULL;
      ozone_node_t *node = (ozone_node_t*)file_list_get_userdata_at_offset(ozone->horizontal_list, i);

      if (!node)
         continue;

      file_list_get_at_offset(ozone->horizontal_list, i,
            &path, NULL, NULL, NULL);

      if (!path || !strstr(path, file_path_str(FILE_PATH_LPL_EXTENSION)))
         continue;

      video_driver_texture_unload(&node->icon);
      video_driver_texture_unload(&node->content_icon);
   }
}

static void ozone_free_node(ozone_node_t *node)
{
   if (!node)
      return;

   if (node->console_name)
      free(node->console_name);

   free(node);
}

static void ozone_free_list_nodes(file_list_t *list, bool actiondata)
{
   unsigned i, size = (unsigned)file_list_get_size(list);

   for (i = 0; i < size; ++i)
   {
      ozone_free_node((ozone_node_t*)file_list_get_userdata_at_offset(list, i));

      /* file_list_set_userdata() doesn't accept NULL */
      list->list[i].userdata = NULL;

      if (actiondata)
         file_list_free_actiondata(list, i);
   }
}


static void ozone_init_horizontal_list(ozone_handle_t *ozone)
{
   menu_displaylist_info_t info;
   settings_t *settings         = config_get_ptr();

   menu_displaylist_info_init(&info);

   info.list                    = ozone->horizontal_list;
   info.path                    = strdup(
         settings->paths.directory_playlist);
   info.label                   = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST));
   info.exts                    = strdup(
         file_path_str(FILE_PATH_LPL_EXTENSION_NO_DOT));
   info.type_default            = FILE_TYPE_PLAIN;
   info.enum_idx                = MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST;

   if (settings->bools.menu_content_show_playlists && !string_is_empty(info.path))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL, &info))
      {
         size_t i;
         for (i = 0; i < ozone->horizontal_list->size; i++)
         {
            ozone_node_t *node = ozone_alloc_node();
            file_list_set_userdata(ozone->horizontal_list, i, node);
         }

         menu_displaylist_process(&info);
      }
   }

   menu_displaylist_info_free(&info);
}

static void ozone_refresh_horizontal_list(ozone_handle_t *ozone)
{
   ozone_context_destroy_horizontal_list(ozone);
   if (ozone->horizontal_list)
   {
      ozone_free_list_nodes(ozone->horizontal_list, false);
      file_list_free(ozone->horizontal_list);
   }
   ozone->horizontal_list = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   ozone->horizontal_list         = (file_list_t*)
      calloc(1, sizeof(file_list_t));

   if (ozone->horizontal_list)
      ozone_init_horizontal_list(ozone);

   ozone_context_reset_horizontal_list(ozone);
}

static void *ozone_init(void **userdata, bool video_is_threaded) 
{
   bool fallback_color_theme           = false;
   unsigned width, height, color_theme = 0;
   ozone_handle_t *ozone               = NULL;
   settings_t *settings                = config_get_ptr();
   menu_handle_t *menu                 = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return false;

   if (!menu_display_init_first_driver(video_is_threaded))
      goto error;

   video_driver_get_size(&width, &height);

   ozone = (ozone_handle_t*)calloc(1, sizeof(ozone_handle_t));

   if (!ozone)
      goto error;

   *userdata = ozone;

   ozone->selection_buf_old = (file_list_t*)calloc(1, sizeof(file_list_t));

   ozone->draw_sidebar              = true;
   ozone->sidebar_offset            = 0;
   ozone->pending_message           = NULL;
   ozone->is_playlist               = false;
   ozone->categories_selection_ptr  = 0;
   ozone->pending_message           = NULL;

   ozone->system_tab_end                = 0;
   ozone->tabs[ozone->system_tab_end]     = OZONE_SYSTEM_TAB_MAIN;
   if (settings->bools.menu_content_show_settings && !settings->bools.kiosk_mode_enable)
      ozone->tabs[++ozone->system_tab_end] = OZONE_SYSTEM_TAB_SETTINGS;
   if (settings->bools.menu_content_show_favorites)
      ozone->tabs[++ozone->system_tab_end] = OZONE_SYSTEM_TAB_FAVORITES;
   if (settings->bools.menu_content_show_history)
      ozone->tabs[++ozone->system_tab_end] = OZONE_SYSTEM_TAB_HISTORY;
#ifdef HAVE_IMAGEVIEWERe
   if (settings->bools.menu_content_show_images)
      ozone->tabs[++ozone->system_tab_end] = OZONE_SYSTEM_TAB_IMAGES;
#endif
   if (settings->bools.menu_content_show_music)
      ozone->tabs[++ozone->system_tab_end] = OZONE_SYSTEM_TAB_MUSIC;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   if (settings->bools.menu_content_show_video)
      ozone->tabs[++ozone->system_tab_end] = OZONE_SYSTEM_TAB_VIDEO;
#endif
#ifdef HAVE_NETWORKING
   if (settings->bools.menu_content_show_netplay)
      ozone->tabs[++ozone->system_tab_end] = OZONE_SYSTEM_TAB_NETPLAY;
#endif
#ifdef HAVE_LIBRETRODB
   if (settings->bools.menu_content_show_add && !settings->bools.kiosk_mode_enable)
      ozone->tabs[++ozone->system_tab_end] = OZONE_SYSTEM_TAB_ADD;
#endif

   menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   menu_display_set_width(width);
   menu_display_set_height(height);

   menu_display_allocate_white_texture();

   ozone->horizontal_list = (file_list_t*)calloc(1, sizeof(file_list_t));

   if (ozone->horizontal_list)
      ozone_init_horizontal_list(ozone);

   /* Theme */
   if (settings->bools.menu_use_preferred_system_color_theme)
   {
#ifdef HAVE_LIBNX
      if (R_SUCCEEDED(setsysInitialize())) 
      {
         ColorSetId theme;
         setsysGetColorSetId(&theme);
         color_theme = (theme == ColorSetId_Dark) ? 1 : 0;
         ozone_set_color_theme(ozone, color_theme);
         settings->uints.menu_ozone_color_theme = color_theme;
         settings->bools.menu_preferred_system_color_theme_set = true;
         setsysExit();
      }
      else
         fallback_color_theme = true;
#endif
   }
   else
      fallback_color_theme = true;

   if (fallback_color_theme)
   {
      color_theme = settings->uints.menu_ozone_color_theme;
      ozone_set_color_theme(ozone, color_theme);
   }
   
   ozone->need_compute                 = false;
   ozone->animations.scroll_y          = 0.0f;
   ozone->animations.scroll_y_sidebar  = 0.0f;

   /* Assets path */
   fill_pathname_join(
      ozone->assets_path,
      settings->paths.directory_assets,
      "ozone",
      sizeof(ozone->assets_path)
   );

   /* PNG path */
   fill_pathname_join(
      ozone->png_path,
      ozone->assets_path,
      "png",
      sizeof(ozone->png_path)
   );

   /* Icons path */
   fill_pathname_join(
      ozone->icons_path,
      ozone->png_path,
      "icons",
      sizeof(ozone->icons_path)
   );

   /* Sidebar path */
   fill_pathname_join(
      ozone->tab_path,
      ozone->png_path,
      "sidebar",
      sizeof(ozone->tab_path)
   );

   last_use_preferred_system_color_theme = settings->bools.menu_use_preferred_system_color_theme;

   return menu;

error:
   if (ozone->horizontal_list)
   {
      ozone_free_list_nodes(ozone->horizontal_list, false);
      file_list_free(ozone->horizontal_list);
   }
   ozone->horizontal_list = NULL;

   if (menu)
      free(menu);

   return NULL;
}

static void ozone_free(void *data)
{
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (ozone)
   {
      video_coord_array_free(&ozone->raster_blocks.footer.carr);
      video_coord_array_free(&ozone->raster_blocks.title.carr);
      video_coord_array_free(&ozone->raster_blocks.time.carr);
      video_coord_array_free(&ozone->raster_blocks.entries_label.carr);
      video_coord_array_free(&ozone->raster_blocks.entries_sublabel.carr);
      video_coord_array_free(&ozone->raster_blocks.sidebar.carr);

      font_driver_bind_block(NULL, NULL);

      if (ozone->selection_buf_old)
      {
         ozone_free_list_nodes(ozone->selection_buf_old, false);
         file_list_free(ozone->selection_buf_old);
      }

      if (ozone->horizontal_list)
      {
         ozone_free_list_nodes(ozone->horizontal_list, false);
         file_list_free(ozone->horizontal_list);
      }

      if (!string_is_empty(ozone->pending_message))
         free(ozone->pending_message);
   }
}

static void ozone_context_reset(void *data, bool is_threaded)
{
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (ozone)
   {
      ozone->has_all_assets = true;

      /* Fonts init */
      unsigned i;
      unsigned size;
      char font_path[PATH_MAX_LENGTH];

      fill_pathname_join(font_path, ozone->assets_path, "regular.ttf", sizeof(font_path));
      ozone->fonts.footer = menu_display_font_file(font_path, FONT_SIZE_FOOTER, is_threaded);
      ozone->fonts.entries_label = menu_display_font_file(font_path, FONT_SIZE_ENTRIES_LABEL, is_threaded);
      ozone->fonts.entries_sublabel = menu_display_font_file(font_path, FONT_SIZE_ENTRIES_SUBLABEL, is_threaded);
      ozone->fonts.time = menu_display_font_file(font_path, FONT_SIZE_TIME, is_threaded);
      ozone->fonts.sidebar = menu_display_font_file(font_path, FONT_SIZE_SIDEBAR, is_threaded);

      fill_pathname_join(font_path, ozone->assets_path, "bold.ttf", sizeof(font_path));
      ozone->fonts.title = menu_display_font_file(font_path, FONT_SIZE_TITLE, is_threaded);

      if (
         !ozone->fonts.footer           ||
         !ozone->fonts.entries_label    ||
         !ozone->fonts.entries_sublabel ||
         !ozone->fonts.time             ||
         !ozone->fonts.sidebar          ||
         !ozone->fonts.title
      )
      {
         ozone->has_all_assets = false;
      }

      /* Naive font size */
      ozone->title_font_glyph_width = FONT_SIZE_TITLE * 3/4;
      ozone->entry_font_glyph_width = FONT_SIZE_ENTRIES_LABEL * 3/4;
      ozone->sublabel_font_glyph_width = FONT_SIZE_ENTRIES_SUBLABEL * 3/4;

      /* More realistic font size */
      size = font_driver_get_message_width(ozone->fonts.title, "a", 1, 1);
      if (size)
         ozone->title_font_glyph_width = size;
      size = font_driver_get_message_width(ozone->fonts.entries_label, "a", 1, 1);
      if (size)
         ozone->entry_font_glyph_width = size;
      size = font_driver_get_message_width(ozone->fonts.entries_sublabel, "a", 1, 1);
      if (size)
         ozone->sublabel_font_glyph_width = size;

      /* Textures init */
      for (i = 0; i < OZONE_TEXTURE_LAST; i++)
      {
         char filename[PATH_MAX_LENGTH];
         strlcpy(filename, OZONE_TEXTURES_FILES[i], sizeof(filename));
         strlcat(filename, ".png", sizeof(filename));

         if (!menu_display_reset_textures_list(filename, ozone->png_path, &ozone->textures[i], TEXTURE_FILTER_MIPMAP_LINEAR))
            ozone->has_all_assets = false;
      }

      /* Sidebar textures */
      for (i = 0; i < OZONE_TAB_TEXTURE_LAST; i++)
      {
         char filename[PATH_MAX_LENGTH];
         strlcpy(filename, OZONE_TAB_TEXTURES_FILES[i], sizeof(filename));
         strlcat(filename, ".png", sizeof(filename));

         if (!menu_display_reset_textures_list(filename, ozone->tab_path, &ozone->tab_textures[i], TEXTURE_FILTER_MIPMAP_LINEAR))
            ozone->has_all_assets = false;
      }

      /* Theme textures */
      if (!ozone_reset_theme_textures(ozone))
         ozone->has_all_assets = false;

      /* Icons textures init */
      for (i = 0; i < OZONE_ENTRIES_ICONS_TEXTURE_LAST; i++)
         if (!menu_display_reset_textures_list(ozone_entries_icon_texture_path(ozone, i), ozone->icons_path, &ozone->icons_textures[i], TEXTURE_FILTER_MIPMAP_LINEAR))
            ozone->has_all_assets = false;

      menu_display_allocate_white_texture();

      /* Horizontal list */
      ozone_context_reset_horizontal_list(ozone);

      /* State reset */
      ozone->frame_count                  = 0;
      ozone->fade_direction               = false;
      ozone->cursor_in_sidebar            = false;
      ozone->cursor_in_sidebar_old        = false;
      ozone->draw_old_list                = false;
      ozone->messagebox_state             = false;
      ozone->messagebox_state_old         = false;

      /* Animations */
      ozone->animations.cursor_alpha   = 1.0f;
      ozone->animations.scroll_y       = 0.0f;
      ozone->animations.list_alpha     = 1.0f;

      /* Missing assets message */
      /* TODO Localize */
      if (!ozone->has_all_assets)
         runloop_msg_queue_push("Some assets are missing - please update them", 1, 256, false);

      ozone_restart_cursor_animation(ozone);
   }
}

static void ozone_collapse_end(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;
   ozone->draw_sidebar = false;
}

static void ozone_context_destroy(void *data)
{
   unsigned i;
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (!ozone)
      return;

   /* Theme */
   ozone_unload_theme_textures(ozone);

   /* Icons */
   for (i = 0; i < OZONE_ENTRIES_ICONS_TEXTURE_LAST; i++)
      video_driver_texture_unload(&ozone->icons_textures[i]);

   /* Textures */
   for (i = 0; i < OZONE_TEXTURE_LAST; i++)
      video_driver_texture_unload(&ozone->textures[i]);
   
   /* Icons */
   for (i = 0; i < OZONE_TAB_TEXTURE_LAST; i++)
      video_driver_texture_unload(&ozone->tab_textures[i]);

   video_driver_texture_unload(&menu_display_white_texture);

   menu_display_font_free(ozone->fonts.footer);
   menu_display_font_free(ozone->fonts.title);
   menu_display_font_free(ozone->fonts.time);
   menu_display_font_free(ozone->fonts.entries_label);
   menu_display_font_free(ozone->fonts.entries_sublabel);
   menu_display_font_free(ozone->fonts.sidebar);

   ozone->fonts.footer = NULL;
   ozone->fonts.title = NULL;
   ozone->fonts.time = NULL;
   ozone->fonts.entries_label = NULL;
   ozone->fonts.entries_sublabel = NULL;
   ozone->fonts.sidebar = NULL;

   menu_animation_ctx_tag tag = (uintptr_t) &ozone_default_theme;
   menu_animation_kill_by_tag(&tag);

   /* Horizontal list */
   ozone_context_destroy_horizontal_list(ozone);
}

static void *ozone_list_get_entry(void *data,
      enum menu_list_type type, unsigned i)
{
   size_t list_size        = 0;
   ozone_handle_t* ozone   = (ozone_handle_t*) data;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         {
            file_list_t *menu_stack = menu_entries_get_menu_stack_ptr(0);
            list_size  = menu_entries_get_stack_size(0);
            if (i < list_size)
               return (void*)&menu_stack->list[i];
         }
         break;
      case MENU_LIST_HORIZONTAL:
         if (ozone && ozone->horizontal_list)
            list_size = file_list_get_size(ozone->horizontal_list);
         if (i < list_size)
            return (void*)&ozone->horizontal_list->list[i];
         break;
      default:
         break;
   }

   return NULL;
}

static int ozone_list_push(void *data, void *userdata,
      menu_displaylist_info_t *info, unsigned type)
{
   menu_displaylist_ctx_parse_entry_t entry;
   int ret                = -1;
   unsigned i             = 0;
   core_info_list_t *list = NULL;
   menu_handle_t *menu    = (menu_handle_t*)data;

   switch (type)
   {
      case DISPLAYLIST_LOAD_CONTENT_LIST:
         {
            settings_t *settings = config_get_ptr();

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES),
                  msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES),
                  MENU_ENUM_LABEL_FAVORITES,
                  MENU_SETTING_ACTION, 0, 0);

            core_info_get_list(&list);
            if (core_info_list_num_info_files(list))
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST,
                     MENU_SETTING_ACTION, 0, 0);
            }

#ifdef HAVE_LIBRETRODB
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_COLLECTION_LIST),
                  msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST),
                  MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST,
                  MENU_SETTING_ACTION, 0, 0);
#endif

            if (frontend_driver_parse_drive_list(info->list, true) != 0)
               menu_entries_append_enum(info->list, "/",
                     msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
                     MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,
                     MENU_SETTING_ACTION, 0, 0);

            if (!settings->bools.kiosk_mode_enable)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS),
                     MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS,
                     MENU_SETTING_ACTION, 0, 0);
            }

            info->need_push    = true;
            info->need_refresh = true;
            ret = 0;
         }
         break;
      case DISPLAYLIST_MAIN_MENU:
         {
            settings_t   *settings      = config_get_ptr();
            rarch_system_info_t *system = runloop_get_system_info();
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            entry.data            = menu;
            entry.info            = info;
            entry.parse_type      = PARSE_ACTION;
            entry.add_empty_entry = false;

            if (!string_is_empty(system->info.library_name) &&
                  !string_is_equal(system->info.library_name,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE)))
            {
               entry.enum_idx      = MENU_ENUM_LABEL_CONTENT_SETTINGS;
               menu_displaylist_setting(&entry);
            }

            if (system->load_no_content)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_START_CORE;
               menu_displaylist_setting(&entry);
            }

#ifndef HAVE_DYNAMIC
            if (frontend_driver_has_fork())
#endif
            {
               if (settings->bools.menu_show_load_core)
               {
                  entry.enum_idx   = MENU_ENUM_LABEL_CORE_LIST;
                  menu_displaylist_setting(&entry);
               }
            }

            if (settings->bools.menu_show_load_content)
            {
               const struct retro_subsystem_info* subsystem = NULL;

               entry.enum_idx      = MENU_ENUM_LABEL_LOAD_CONTENT_LIST;
               menu_displaylist_setting(&entry);

               subsystem           = system->subsystem.data;

               if (subsystem)
               {
                  for (i = 0; i < (unsigned)system->subsystem.size; i++, subsystem++)
                  {
                     char s[PATH_MAX_LENGTH];
                     if (content_get_subsystem() == i)
                     {
                        if (content_get_subsystem_rom_id() < subsystem->num_roms)
                        {
                           snprintf(s, sizeof(s),
                                 "Load %s %s",
                                 subsystem->desc,
                                 i == content_get_subsystem()
                                 ? "\u2605" : " ");
                           menu_entries_append_enum(info->list,
                                 s,
                                 msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_ADD),
                                 MENU_ENUM_LABEL_SUBSYSTEM_ADD,
                                 MENU_SETTINGS_SUBSYSTEM_ADD + i, 0, 0);
                        }
                        else
                        {
                           snprintf(s, sizeof(s),
                                 "Start %s %s",
                                 subsystem->desc,
                                 i == content_get_subsystem()
                                 ? "\u2605" : " ");
                           menu_entries_append_enum(info->list,
                                 s,
                                 msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_LOAD),
                                 MENU_ENUM_LABEL_SUBSYSTEM_LOAD,
                                 MENU_SETTINGS_SUBSYSTEM_LOAD, 0, 0);
                        }
                     }
                     else
                     {
                        snprintf(s, sizeof(s),
                              "Load %s %s",
                              subsystem->desc,
                              i == content_get_subsystem()
                              ? "\u2605" : " ");
                        menu_entries_append_enum(info->list,
                              s,
                              msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_ADD),
                              MENU_ENUM_LABEL_SUBSYSTEM_ADD,
                              MENU_SETTINGS_SUBSYSTEM_ADD + i, 0, 0);
                     }
                  }
               }
            }

            entry.enum_idx      = MENU_ENUM_LABEL_ADD_CONTENT_LIST;
            menu_displaylist_setting(&entry);
#ifdef HAVE_QT
            if (settings->bools.desktop_menu_enable)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_SHOW_WIMP;
               menu_displaylist_setting(&entry);
            }
#endif
#if defined(HAVE_NETWORKING)
            if (settings->bools.menu_show_online_updater && !settings->bools.kiosk_mode_enable)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_ONLINE_UPDATER;
               menu_displaylist_setting(&entry);
            }
#endif
            if (!settings->bools.menu_content_show_settings && !string_is_empty(settings->paths.menu_content_show_settings_password))
            {
               entry.enum_idx      = MENU_ENUM_LABEL_XMB_MAIN_MENU_ENABLE_SETTINGS;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.kiosk_mode_enable && !string_is_empty(settings->paths.kiosk_mode_password))
            {
               entry.enum_idx      = MENU_ENUM_LABEL_MENU_DISABLE_KIOSK_MODE;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_information)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_INFORMATION_LIST;
               menu_displaylist_setting(&entry);
            }

#ifdef HAVE_LAKKA_SWITCH
            entry.enum_idx      = MENU_ENUM_LABEL_SWITCH_CPU_PROFILE;
            menu_displaylist_setting(&entry);

            entry.enum_idx      = MENU_ENUM_LABEL_SWITCH_GPU_PROFILE;
            menu_displaylist_setting(&entry);

            entry.enum_idx      = MENU_ENUM_LABEL_SWITCH_BACKLIGHT_CONTROL;
            menu_displaylist_setting(&entry);
#endif

#ifndef HAVE_DYNAMIC
            entry.enum_idx      = MENU_ENUM_LABEL_RESTART_RETROARCH;
            menu_displaylist_setting(&entry);
#endif

            if (settings->bools.menu_show_configurations && !settings->bools.kiosk_mode_enable)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_CONFIGURATIONS_LIST;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_help)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_HELP_LIST;
               menu_displaylist_setting(&entry);
            }

#if !defined(IOS)
            if (settings->bools.menu_show_quit_retroarch)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_QUIT_RETROARCH;
               menu_displaylist_setting(&entry);
            }
#endif

            if (settings->bools.menu_show_reboot)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_REBOOT;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_shutdown)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_SHUTDOWN;
               menu_displaylist_setting(&entry);
            }

            info->need_push    = true;
            ret = 0;
         }
         break;
   }
   return ret;
}

static size_t ozone_list_get_selection(void *data)
{
   ozone_handle_t *ozone      = (ozone_handle_t*)data;

   if (!ozone)
      return 0;

   return ozone->categories_selection_ptr;
}

static void ozone_list_clear(file_list_t *list)
{
   menu_animation_ctx_tag tag = (uintptr_t)list;
   menu_animation_kill_by_tag(&tag);

   ozone_free_list_nodes(list, false);
}

static void ozone_list_free(file_list_t *list, size_t a, size_t b)
{
   ozone_list_clear(list);
}

/* Compute new scroll position
 * If the center of the currently selected entry is not in the middle
 * And if we can scroll so that it's in the middle
 * Then scroll
 */
static void ozone_update_scroll(ozone_handle_t *ozone, bool allow_animation, ozone_node_t *node)
{
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   menu_animation_ctx_tag tag = (uintptr_t) selection_buf;
   menu_animation_ctx_entry_t entry;
   float new_scroll = 0, entries_middle;
   float bottom_boundary, current_selection_middle_onscreen;
   unsigned video_info_height;

   video_driver_get_size(NULL, &video_info_height);

   current_selection_middle_onscreen    = ENTRIES_START_Y + ozone->animations.scroll_y + node->position_y + node->height / 2;
   bottom_boundary                      = video_info_height - 87 - 78;
   entries_middle                       = video_info_height/2;

   if (current_selection_middle_onscreen != entries_middle)
      new_scroll = ozone->animations.scroll_y - (current_selection_middle_onscreen - entries_middle);
   
   if (new_scroll + ozone->entries_height < bottom_boundary)
      new_scroll = -(78 + ozone->entries_height - bottom_boundary);

   if (new_scroll > 0)
      new_scroll = 0;
   
   if (allow_animation)
   {
      /* Cursor animation */
      ozone->animations.cursor_alpha = 0.0f;

      entry.cb = NULL;
      entry.duration = ANIMATION_CURSOR_DURATION;
      entry.easing_enum = EASING_OUT_QUAD;
      entry.subject = &ozone->animations.cursor_alpha;
      entry.tag = tag;
      entry.target_value = 1.0f;
      entry.userdata = NULL;

      menu_animation_push(&entry);

      /* Scroll animation */
      entry.cb = NULL;
      entry.duration = ANIMATION_CURSOR_DURATION;
      entry.easing_enum = EASING_OUT_QUAD;
      entry.subject = &ozone->animations.scroll_y;
      entry.tag = tag;
      entry.target_value = new_scroll;
      entry.userdata = NULL;

      menu_animation_push(&entry);
   }
   else
   {
      ozone->selection_old = ozone->selection;
      ozone->animations.cursor_alpha = 1.0f;
      ozone->animations.scroll_y = new_scroll;
   }
}

static unsigned ozone_count_lines(const char *str)
{
   unsigned c     = 0;
   unsigned lines = 1;

   for (c = 0; str[c]; c++)
      lines += (str[c] == '\n');
   return lines;
}

static bool ozone_is_playlist(ozone_handle_t *ozone)
{
   bool is_playlist;

   switch (ozone->categories_selection_ptr)
   {
      case OZONE_SYSTEM_TAB_MAIN:
      case OZONE_SYSTEM_TAB_SETTINGS:
      case OZONE_SYSTEM_TAB_ADD:
         is_playlist = false;
         break;
      case OZONE_SYSTEM_TAB_HISTORY:
      case OZONE_SYSTEM_TAB_FAVORITES:
      case OZONE_SYSTEM_TAB_MUSIC:
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
      case OZONE_SYSTEM_TAB_VIDEO:
#endif
#ifdef HAVE_IMAGEVIEWER
      case OZONE_SYSTEM_TAB_IMAGES:
#endif
#ifdef HAVE_NETWORKING
      case OZONE_SYSTEM_TAB_NETPLAY:
#endif
      default:
         is_playlist = true;
         break;
   }

   return is_playlist && ozone->depth == 1;
}

static void ozone_compute_entries_position(ozone_handle_t *ozone)
{
   /* Compute entries height and adjust scrolling if needed */
   unsigned video_info_height;
   unsigned video_info_width;
   size_t i, entries_end;
   file_list_t *selection_buf = NULL;

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   entries_end   = menu_entries_get_size();
   selection_buf = menu_entries_get_selection_buf_ptr(0);

   video_driver_get_size(&video_info_width, &video_info_height);

   ozone->entries_height = 0;

   for (i = 0; i < entries_end; i++)
   {
      /* Entry */
      menu_entry_t entry;
      ozone_node_t *node     = NULL;
      
      menu_entry_init(&entry);
      menu_entry_get(&entry, 0, (unsigned)i, NULL, true);

      /* Empty playlist detection:
         only one item which icon is
         OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO */
      if (ozone->is_playlist && entries_end == 1)
      {
         unsigned icon = ozone_entries_icon_get_id(ozone, entry.enum_idx, entry.type, false);
         ozone->empty_playlist = icon == OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO;
      }
      else
      {
         ozone->empty_playlist = false;
      }

      /* Cache node */
      node = (ozone_node_t*)file_list_get_userdata_at_offset(selection_buf, i);

      if (!node)
         continue;

      node->height = (entry.sublabel ? 100 : 60-8);
      node->wrap   = false;

      if (entry.sublabel)
      {
         char *sublabel_str = menu_entry_get_sublabel(&entry);

         word_wrap(sublabel_str, sublabel_str, (video_info_width - 548) / ozone->sublabel_font_glyph_width, false);

         unsigned lines = ozone_count_lines(sublabel_str);

         if (lines > 1)
         {
            node->height += lines * 15;
            node->wrap = true;
         }

         free(sublabel_str);
      }

      node->position_y = ozone->entries_height;

      ozone->entries_height += node->height;

      menu_entry_free(&entry);
   }

   /* Update scrolling */
   ozone->selection = menu_navigation_get_selection();
   ozone_update_scroll(ozone, false, (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, ozone->selection));
}

static void ozone_render(void *data, bool is_idle)
{
   size_t i;
   menu_animation_ctx_delta_t delta;
   unsigned end                     = (unsigned)menu_entries_get_size();
   ozone_handle_t *ozone            = (ozone_handle_t*)data;
   if (!data)
      return;
   
   if (ozone->need_compute)
   {
      ozone_compute_entries_position(ozone);
      ozone->need_compute = false;
   }

   ozone->selection = menu_navigation_get_selection();

   delta.current = menu_animation_get_delta_time();

   if (menu_animation_get_ideal_delta_time(&delta))
      menu_animation_update(delta.ideal);

   /* TODO Handle pointer & mouse */

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   if (i >= end)
   {
      i = 0;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &i);
   }

   menu_animation_ctl(MENU_ANIMATION_CTL_CLEAR_ACTIVE, NULL);
}

static void ozone_draw_icon(
      video_frame_info_t *video_info,
      unsigned icon_width,
      unsigned icon_height,
      uintptr_t texture,
      float x, float y,
      unsigned width, unsigned height,
      float rotation, float scale_factor,
      float *color)
{
   menu_display_ctx_rotate_draw_t rotate_draw;
   menu_display_ctx_draw_t draw;
   struct video_coords coords;
   math_matrix_4x4 mymat;

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = rotation;
   rotate_draw.scale_x      = scale_factor;
   rotate_draw.scale_y      = scale_factor;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   menu_display_rotate_z(&rotate_draw, video_info);

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = color ? (const float*)color : ozone_pure_white;

   draw.x               = x;
   draw.y               = height - y - icon_height;
   draw.width           = icon_width;
   draw.height          = icon_height;
   draw.scale_factor    = scale_factor;
   draw.rotation        = rotation;
   draw.coords          = &coords;
   draw.matrix_data     = &mymat;
   draw.texture         = texture;
   draw.prim_type       = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id     = 0;

   menu_display_draw(&draw, video_info);
}

static void ozone_draw_header(ozone_handle_t *ozone, video_frame_info_t *video_info)
{
   char title[255];
   menu_animation_ctx_ticker_t ticker;
   settings_t *settings     = config_get_ptr();
   unsigned timedate_offset = 0;

   /* Separator */
   menu_display_draw_quad(video_info, 30, 87, video_info->width - 60, 1, video_info->width, video_info->height, ozone->theme->header_footer_separator);

   /* Title */
   ticker.s = title;
   ticker.len = (video_info->width - 128 - 47 - 130) / ozone->title_font_glyph_width;
   ticker.idx = ozone->frame_count / 20;
   ticker.str = ozone->title;
   ticker.selected = true;

   menu_animation_ticker(&ticker);

   ozone_draw_text(video_info, ozone, title, 128, 20 + FONT_SIZE_TITLE, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.title, ozone->theme->text_rgba, false);

   /* Icon */
   menu_display_blend_begin(video_info);
   ozone_draw_icon(video_info, 60, 60, ozone->textures[OZONE_TEXTURE_RETROARCH], 47, 14, video_info->width, video_info->height, 0, 1, ozone->theme->entries_icon);
   menu_display_blend_end(video_info);

   /* Battery */
   if (video_info->battery_level_enable)
   {
      char msg[12];
      static retro_time_t last_time  = 0;
      bool charging                  = false;
      retro_time_t current_time      = cpu_features_get_time_usec();
      int percent                    = 0;
      enum frontend_powerstate state = get_last_powerstate(&percent);

      if (state == FRONTEND_POWERSTATE_CHARGING)
         charging = true;

      if (current_time - last_time >= INTERVAL_BATTERY_LEVEL_CHECK)
      {
         last_time = current_time;
         task_push_get_powerstate();
      }

      *msg = '\0';

      if (percent > 0)
      {
         timedate_offset = 95;

         snprintf(msg, sizeof(msg), "%d%%", percent);

         ozone_draw_text(video_info, ozone, msg, video_info->width - 85, 30 + FONT_SIZE_TIME, TEXT_ALIGN_RIGHT, video_info->width, video_info->height, ozone->fonts.time, ozone->theme->text_rgba, false);

         menu_display_blend_begin(video_info);
         ozone_draw_icon(video_info, 92, 92, ozone->icons_textures[charging ? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_CHARGING : OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_FULL], video_info->width - 60 - 56, 30 - 28, video_info->width, video_info->height, 0, 1, ozone->theme->entries_icon);
         menu_display_blend_end(video_info);
      }
   }

   /* Timedate */
   if (video_info->timedate_enable)
   {
      menu_display_ctx_datetime_t datetime;
      char timedate[255];

      timedate[0] = '\0';

      datetime.s = timedate;
      datetime.time_mode = settings->uints.menu_timedate_style;
      datetime.len = sizeof(timedate);

      menu_display_timedate(&datetime);

      ozone_draw_text(video_info, ozone, timedate, video_info->width - 87 - timedate_offset, 30 + FONT_SIZE_TIME, TEXT_ALIGN_RIGHT, video_info->width, video_info->height, ozone->fonts.time, ozone->theme->text_rgba, false);

      menu_display_blend_begin(video_info);
      ozone_draw_icon(video_info, 92, 92, ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOCK], video_info->width - 60 - 56 - timedate_offset, 30 - 28, video_info->width, video_info->height, 0, 1, ozone->theme->entries_icon);
      menu_display_blend_end(video_info);
   }
}

static void ozone_color_alpha(float *color, float alpha)
{
   color[3] = color[7] = color[11] = color[15] = alpha;
}

static void ozone_draw_footer(ozone_handle_t *ozone, video_frame_info_t *video_info, settings_t *settings)
{
   char core_title[255];
   /* Separator */
   menu_display_draw_quad(video_info, 23, video_info->height - 78, video_info->width - 60, 1, video_info->width, video_info->height, ozone->theme->header_footer_separator);

   /* Core title or Switch icon */
   if (settings->bools.menu_core_enable && menu_entries_get_core_title(core_title, sizeof(core_title)) == 0)
      ozone_draw_text(video_info, ozone, core_title, 59, video_info->height - 49 + FONT_SIZE_FOOTER, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.footer, ozone->theme->text_rgba, false);
   else
      ozone_draw_icon(video_info, 69, 30, ozone->theme->textures[OZONE_THEME_TEXTURE_SWITCH], 59, video_info->height - 52, video_info->width,video_info->height, 0, 1, NULL);

   /* Buttons */

   {
      unsigned back_width  = 215;
      unsigned back_height = 49;
      unsigned ok_width    = 96;
      unsigned ok_height   = 49;
      bool do_swap         = video_info->input_menu_swap_ok_cancel_buttons;

      if (do_swap)
      {
         back_width  = 96;
         back_height = 49;
         ok_width    = 215;
         ok_height   = 49;
      }

      menu_display_blend_begin(video_info);

      if (do_swap)
      {
         ozone_draw_icon(video_info, 25, 25, ozone->theme->textures[OZONE_THEME_TEXTURE_BUTTON_B], video_info->width - 133, video_info->height - 49, video_info->width,video_info->height, 0, 1, NULL);
         ozone_draw_icon(video_info, 25, 25, ozone->theme->textures[OZONE_THEME_TEXTURE_BUTTON_A], video_info->width - 251, video_info->height - 49, video_info->width,video_info->height, 0, 1, NULL);
      }
      else
      {
         ozone_draw_icon(video_info, 25, 25, ozone->theme->textures[OZONE_THEME_TEXTURE_BUTTON_B], video_info->width - 251, video_info->height - 49, video_info->width,video_info->height, 0, 1, NULL);
         ozone_draw_icon(video_info, 25, 25, ozone->theme->textures[OZONE_THEME_TEXTURE_BUTTON_A], video_info->width - 133, video_info->height - 49, video_info->width,video_info->height, 0, 1, NULL);
      }

      menu_display_blend_end(video_info);

      ozone_draw_text(video_info, ozone,
            do_swap ? 
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK) :
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK),
            video_info->width - back_width, video_info->height - back_height + FONT_SIZE_FOOTER, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.footer, ozone->theme->text_rgba, false);
      ozone_draw_text(video_info, ozone,
            do_swap ? 
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK) :
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK),
            video_info->width - ok_width, video_info->height - ok_height + FONT_SIZE_FOOTER, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.footer, ozone->theme->text_rgba, false);
   }

   menu_display_blend_end(video_info);
}

/* TODO Fluid sidebar width ? */

static void ozone_draw_cursor_slice(ozone_handle_t *ozone,
      video_frame_info_t *video_info,
      int x_offset,
      unsigned width, unsigned height,
      size_t y, float alpha)
{
   ozone_color_alpha(ozone->theme_dynamic.cursor_alpha, alpha);
   ozone_color_alpha(ozone->theme_dynamic.cursor_border, alpha);

   menu_display_blend_begin(video_info);

   /* Cursor without border */
   menu_display_draw_texture_slice(
      video_info,
      x_offset - 14,
      y + 8,
      80, 80,
      width + 3 + 28 - 4,
      height + 20,
      video_info->width, video_info->height,
      ozone->theme_dynamic.cursor_alpha,
      20, 1.0,
      ozone->theme->textures[OZONE_THEME_TEXTURE_CURSOR_NO_BORDER]
   );

   /* Tainted border */
   menu_display_draw_texture_slice(
      video_info,
      x_offset - 14,
      y + 8,
      80, 80,
      width + 3 + 28 - 4,
      height + 20,
      video_info->width, video_info->height,
      ozone->theme_dynamic.cursor_border,
      20, 1.0,
      ozone->textures[OZONE_TEXTURE_CURSOR_BORDER]
   );

   menu_display_blend_end(video_info);
}

static void ozone_draw_cursor_fallback(ozone_handle_t *ozone,
      video_frame_info_t *video_info,
      int x_offset,
      unsigned width, unsigned height,
      size_t y, float alpha)
{
   ozone_color_alpha(ozone->theme_dynamic.selection_border, alpha);
   ozone_color_alpha(ozone->theme_dynamic.selection, alpha);
   
   /* Fill */
   menu_display_draw_quad(video_info, x_offset, y, width, height - 5, video_info->width, video_info->height, ozone->theme_dynamic.selection);

   /* Borders (can't do one single quad because of alpha) */

   /* Top */
   menu_display_draw_quad(video_info, x_offset - 3, y - 3, width + 6, 3, video_info->width, video_info->height, ozone->theme_dynamic.selection_border);

   /* Bottom */
   menu_display_draw_quad(video_info, x_offset - 3, y + height - 5, width + 6, 3, video_info->width, video_info->height, ozone->theme_dynamic.selection_border);

   /* Left */
   menu_display_draw_quad(video_info, x_offset - 3, y, 3, height - 5, video_info->width, video_info->height, ozone->theme_dynamic.selection_border);

   /* Right */
   menu_display_draw_quad(video_info, x_offset + width, y, 3, height - 5, video_info->width, video_info->height, ozone->theme_dynamic.selection_border);
}

static void ozone_draw_cursor(ozone_handle_t *ozone,
      video_frame_info_t *video_info,
      int x_offset,
      unsigned width, unsigned height,
      size_t y, float alpha)
{
   if (ozone->has_all_assets)
      ozone_draw_cursor_slice(ozone, video_info, x_offset, width, height, y, alpha);
   else
      ozone_draw_cursor_fallback(ozone, video_info, x_offset, width, height, y, alpha);
}

static void ozone_draw_sidebar(ozone_handle_t *ozone, video_frame_info_t *video_info)
{
   size_t y;
   unsigned i, sidebar_height;
   char console_title[255];
   menu_animation_ctx_ticker_t ticker;

   unsigned selection_y          = 0;
   unsigned selection_old_y      = 0;
   unsigned horizontal_list_size = 0;

   if (!ozone->draw_sidebar)
      return;

   if (ozone->horizontal_list)
      horizontal_list_size = ozone->horizontal_list->size;

   menu_display_scissor_begin(video_info, 0, 87, 408, video_info->height - 87 - 78);

   /* Background */
   sidebar_height = video_info->height - 87 - 55 - 78;

   if (!video_info->libretro_running)
   {
      menu_display_draw_quad(video_info, ozone->sidebar_offset, 88, 408, 55/2, video_info->width, video_info->height, ozone->theme->sidebar_top_gradient);
      menu_display_draw_quad(video_info, ozone->sidebar_offset, 88 + 55/2, 408, sidebar_height, video_info->width, video_info->height, ozone->theme->sidebar_background);
      menu_display_draw_quad(video_info, ozone->sidebar_offset, 55*2 + sidebar_height, 408, 55/2 + 1, video_info->width, video_info->height, ozone->theme->sidebar_bottom_gradient);
   }

   /* Tabs */
   /* y offset computation */
   y = ENTRIES_START_Y - 10;
   for (i = 0; i < ozone->system_tab_end + horizontal_list_size + 1; i++)
   {
      if (i == ozone->categories_selection_ptr)
      {
         selection_y = y;
         if (ozone->categories_selection_ptr > ozone->system_tab_end)
            selection_y += 30;
      }

      if (i == ozone->categories_active_idx_old)
      {
         selection_old_y = y;
         if (ozone->categories_active_idx_old > ozone->system_tab_end)
            selection_old_y += 30;
      }

      y += 65;
   }

   /* Cursor */
   if (ozone->cursor_in_sidebar)
      ozone_draw_cursor(ozone, video_info, ozone->sidebar_offset + 41, 408 - 81, 52, selection_y-8 + ozone->animations.scroll_y_sidebar, ozone->animations.cursor_alpha);

   if (ozone->cursor_in_sidebar_old)
      ozone_draw_cursor(ozone, video_info, ozone->sidebar_offset + 41, 408 - 81, 52, selection_old_y-8 + ozone->animations.scroll_y_sidebar, 1-ozone->animations.cursor_alpha);

   /* Menu tabs */
   y = ENTRIES_START_Y - 10;
   menu_display_blend_begin(video_info);

   for (i = 0; i < ozone->system_tab_end+1; i++)
   {
      enum msg_hash_enums value_idx;
      const char *title = NULL;
      bool     selected = (ozone->categories_selection_ptr == i);
      unsigned     icon = ozone_system_tabs_icons[ozone->tabs[i]];

      /* Icon */
      ozone_draw_icon(video_info, 40, 40, ozone->tab_textures[icon], ozone->sidebar_offset + 41 + 10, y - 5 + ozone->animations.scroll_y_sidebar, video_info->width, video_info->height, 0, 1, (selected ? ozone->theme->text_selected : ozone->theme->entries_icon));

      value_idx = ozone_system_tabs_value[ozone->tabs[i]];
      title     = msg_hash_to_str(value_idx);

      /* Text */
      ozone_draw_text(video_info, ozone, title, ozone->sidebar_offset + 115 - 10, y + FONT_SIZE_SIDEBAR + ozone->animations.scroll_y_sidebar, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.sidebar, (selected ? ozone->theme->text_selected_rgba : ozone->theme->text_rgba), true);

      y += 65;
   }

   menu_display_blend_end(video_info);

   /* Console tabs */
   if (horizontal_list_size > 0)
   {
      menu_display_draw_quad(video_info, ozone->sidebar_offset + 41 + 10, y - 5 + ozone->animations.scroll_y_sidebar, 408-81, 1, video_info->width, video_info->height, ozone->theme->entries_border);

      y += 30;

      menu_display_blend_begin(video_info);

      for (i = 0; i < horizontal_list_size; i++)
      {
         bool selected = (ozone->categories_selection_ptr == ozone->system_tab_end + 1 + i);

         ozone_node_t *node = (ozone_node_t*) file_list_get_userdata_at_offset(ozone->horizontal_list, i);

         if (!node)
            goto console_iterate;

         /* Icon */
         ozone_draw_icon(video_info, 40, 40, node->icon, ozone->sidebar_offset + 41 + 10, y - 5 + ozone->animations.scroll_y_sidebar, video_info->width, video_info->height, 0, 1, (selected ? ozone->theme->text_selected : ozone->theme->entries_icon));

         /* Text */
         ticker.idx        = ozone->frame_count / 20;
         ticker.len        = 19;
         ticker.s          = console_title;
         ticker.selected   = selected;
         ticker.str        = node->console_name;

         menu_animation_ticker(&ticker);

         ozone_draw_text(video_info, ozone, console_title, ozone->sidebar_offset + 115 - 10, y + FONT_SIZE_SIDEBAR + ozone->animations.scroll_y_sidebar, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.sidebar, (selected ? ozone->theme->text_selected_rgba : ozone->theme->text_rgba), true);

console_iterate:
         y += 65;
      }

      menu_display_blend_end(video_info);

   }

   font_driver_flush(video_info->width, video_info->height, ozone->fonts.sidebar, video_info);
   ozone->raster_blocks.sidebar.carr.coords.vertices = 0;

   menu_display_scissor_end(video_info);
}

static void ozone_draw_entry_value(ozone_handle_t *ozone, 
      video_frame_info_t *video_info,
      char *value,
      unsigned x, unsigned y,
      uint32_t alpha_uint32,
      menu_entry_t *entry)
{
   bool switch_is_on = true;
   bool do_draw_text = false;

   if (!entry->checked && string_is_empty(value))
      return;

   /* check icon */
   if (entry->checked)
   {
      menu_display_blend_begin(video_info);
      ozone_draw_icon(video_info, 30, 30, ozone->theme->textures[OZONE_THEME_TEXTURE_CHECK], x - 20, y - 22, video_info->width, video_info->height, 0, 1, ozone->theme_dynamic.entries_checkmark);
      menu_display_blend_end(video_info);
      return;
   }

   /* text value */
   if (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)) ||
         (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF))))
   {
      switch_is_on = false;
      do_draw_text = false;
   }
   else if (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)) ||
         (string_is_equal(value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON))))
   {
      switch_is_on = true;
      do_draw_text = false;
   }
   else
   {
      if (!string_is_empty(entry->value))
      {
         if (
               string_is_equal(entry->value, "...")     ||
               string_is_equal(entry->value, "(PRESET)")  ||
               string_is_equal(entry->value, "(SHADER)")  ||
               string_is_equal(entry->value, "(COMP)")  ||
               string_is_equal(entry->value, "(CORE)")  ||
               string_is_equal(entry->value, "(MOVIE)") ||
               string_is_equal(entry->value, "(MUSIC)") ||
               string_is_equal(entry->value, "(DIR)")   ||
               string_is_equal(entry->value, "(RDB)")   ||
               string_is_equal(entry->value, "(CURSOR)")||
               string_is_equal(entry->value, "(CFILE)") ||
               string_is_equal(entry->value, "(FILE)")  ||
               string_is_equal(entry->value, "(IMAGE)")
            )
         {
            return;
         }
         else
            do_draw_text = true;
      }
      else
         do_draw_text = true;
   }

   if (do_draw_text)
   {
      ozone_draw_text(video_info, ozone, value, x, y, TEXT_ALIGN_RIGHT, video_info->width, video_info->height, ozone->fonts.entries_label, COLOR_TEXT_ALPHA(ozone->theme->text_selected_rgba, alpha_uint32), false);
   }
   else
   {
      ozone_draw_text(video_info, ozone, (switch_is_on ? msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON) : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)),
               x, y, TEXT_ALIGN_RIGHT, video_info->width, video_info->height, ozone->fonts.entries_label,
               COLOR_TEXT_ALPHA(switch_is_on ? ozone->theme->text_selected_rgba : ozone->theme->text_sublabel_rgba, alpha_uint32), false);
   }
}

static void ozone_draw_entries(ozone_handle_t *ozone, video_frame_info_t *video_info,
   unsigned selection, unsigned selection_old,
   file_list_t *selection_buf, float alpha, float scroll_y,
   bool is_playlist)
{
   bool old_list;
   uint32_t alpha_uint32;
   size_t i, y, entries_end;
   float sidebar_offset, bottom_boundary, invert, alpha_anim;
   unsigned video_info_height, video_info_width, entry_width, button_height;
   menu_entry_t entry;
   int x_offset            = 22;
   size_t selection_y      = 0;
   size_t old_selection_y  = 0;

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   entries_end    = file_list_get_size(selection_buf);
   old_list       = selection_buf == ozone->selection_buf_old;
   y              = ENTRIES_START_Y;
   sidebar_offset = ozone->sidebar_offset / 2.0f;
   entry_width    = video_info->width - 548;
   button_height  = 52; /* height of the button (entry minus sublabel) */

   video_driver_get_size(&video_info_width, &video_info_height);

   bottom_boundary = video_info_height - 87 - 78;
   invert          = (ozone->fade_direction) ? -1 : 1;
   alpha_anim      = old_list ? alpha : 1.0f - alpha;

   if (old_list)
      alpha = 1.0f - alpha;

   if (alpha != 1.0f)
   {    
      if (old_list)
         x_offset += invert * -(alpha_anim * 120); /* left */
      else
         x_offset += invert * (alpha_anim * 120);  /* right */
   }

   x_offset     += (int) sidebar_offset;
   alpha_uint32  = (uint32_t)(alpha*255.0f);

   /* Borders layer */
   for (i = 0; i < entries_end; i++)
   {
      bool entry_selected     = selection == i;
      bool entry_old_selected = selection_old == i;
      ozone_node_t *node      = NULL;
      if (entry_selected)
         selection_y = y;
      
      if (entry_old_selected)
         old_selection_y = y;

      node                    = (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, i);

      if (!node || ozone->empty_playlist)
         goto border_iterate;

      if (y + scroll_y + node->height + 20 < ENTRIES_START_Y)
         goto border_iterate;
      else if (y + scroll_y - node->height - 20 > bottom_boundary)
         goto border_iterate;

      ozone_color_alpha(ozone->theme_dynamic.entries_border, alpha);
      ozone_color_alpha(ozone->theme_dynamic.entries_checkmark, alpha);

      /* Borders */
      menu_display_draw_quad(video_info, x_offset + 456-3, y - 3 + scroll_y, entry_width + 10 - 3 -1, 1, video_info->width, video_info->height, ozone->theme_dynamic.entries_border);
      menu_display_draw_quad(video_info, x_offset + 456-3, y - 3 + button_height + scroll_y, entry_width + 10 - 3-1, 1, video_info->width, video_info->height, ozone->theme_dynamic.entries_border);

border_iterate:
      y += node->height;
   }

   /* Cursor(s) layer - current */
   if (!ozone->cursor_in_sidebar)
      ozone_draw_cursor(ozone, video_info, x_offset + 456, entry_width, button_height, selection_y + scroll_y, ozone->animations.cursor_alpha * alpha);

   /* Old*/
   if (!ozone->cursor_in_sidebar_old)
      ozone_draw_cursor(ozone, video_info, x_offset + 456, entry_width, button_height, old_selection_y + scroll_y, (1-ozone->animations.cursor_alpha) * alpha);

   /* Icons + text */
   y = ENTRIES_START_Y;

   if (old_list)
      y += ozone->old_list_offset_y;

   for (i = 0; i < entries_end; i++)
   {
      unsigned icon;
      menu_entry_t entry;
      menu_animation_ctx_ticker_t ticker;
      char entry_value[255];
      char rich_label[255];
      char entry_value_ticker[255];
      char *sublabel_str;
      ozone_node_t *node      = NULL;
      char *entry_rich_label  = NULL;
      bool entry_selected     = false;
      int text_offset         = -40;

      entry_value[0]         = '\0';
      entry_selected         = selection == i;
      node                   = (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, i);

      menu_entry_init(&entry);
      menu_entry_get(&entry, 0, (unsigned)i, selection_buf, true);
      menu_entry_get_value(&entry, entry_value, sizeof(entry_value));

      if (!node)
         continue;

      if (y + scroll_y + node->height + 20 < ENTRIES_START_Y)
         goto icons_iterate;
      else if (y + scroll_y - node->height - 20 > bottom_boundary)
         goto icons_iterate;

      /* Prepare text */
      entry_rich_label = menu_entry_get_rich_label(&entry);

      ticker.idx = ozone->frame_count / 20;
      ticker.s = rich_label;
      ticker.str = entry_rich_label;
      ticker.selected = entry_selected && !ozone->cursor_in_sidebar;
      ticker.len = (entry_width - 60 - text_offset) / ozone->entry_font_glyph_width;

      menu_animation_ticker(&ticker);

      if (ozone->empty_playlist)
      {
         unsigned text_width = font_driver_get_message_width(ozone->fonts.entries_label, rich_label, (unsigned)strlen(rich_label), 1);
         x_offset = (video_info_width - 408 - 162)/2 - text_width/2;
         y = video_info_height/2 - 60;
      }

      sublabel_str = menu_entry_get_sublabel(&entry);

      if (node->wrap)
         word_wrap(sublabel_str, sublabel_str, (video_info->width - 548) / ozone->sublabel_font_glyph_width, false);

      /* Icon */
      icon = ozone_entries_icon_get_id(ozone, entry.enum_idx, entry.type, entry_selected);
      if (icon != OZONE_ENTRIES_ICONS_TEXTURE_SUBSETTING)
      {
         uintptr_t texture = ozone->icons_textures[icon];

         /* Console specific icons */
         if (entry.type == FILE_TYPE_RPL_ENTRY && ozone->horizontal_list && ozone->categories_selection_ptr > ozone->system_tab_end)
         {
            ozone_node_t *sidebar_node = (ozone_node_t*) file_list_get_userdata_at_offset(ozone->horizontal_list, ozone->categories_selection_ptr - ozone->system_tab_end);

            if (!sidebar_node || !sidebar_node->content_icon)
               texture = ozone->icons_textures[icon];
            else
               texture = sidebar_node->content_icon;
         }

         ozone_color_alpha(ozone->theme_dynamic.entries_icon, alpha);

         menu_display_blend_begin(video_info);
         ozone_draw_icon(video_info, 46, 46, texture, x_offset + 451+5+10, y + scroll_y, video_info->width, video_info->height, 0, 1, ozone->theme_dynamic.entries_icon);
         menu_display_blend_end(video_info);

         text_offset = 0;
      }

      /* Draw text */
      ozone_draw_text(video_info, ozone, rich_label, text_offset + x_offset + 521, y + FONT_SIZE_ENTRIES_LABEL + 8 - 1 + scroll_y, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.entries_label, COLOR_TEXT_ALPHA(ozone->theme->text_rgba, alpha_uint32), false);
      ozone_draw_text(video_info, ozone, sublabel_str, x_offset + 470, y + FONT_SIZE_ENTRIES_SUBLABEL + 80 - 20 - 3 + scroll_y, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.entries_sublabel, COLOR_TEXT_ALPHA(ozone->theme->text_sublabel_rgba, alpha_uint32), false);

      /* Value */
      ticker.idx = ozone->frame_count / 20;
      ticker.s = entry_value_ticker;
      ticker.str = entry_value;
      ticker.selected = entry_selected && !ozone->cursor_in_sidebar;
      ticker.len = (entry_width - 60 - ((int)utf8len(entry_rich_label) * ozone->entry_font_glyph_width)) / ozone->entry_font_glyph_width;

      menu_animation_ticker(&ticker);
      ozone_draw_entry_value(ozone, video_info, entry_value_ticker, x_offset + 426 + entry_width, y + FONT_SIZE_ENTRIES_LABEL + 8 - 1 + scroll_y,alpha_uint32, &entry);
      
      free(entry_rich_label);
      free(sublabel_str);

icons_iterate:
      y += node->height;
      menu_entry_free(&entry);
   }

   /* Text layer */
   font_driver_flush(video_info->width, video_info->height, ozone->fonts.entries_label, video_info);
   font_driver_flush(video_info->width, video_info->height, ozone->fonts.entries_sublabel, video_info);
}

static void ozone_selection_changed(ozone_handle_t *ozone, bool allow_animation)
{
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   menu_animation_ctx_tag tag = (uintptr_t) selection_buf;

   size_t new_selection = menu_navigation_get_selection();
   ozone_node_t *node = (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, new_selection);

   if (!node)
      return;

   if (ozone->selection != new_selection)
   {
      ozone->selection_old = ozone->selection;
      ozone->selection = new_selection;

      ozone->cursor_in_sidebar_old = ozone->cursor_in_sidebar;

      menu_animation_kill_by_tag(&tag);

      ozone_update_scroll(ozone, allow_animation, node);
   }
}

static void ozone_navigation_clear(void *data, bool pending_push)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   if (!pending_push)
      ozone_selection_changed(ozone, true);
}

static void ozone_navigation_pointer_changed(void *data)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   ozone_selection_changed(ozone, true);
}

static void ozone_navigation_set(void *data, bool scroll)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   ozone_selection_changed(ozone, true);
}

static void ozone_navigation_alphabet(void *data, size_t *unused)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   ozone_selection_changed(ozone, true);
}

static unsigned ozone_get_system_theme()
{
   unsigned ret = 0;
#ifdef HAVE_LIBNX
   if (R_SUCCEEDED(setsysInitialize())) 
   {
      ColorSetId theme;
      setsysGetColorSetId(&theme);
      ret = (theme == ColorSetId_Dark) ? 1 : 0;
      setsysExit();
   }

   return ret;
#endif
   return 0;
}

static void ozone_draw_backdrop(video_frame_info_t *video_info, float alpha)
{
   /* TODO Replace this backdrop by a blur shader on the whole screen if available */
   if (alpha == -1)
      alpha = 0.75f;
   ozone_color_alpha(ozone_backdrop, alpha);
   menu_display_draw_quad(video_info, 0, 0, video_info->width, video_info->height, video_info->width, video_info->height, ozone_backdrop);
}

static void ozone_draw_osk(ozone_handle_t *ozone,
      video_frame_info_t *video_info,
      const char *label, const char *str)
{
   int i;
   const char *text;
   char message[2048];
   unsigned text_color;
   struct string_list *list;

   unsigned margin         = 75;
   unsigned padding        = 10;
   unsigned bottom_end     = video_info->height/2;
   unsigned y_offset       = 0;
   bool draw_placeholder   = string_is_empty(str);

   retro_time_t current_time      = cpu_features_get_time_usec();
   static retro_time_t last_time  = 0;

   if (current_time - last_time >= INTERVAL_OSK_CURSOR)
   {
      ozone->osk_cursor = !ozone->osk_cursor;
      last_time = current_time;
   }

   /* Border */
   /* Top */
   menu_display_draw_quad(video_info, margin, margin, video_info->width - margin*2, 1, video_info->width, video_info->height, ozone->theme->entries_border);

   /* Bottom */
   menu_display_draw_quad(video_info, margin, bottom_end - margin, video_info->width - margin*2, 1, video_info->width, video_info->height, ozone->theme->entries_border);

   /* Left */
   menu_display_draw_quad(video_info, margin, margin, 1, bottom_end - margin*2, video_info->width, video_info->height, ozone->theme->entries_border);

   /* Right */
   menu_display_draw_quad(video_info, video_info->width - margin, margin, 1, bottom_end - margin*2, video_info->width, video_info->height, ozone->theme->entries_border);

   /* Backdrop */
   /* TODO Remove the backdrop if blur shader is available */
   menu_display_draw_quad(video_info, margin + 1, margin + 1, video_info->width - margin*2 - 2, bottom_end - margin*2 - 2, video_info->width, video_info->height, ozone_osk_backdrop);

   /* Placeholder & text*/
   if (!draw_placeholder)
   {
      text        = str;
      text_color  = 0xffffffff;
   }
   else
   {
      text        = label;
      text_color  = ozone_theme_light.text_sublabel_rgba;
   }

   word_wrap(message, text, (video_info->width - margin*2 - padding*2) / ozone->entry_font_glyph_width, true);

   list = string_split(message, "\n");

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;

      ozone_draw_text(video_info, ozone, msg, margin + padding * 2, margin + padding + FONT_SIZE_ENTRIES_LABEL + y_offset, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.entries_label, text_color, false);

      /* Cursor */
      if (i == list->size - 1)
      {
         if (ozone->osk_cursor)
         {
            unsigned cursor_x = draw_placeholder ? 0 : font_driver_get_message_width(ozone->fonts.entries_label, msg, (unsigned)strlen(msg), 1);
            menu_display_draw_quad(video_info, margin + padding*2 + cursor_x, margin + padding + y_offset + 3, 1, 25, video_info->width, video_info->height, ozone_pure_white);
         }
      }
      else
      {
         y_offset += 25;
      }
   }

   /* Keyboard */
   menu_display_draw_keyboard(
            ozone->theme->textures[OZONE_THEME_TEXTURE_CURSOR_STATIC],
            ozone->fonts.entries_label,
            video_info,
            menu_event_get_osk_grid(),
            menu_event_get_osk_ptr(),
            ozone->theme->text_rgba);

   string_list_free(list);
}

static void ozone_draw_messagebox(ozone_handle_t *ozone,
      video_frame_info_t *video_info,
      const char *message)
{
   unsigned i, y_position;
   int x, y, longest = 0, longest_width = 0;
   float line_height        = 0;
   unsigned width           = video_info->width;
   unsigned height          = video_info->height;
   struct string_list *list = !string_is_empty(message)
      ? string_split(message, "\n") : NULL;

   if (!list || !ozone || !ozone->fonts.footer)
   {
      if (list)
         string_list_free(list);
      return;
   }

   if (list->elems == 0)
      goto end;

   line_height      = 25;

   y_position       = height / 2;
   if (menu_input_dialog_get_display_kb())
      y_position    = height / 4;

   x                = width  / 2;
   y                = y_position - (list->size-1) * line_height / 2;

   /* find the longest line width */
   for (i = 0; i < list->size; i++)
   {
      const char *msg  = list->elems[i].data;
      int len          = (int)utf8len(msg);

      if (len > longest)
      {
         longest       = len;
         longest_width = font_driver_get_message_width(
               ozone->fonts.footer, msg, (unsigned)strlen(msg), 1);
      }
   }

   ozone_color_alpha(ozone->theme_dynamic.message_background, ozone->animations.messagebox_alpha);

   menu_display_blend_begin(video_info);

   if (ozone->has_all_assets) /* avoid drawing a black box if there's no assets */
      menu_display_draw_texture_slice(
         video_info,
         x - longest_width/2 - 48,
         y + 16 - 48,
         256, 256,
         longest_width + 48 * 2,
         line_height * list->size + 48 * 2,
         width, height,
         ozone->theme_dynamic.message_background,
         16, 1.0,
         ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DIALOG_SLICE]
      );

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;

      if (msg)
         ozone_draw_text(video_info, ozone,
            msg,
            x - longest_width/2.0,
            y + (i+0.75) * line_height,
            TEXT_ALIGN_LEFT,
            width, height,
            ozone->fonts.footer,
            COLOR_TEXT_ALPHA(ozone->theme->text_rgba, (uint32_t)(ozone->animations.messagebox_alpha*255.0f)),
            false
         );
   }

end:
   string_list_free(list);
}

static void ozone_messagebox_fadeout_cb(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   free(ozone->pending_message);
   ozone->pending_message = NULL;

   ozone->should_draw_messagebox = false;
}

static void ozone_frame(void *data, video_frame_info_t *video_info)
{
   ozone_handle_t* ozone                  = (ozone_handle_t*) data;
   settings_t  *settings                  = config_get_ptr();
   unsigned color_theme                   = video_info->ozone_color_theme;
   menu_animation_ctx_tag messagebox_tag  = (uintptr_t)ozone->pending_message;

   menu_animation_ctx_entry_t entry;

   if (!ozone)
      return;

   /* Change theme on the fly */
   if (color_theme != last_color_theme || last_use_preferred_system_color_theme != settings->bools.menu_use_preferred_system_color_theme)
   {
      if (!settings->bools.menu_use_preferred_system_color_theme)
         ozone_set_color_theme(ozone, color_theme);
      else
      {
         video_info->ozone_color_theme = ozone_get_system_theme();
         ozone_set_color_theme(ozone, video_info->ozone_color_theme);
      }

      last_use_preferred_system_color_theme = settings->bools.menu_use_preferred_system_color_theme;
   }

   ozone->frame_count++;

   menu_display_set_viewport(video_info->width, video_info->height);

   /* Clear text */
   font_driver_bind_block(ozone->fonts.footer,  &ozone->raster_blocks.footer);
   font_driver_bind_block(ozone->fonts.title,  &ozone->raster_blocks.title);
   font_driver_bind_block(ozone->fonts.time,  &ozone->raster_blocks.time);
   font_driver_bind_block(ozone->fonts.entries_label,  &ozone->raster_blocks.entries_label);
   font_driver_bind_block(ozone->fonts.entries_sublabel,  &ozone->raster_blocks.entries_sublabel);
   font_driver_bind_block(ozone->fonts.sidebar,  &ozone->raster_blocks.sidebar);

   ozone->raster_blocks.footer.carr.coords.vertices = 0;
   ozone->raster_blocks.title.carr.coords.vertices = 0;
   ozone->raster_blocks.time.carr.coords.vertices = 0;
   ozone->raster_blocks.entries_label.carr.coords.vertices = 0;
   ozone->raster_blocks.entries_sublabel.carr.coords.vertices = 0;
   ozone->raster_blocks.sidebar.carr.coords.vertices = 0;

   /* TODO Replace this by blur backdrop if available */
   ozone_color_alpha(ozone->theme->background, video_info->libretro_running ? 0.75f : 1.0f);

   /* Background */
   menu_display_draw_quad(video_info, 
      0, 0, video_info->width, video_info->height, 
      video_info->width, video_info->height, 
      ozone->theme->background
   );

   /* Header, footer */
   ozone_draw_header(ozone, video_info);
   ozone_draw_footer(ozone, video_info, settings);

   /* Sidebar */
   ozone_draw_sidebar(ozone, video_info);

   /* Menu entries */
   menu_display_scissor_begin(video_info, ozone->sidebar_offset + 408, 87, video_info->width - 408 + (-ozone->sidebar_offset), video_info->height - 87 - 78);

   /* Current list */
   ozone_draw_entries(ozone,
      video_info,
      ozone->selection,
      ozone->selection_old,
      menu_entries_get_selection_buf_ptr(0),
      ozone->animations.list_alpha,
      ozone->animations.scroll_y,
      ozone->is_playlist
   );
   
   /* Old list */
   if (ozone->draw_old_list)
      ozone_draw_entries(ozone,
         video_info,
         ozone->selection_old_list,
         ozone->selection_old_list,
         ozone->selection_buf_old,
         ozone->animations.list_alpha,
         ozone->scroll_old,
         ozone->is_playlist_old
      );

   menu_display_scissor_end(video_info);

   /* Flush first layer of text */
   font_driver_flush(video_info->width, video_info->height, ozone->fonts.footer, video_info);
   font_driver_flush(video_info->width, video_info->height, ozone->fonts.title, video_info);
   font_driver_flush(video_info->width, video_info->height, ozone->fonts.time, video_info);

   font_driver_bind_block(ozone->fonts.footer, NULL);
   font_driver_bind_block(ozone->fonts.title, NULL);
   font_driver_bind_block(ozone->fonts.time, NULL);
   font_driver_bind_block(ozone->fonts.entries_label, NULL);

   /* Message box & OSK - second layer of text */
   ozone->raster_blocks.footer.carr.coords.vertices = 0;
   ozone->raster_blocks.entries_label.carr.coords.vertices = 0;

   if (ozone->should_draw_messagebox || menu_input_dialog_get_display_kb())
   {
      /* Fade in animation */
      if (ozone->messagebox_state_old != ozone->messagebox_state && ozone->messagebox_state)
      {
         ozone->messagebox_state_old = ozone->messagebox_state;

         menu_animation_kill_by_tag(&messagebox_tag);
         ozone->animations.messagebox_alpha = 0.0f;

         entry.cb = NULL;
         entry.duration = ANIMATION_PUSH_ENTRY_DURATION;
         entry.easing_enum = EASING_OUT_QUAD;
         entry.subject = &ozone->animations.messagebox_alpha;
         entry.tag = messagebox_tag;
         entry.target_value = 1.0f;
         entry.userdata = NULL;

         menu_animation_push(&entry);
      }
      /* Fade out animation */
      else if (ozone->messagebox_state_old != ozone->messagebox_state && !ozone->messagebox_state)
      {
         ozone->messagebox_state_old = ozone->messagebox_state;
         ozone->messagebox_state = false;

         menu_animation_kill_by_tag(&messagebox_tag);
         ozone->animations.messagebox_alpha = 1.0f;

         entry.cb = ozone_messagebox_fadeout_cb;
         entry.duration = ANIMATION_PUSH_ENTRY_DURATION;
         entry.easing_enum = EASING_OUT_QUAD;
         entry.subject = &ozone->animations.messagebox_alpha;
         entry.tag = messagebox_tag;
         entry.target_value = 0.0f;
         entry.userdata = ozone;

         menu_animation_push(&entry);
      }

      ozone_draw_backdrop(video_info, fmin(ozone->animations.messagebox_alpha, 0.75f));

      if (menu_input_dialog_get_display_kb())
      {
         const char *label = menu_input_dialog_get_label_buffer();
         const char *str   = menu_input_dialog_get_buffer();

         ozone_draw_osk(ozone, video_info, label, str);
      }
      else
      {
         ozone_draw_messagebox(ozone, video_info, ozone->pending_message);
      }
   }

   font_driver_flush(video_info->width, video_info->height, ozone->fonts.footer, video_info);
   font_driver_flush(video_info->width, video_info->height, ozone->fonts.entries_label, video_info);

   menu_display_unset_viewport(video_info->width, video_info->height);
}

static void ozone_set_header(ozone_handle_t *ozone)
{
   if (ozone->categories_selection_ptr <= ozone->system_tab_end)
   {
      menu_entries_get_title(ozone->title, sizeof(ozone->title));
   }
   else if (ozone->horizontal_list)
   {
      ozone_node_t *node = (ozone_node_t*) file_list_get_userdata_at_offset(ozone->horizontal_list, ozone->categories_selection_ptr - ozone->system_tab_end-1);

      if (node && node->console_name)
         strlcpy(ozone->title, node->console_name, sizeof(ozone->title));
   }
}

static void ozone_animation_end(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;
   ozone->draw_old_list = false;
}

static void ozone_list_open(ozone_handle_t *ozone)
{
   struct menu_animation_ctx_entry entry;

   ozone->draw_old_list = true;

   /* Left/right animation */
   ozone->animations.list_alpha = 0.0f;

   entry.cb = ozone_animation_end;
   entry.duration = ANIMATION_PUSH_ENTRY_DURATION;
   entry.easing_enum = EASING_OUT_QUAD;
   entry.subject = &ozone->animations.list_alpha;
   entry.tag = (uintptr_t) NULL;
   entry.target_value = 1.0f;
   entry.userdata = ozone;

   menu_animation_push(&entry);

   /* Sidebar animation */
   if (ozone->depth == 1)
   {
      ozone->draw_sidebar = true;

      entry.cb = NULL;
      entry.duration = ANIMATION_PUSH_ENTRY_DURATION;
      entry.easing_enum = EASING_OUT_QUAD;
      entry.subject = &ozone->sidebar_offset;
      entry.tag = (uintptr_t) NULL;
      entry.target_value = 0.0f;
      entry.userdata = NULL;

      menu_animation_push(&entry);
   }
   else if (ozone->depth > 1)
   {
      struct menu_animation_ctx_entry entry;

      entry.cb = ozone_collapse_end;
      entry.duration = ANIMATION_PUSH_ENTRY_DURATION;
      entry.easing_enum = EASING_OUT_QUAD;
      entry.subject = &ozone->sidebar_offset;
      entry.tag = (uintptr_t) NULL;
      entry.target_value = -408.0f;
      entry.userdata = (void*) ozone;

      menu_animation_push(&entry);
   }
}

static void ozone_populate_entries(void *data, const char *path, const char *label, unsigned k)
{
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (!ozone)
      return;

   ozone_set_header(ozone);

   if (menu_driver_ctl(RARCH_MENU_CTL_IS_PREVENT_POPULATE, NULL))
   {
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

      /* TODO Update thumbnails */
      ozone_selection_changed(ozone, false);
      return;
   }

   ozone->need_compute = true;

   int new_depth = (int)ozone_list_get_size(ozone, MENU_LIST_PLAIN);

   ozone->fade_direction   = new_depth <= ozone->depth;
   ozone->depth            = new_depth;
   ozone->is_playlist      = ozone_is_playlist(ozone);

   if (ozone->categories_selection_ptr == ozone->categories_active_idx_old)
   {
      ozone_list_open(ozone);
   }
}

static void ozone_change_tab(ozone_handle_t *ozone, 
      enum msg_hash_enums tab, 
      enum menu_settings_type type)
{
   file_list_t *menu_stack = menu_entries_get_menu_stack_ptr(0);
   size_t stack_size;
   menu_ctx_list_t list_info;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t selection = menu_navigation_get_selection();
   menu_file_list_cbs_t *cbs = selection_buf ?
      (menu_file_list_cbs_t*)file_list_get_actiondata_at_offset(selection_buf,
            selection) : NULL;

   list_info.type = MENU_LIST_HORIZONTAL;
   list_info.action = MENU_ACTION_LEFT;

   stack_size = menu_stack->size;

   if (menu_stack->list[stack_size - 1].label)
      free(menu_stack->list[stack_size - 1].label);
   menu_stack->list[stack_size - 1].label = NULL;
   
   menu_stack->list[stack_size - 1].label =
      strdup(msg_hash_to_str(tab));
   menu_stack->list[stack_size - 1].type =
      type;

   menu_driver_list_cache(&list_info);

   if (cbs && cbs->action_content_list_switch)
      cbs->action_content_list_switch(selection_buf, menu_stack, "", "", 0);
}

static void ozone_go_to_sidebar(ozone_handle_t *ozone, uintptr_t tag)
{
   struct menu_animation_ctx_entry entry;

   ozone->selection_old           = ozone->selection;
   ozone->cursor_in_sidebar_old   = ozone->cursor_in_sidebar;
   ozone->cursor_in_sidebar       = true;
   
   /* Cursor animation */
   ozone->animations.cursor_alpha = 0.0f;

   entry.cb = NULL;
   entry.duration = ANIMATION_CURSOR_DURATION;
   entry.easing_enum = EASING_OUT_QUAD;
   entry.subject = &ozone->animations.cursor_alpha;
   entry.tag = tag;
   entry.target_value = 1.0f;
   entry.userdata = NULL;

   menu_animation_push(&entry);
}

static void ozone_leave_sidebar(ozone_handle_t *ozone, uintptr_t tag)
{
   struct menu_animation_ctx_entry entry;

   if (ozone->empty_playlist)
      return;

   ozone->categories_active_idx_old = ozone->categories_selection_ptr;
   ozone->cursor_in_sidebar_old     = ozone->cursor_in_sidebar;
   ozone->cursor_in_sidebar         = false;
   
   /* Cursor animation */
   ozone->animations.cursor_alpha   = 0.0f;

   entry.cb = NULL;
   entry.duration = ANIMATION_CURSOR_DURATION;
   entry.easing_enum = EASING_OUT_QUAD;
   entry.subject = &ozone->animations.cursor_alpha;
   entry.tag = tag;
   entry.target_value = 1.0f;
   entry.userdata = NULL;

   menu_animation_push(&entry);
}

static unsigned ozone_get_selected_sidebar_y_position(ozone_handle_t *ozone)
{
   return ozone->categories_selection_ptr * 65 + (ozone->categories_selection_ptr > ozone->system_tab_end ? 30 : 0);
}

static unsigned ozone_get_sidebar_height(ozone_handle_t *ozone)
{
   return (ozone->system_tab_end + 1 + (ozone->horizontal_list ? ozone->horizontal_list->size : 0)) * 65
      + (ozone->horizontal_list && ozone->horizontal_list->size > 0 ? 30 : 0);
}

static void ozone_sidebar_goto(ozone_handle_t *ozone, unsigned new_selection)
{
   unsigned video_info_height;

   video_driver_get_size(NULL, &video_info_height);

   struct menu_animation_ctx_entry entry;

   menu_animation_ctx_tag tag = (uintptr_t)ozone;

   if (ozone->categories_selection_ptr != new_selection)
   {
      ozone->categories_active_idx_old = ozone->categories_selection_ptr;
      ozone->categories_selection_ptr = new_selection;

      ozone->cursor_in_sidebar_old = ozone->cursor_in_sidebar;

      menu_animation_kill_by_tag(&tag);
   }

   /* Cursor animation */
   ozone->animations.cursor_alpha = 0.0f;

   entry.cb = NULL;
   entry.duration = ANIMATION_CURSOR_DURATION;
   entry.easing_enum = EASING_OUT_QUAD;
   entry.subject = &ozone->animations.cursor_alpha;
   entry.tag = tag;
   entry.target_value = 1.0f;
   entry.userdata = NULL;

   menu_animation_push(&entry);

   /* Scroll animation */
   float new_scroll                             = 0;
   float selected_position_y                    = ozone_get_selected_sidebar_y_position(ozone);
   float current_selection_middle_onscreen      = ENTRIES_START_Y - 10 + ozone->animations.scroll_y_sidebar + selected_position_y + 65 / 2;
   float bottom_boundary                        = video_info_height - 87 - 78;
   float entries_middle                         = video_info_height/2;
   float entries_height                         = ozone_get_sidebar_height(ozone);

   if (current_selection_middle_onscreen != entries_middle)
      new_scroll = ozone->animations.scroll_y_sidebar - (current_selection_middle_onscreen - entries_middle);

   if (new_scroll + entries_height < bottom_boundary)
      new_scroll = -(30 + entries_height - bottom_boundary);

   if (new_scroll > 0)
      new_scroll = 0;

   entry.cb = NULL;
   entry.duration = ANIMATION_CURSOR_DURATION;
   entry.easing_enum = EASING_OUT_QUAD;
   entry.subject = &ozone->animations.scroll_y_sidebar;
   entry.tag = tag;
   entry.target_value = new_scroll;
   entry.userdata = NULL;

   menu_animation_push(&entry);

   if (new_selection > ozone->system_tab_end)
   {
      ozone_change_tab(ozone, MENU_ENUM_LABEL_HORIZONTAL_MENU, MENU_SETTING_HORIZONTAL_MENU);
   }
   else
   {
      ozone_change_tab(ozone, ozone_system_tabs_idx[ozone->tabs[new_selection]], ozone_system_tabs_type[ozone->tabs[new_selection]]);
   }
}

static int ozone_menu_iterate(menu_handle_t *menu, void *userdata, enum menu_action action)
{
   int new_selection;
   struct menu_animation_ctx_entry entry;
   enum menu_action new_action;
   menu_animation_ctx_tag tag;

   file_list_t *selection_buf    = NULL;
   ozone_handle_t *ozone         = (ozone_handle_t*) userdata;
   unsigned horizontal_list_size = 0;

   if (ozone->horizontal_list)
      horizontal_list_size = ozone->horizontal_list->size;

   ozone->messagebox_state = false || menu_input_dialog_get_display_kb();

   if (!ozone)
      return generic_menu_iterate(menu, userdata, action);
      
   selection_buf              = menu_entries_get_selection_buf_ptr(0);
   tag                        = (uintptr_t)selection_buf;
   new_action                 = action;

   /* Inputs override */
   switch (action)
   {
      case MENU_ACTION_DOWN:
         if (!ozone->cursor_in_sidebar)
            break;

         tag = (uintptr_t)ozone;

         new_selection = (ozone->categories_selection_ptr + 1);

         if (new_selection >= ozone->system_tab_end + horizontal_list_size + 1)
            new_selection = 0;

         ozone_sidebar_goto(ozone, new_selection);

         new_action = MENU_ACTION_NOOP;
         break;
      case MENU_ACTION_UP:
         if (!ozone->cursor_in_sidebar)
            break;

         tag = (uintptr_t)ozone;

         new_selection = ozone->categories_selection_ptr - 1;
         
         if (new_selection < 0)
            new_selection = horizontal_list_size + ozone->system_tab_end;

         ozone_sidebar_goto(ozone, new_selection);

         new_action = MENU_ACTION_NOOP;
         break;
      case MENU_ACTION_LEFT:
         if (ozone->cursor_in_sidebar)
         {
            new_action = MENU_ACTION_NOOP;
            break;
         }
         else if (ozone->depth > 1)
            break;

         ozone_go_to_sidebar(ozone, tag);

         new_action = MENU_ACTION_NOOP;
         break;
      case MENU_ACTION_RIGHT:
         if (!ozone->cursor_in_sidebar)
         {
            if (ozone->depth == 1)
               new_action = MENU_ACTION_NOOP;
            break;
         }

         ozone_leave_sidebar(ozone, tag);

         new_action = MENU_ACTION_NOOP;
         break;
      case MENU_ACTION_OK:
         if (ozone->cursor_in_sidebar)
         {
            ozone_leave_sidebar(ozone, tag);
            new_action = MENU_ACTION_NOOP;
            break;
         }

         break;
      case MENU_ACTION_CANCEL:
         if (ozone->cursor_in_sidebar)
         {
            /* Go back to main menu tab */
            if (ozone->categories_selection_ptr != 0)
               ozone_sidebar_goto(ozone, 0);

            new_action = MENU_ACTION_NOOP;
            break;
         }

         if (menu_entries_get_stack_size(0) == 1)
         {
            ozone_go_to_sidebar(ozone, tag);
            new_action = MENU_ACTION_NOOP;
         }
         break;
      default:
         break;
   }

   return generic_menu_iterate(menu, userdata, new_action);
}

/* TODO Fancy toggle animation */

static void ozone_toggle(void *userdata, bool menu_on)
{
   bool tmp              = false;
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   if (!ozone)
      return;

   tmp = !menu_entries_ctl(MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL);

   if (tmp)
      menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   else
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   if (ozone->depth == 1)
   {
      ozone->draw_sidebar = true;
      ozone->sidebar_offset = 0.0f;
   }
}

static bool ozone_menu_init_list(void *data)
{
   menu_displaylist_info_t info;

   file_list_t *menu_stack      = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf   = menu_entries_get_selection_buf_ptr(0);

   menu_displaylist_info_init(&info);

   info.label                   = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
   info.exts                    =
      strdup(file_path_str(FILE_PATH_LPL_EXTENSION_NO_DOT));
   info.type_default            = FILE_TYPE_PLAIN;
   info.enum_idx                = MENU_ENUM_LABEL_MAIN_MENU;

   menu_entries_append_enum(menu_stack, info.path,
         info.label,
         MENU_ENUM_LABEL_MAIN_MENU,
         info.type, info.flags, 0);

   info.list  = selection_buf;

   if (!menu_displaylist_ctl(DISPLAYLIST_MAIN_MENU, &info))
      goto error;

   info.need_push = true;

   if (!menu_displaylist_process(&info))
      goto error;

   menu_displaylist_info_free(&info);
   return true;

error:
   menu_displaylist_info_free(&info);
   return false;
}

static ozone_node_t *ozone_copy_node(const ozone_node_t *old_node)
{
   ozone_node_t *new_node = (ozone_node_t*)malloc(sizeof(*new_node));

   *new_node            = *old_node;

   return new_node;
}

static void ozone_list_insert(void *userdata,
      file_list_t *list,
      const char *path,
      const char *fullpath,
      const char *label,
      size_t list_size,
      unsigned type)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;
   ozone_node_t *node = NULL;
   int i = (int)list_size;

   if (!ozone || !list)
      return;

   ozone->need_compute = true;

   node = (ozone_node_t*)file_list_get_userdata_at_offset(list, i);

   if (!node)
      node = ozone_alloc_node();

   if (!node)
   {
      RARCH_ERR("ozone node could not be allocated.\n");
      return;
   }

   file_list_set_userdata(list, i, node);
}

static void ozone_list_deep_copy(const file_list_t *src, file_list_t *dst,
      size_t first, size_t last)
{
   size_t i, j = 0;
   menu_animation_ctx_tag tag = (uintptr_t)dst;

   menu_animation_kill_by_tag(&tag);

   /* use true here because file_list_copy() doesn't free actiondata */
   ozone_free_list_nodes(dst, true);

   file_list_clear(dst);
   file_list_reserve(dst, (last + 1) - first);

   for (i = first; i <= last; ++i)
   {
      struct item_file *d = &dst->list[j];
      struct item_file *s = &src->list[i];
      void     *src_udata = s->userdata;
      void     *src_adata = s->actiondata;

      *d       = *s;
      d->alt   = string_is_empty(d->alt)   ? NULL : strdup(d->alt);
      d->path  = string_is_empty(d->path)  ? NULL : strdup(d->path);
      d->label = string_is_empty(d->label) ? NULL : strdup(d->label);

      if (src_udata)
         file_list_set_userdata(dst, j, (void*)ozone_copy_node((const ozone_node_t*)src_udata));

      if (src_adata)
      {
         void *data = malloc(sizeof(menu_file_list_cbs_t));
         memcpy(data, src_adata, sizeof(menu_file_list_cbs_t));
         file_list_set_actiondata(dst, j, data);
      }

      ++j;
   }

   dst->size = j;
}

static void ozone_list_cache(void *data,
      enum menu_list_type type, unsigned action)
{
   size_t y, entries_end;
   unsigned i;
   unsigned video_info_height;
   float bottom_boundary;
   ozone_node_t *first_node;
   unsigned first             = 0;
   unsigned last              = 0;
   file_list_t *selection_buf = NULL;
   ozone_handle_t *ozone      = (ozone_handle_t*)data;

   if (!ozone)
      return;

   ozone->need_compute        = true;
   ozone->selection_old_list  = ozone->selection;
   ozone->scroll_old          = ozone->animations.scroll_y;
   ozone->is_playlist_old     = ozone->is_playlist;

   /* Deep copy visible elements */
   video_driver_get_size(NULL, &video_info_height);
   y                          = ENTRIES_START_Y;
   entries_end                = menu_entries_get_size();
   selection_buf              = menu_entries_get_selection_buf_ptr(0);
   bottom_boundary            = video_info_height - 87 - 78;

   for (i = 0; i < entries_end; i++)
   {      
      ozone_node_t *node = (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, i);

      if (!node)
         continue;

      if (y + ozone->animations.scroll_y + node->height + 20 < ENTRIES_START_Y)
      {
         first++;
         goto text_iterate;
      }
      else if (y + ozone->animations.scroll_y - node->height - 20 > bottom_boundary)
         goto text_iterate;

      last++;
text_iterate:
      y += node->height;
   }

   last -= 1;
   last += first;

   first_node = (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, first);
   ozone->old_list_offset_y = first_node->position_y;

   ozone_list_deep_copy(selection_buf, ozone->selection_buf_old, first, last);
}

static int ozone_environ_cb(enum menu_environ_cb type, void *data, void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   if (!ozone)
      return -1;

   switch (type)
   {
      case MENU_ENVIRON_RESET_HORIZONTAL_LIST:
         if (!ozone)
            return -1;

         ozone_refresh_horizontal_list(ozone);
         break;
      default:
         return -1;
   }

   return 0;
}

static void ozone_messagebox(void *data, const char *message)
{
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (!ozone || string_is_empty(message))
      return;

   if (ozone->pending_message)
   {
      free(ozone->pending_message);
      ozone->pending_message = NULL;
   }

   ozone->pending_message = strdup(message);
   ozone->messagebox_state = true || menu_input_dialog_get_display_kb();
   ozone->should_draw_messagebox = true;
}

static int ozone_deferred_push_content_actions(menu_displaylist_info_t *info)
{
   if (!menu_displaylist_ctl(
            DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS, info))
      return -1;
   menu_displaylist_process(info);
   menu_displaylist_info_free(info);
   return 0;
}

static int ozone_list_bind_init_compare_label(menu_file_list_cbs_t *cbs)
{
   if (cbs && cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_CONTENT_ACTIONS:
            cbs->action_deferred_push = ozone_deferred_push_content_actions;
            break;
         default:
            return -1;
      }
   }

   return 0;
}

static int ozone_list_bind_init(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (ozone_list_bind_init_compare_label(cbs) == 0)
      return 0;

   return -1;
}

menu_ctx_driver_t menu_ctx_ozone = {
   NULL,                         /* set_texture */
   ozone_messagebox,
   ozone_menu_iterate,
   ozone_render,
   ozone_frame,
   ozone_init,
   ozone_free,
   ozone_context_reset,
   ozone_context_destroy,
   ozone_populate_entries,
   ozone_toggle,
   ozone_navigation_clear,
   ozone_navigation_pointer_changed,
   ozone_navigation_pointer_changed,
   ozone_navigation_set,
   ozone_navigation_pointer_changed,
   ozone_navigation_alphabet,
   ozone_navigation_alphabet,
   ozone_menu_init_list,
   ozone_list_insert,
   NULL,                         /* list_prepend */
   ozone_list_free,
   ozone_list_clear,
   ozone_list_cache,
   ozone_list_push,
   ozone_list_get_selection,
   ozone_list_get_size,
   ozone_list_get_entry,
   NULL,                         /* list_set_selection */
   ozone_list_bind_init,         /* bind_init */
   NULL,                         /* load_image */
   "ozone",
   ozone_environ_cb,
   NULL,                         /* pointer_tap */
   NULL,                         /* update_thumbnail_path */
   NULL,                         /* update_thumbnail_image */
   NULL,                         /* set_thumbnail_system */
   NULL,                         /* set_thumbnail_content */
   menu_display_osk_ptr_at_pos,
   NULL,                         /* update_savestate_thumbnail_path */
   NULL                          /* update_savestate_thumbnail_image */
};