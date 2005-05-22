/********************************************************************** 
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <string.h>

#include "events.h"
#include "fcintl.h"
#include "game.h"
#include "log.h"
#include "mem.h"
#include "registry.h"
#include "shared.h"
#include "support.h"
#include "version.h"

#include "audio.h"
#include "chatline_g.h"
#include "cityrepdata.h"
#include "civclient.h"
#include "clinet.h"
#include "cma_fec.h"
#include "mapview_common.h"
#include "plrdlg_common.h"
#include "tilespec.h"

#include "options.h"
 
/** Defaults for options normally on command line **/

char default_user_name[512] = "\0";
char default_server_host[512] = "localhost";
int  default_server_port = DEFAULT_SOCK_PORT;
char default_metaserver[512] = METALIST_ADDR;
char default_tileset_name[512] = "\0";
char default_sound_set_name[512] = "stdsounds";
char default_sound_plugin_name[512] = "\0";

bool save_options_on_exit = TRUE;
bool fullscreen_mode = TRUE;

/** Local Options: **/

bool solid_color_behind_units = FALSE;
bool sound_bell_at_new_turn = FALSE;
int  smooth_move_unit_msec = 30;
int smooth_center_slide_msec = 200;
bool do_combat_animation = TRUE;
bool ai_manual_turn_done = TRUE;
bool auto_center_on_unit = TRUE;
bool auto_center_on_combat = FALSE;
bool wakeup_focus = TRUE;
bool center_when_popup_city = TRUE;
bool concise_city_production = FALSE;
bool auto_turn_done = FALSE;
bool meta_accelerators = TRUE;
bool map_scrollbars = FALSE;
bool dialogs_on_top = TRUE;
bool ask_city_name = TRUE;
bool popup_new_cities = TRUE;
bool keyboardless_goto = TRUE;
bool show_task_icons = TRUE;

/* This option is currently set by the client - not by the user. */
bool update_city_text_in_refresh_tile = TRUE;

const char *client_option_class_names[COC_MAX] = {
  N_("Graphics"),
  N_("Sound"),
  N_("Interface"),
  N_("Network")
};

static client_option common_options[] = {
  GEN_STR_OPTION(default_user_name,
		 N_("Login name"),
		 N_("This is the default login username that will be used "
		    "in the connection dialogs or with the -a command-line "
		    "parameter."),
		 COC_NETWORK, NULL, NULL),
  GEN_STR_OPTION(default_server_host,
		 N_("Server"),
		 N_("This is the default server hostname that will be used "
		    "in the connection dialogs or with the -a command-line "
		    "parameter."),
		 COC_NETWORK, NULL, NULL),
  GEN_INT_OPTION(default_server_port,
		 N_("Server port"),
		 N_("This is the default server port that will be used "
		    "in the connection dialogs or with the -a command-line "
		    "parameter."),
		 COC_NETWORK),
  GEN_STR_OPTION(default_metaserver,
		 N_("Metaserver"),
		 N_("The metaserver is a host that the client contacts to "
		    "find out about games on the internet.  Don't change "
		    "this from its default value unless you know what "
		    "you're doing."),
		 COC_NETWORK, NULL, NULL),
  GEN_STR_OPTION(default_sound_set_name,
		 N_("Soundset"),
		 N_("This is the soundset that will be used.  Changing "
		    "this is the same as using the -S command-line "
		    "parameter."),
		 COC_SOUND, get_soundset_list, NULL),
  GEN_STR_OPTION(default_sound_plugin_name,
		 N_("Sound plugin"),
		 N_("If you have a problem with sound, try changing the "
		    "sound plugin.  The new plugin won't take effect until "
		    "you restart Freeciv.  Changing this is the same as "
		    "using the -P command-line option."),
		 COC_SOUND, get_soundplugin_list, NULL),
  GEN_STR_OPTION(default_tileset_name, N_("Tileset"),
		 N_("By changing this option you change the active tileset. "
		    "This is the same as using the -t command-line "
		    "parameter."),
		 COC_GRAPHICS,
		 get_tileset_list, tilespec_reread_callback),

  GEN_BOOL_OPTION_CB(solid_color_behind_units,
		     N_("Solid unit background color"),
		     N_("Setting this option will cause units on the map "
			"view to be drawn with a solid background color "
			"instead of the flag backdrop."),
		     COC_GRAPHICS, mapview_redraw_callback),
  GEN_BOOL_OPTION(sound_bell_at_new_turn, N_("Sound bell at new turn"),
		  N_("Set this option to have a \"bell\" event be generated "
		     "at the start of a new turn.  You can control the "
		     "behavior of the \"bell\" event by editing the message "
		     "options."),
		  COC_SOUND),
  GEN_INT_OPTION(smooth_move_unit_msec,
		 N_("Unit movement animation time (milliseconds)"),
		 N_("This option controls how long unit \"animation\" takes "
		    "when a unit moves on the map view.  Set it to 0 to "
		    "disable animation entirely."),
		 COC_GRAPHICS),
  GEN_INT_OPTION(smooth_center_slide_msec,
		 N_("Mapview recentering time (milliseconds)"),
		 N_("When the map view is recentered, it will slide "
		    "smoothly over the map to its new position.  This "
		    "option controls how long this slide lasts.  Set it to "
		    "0 to disable mapview sliding entirely."),
		 COC_GRAPHICS),
  GEN_BOOL_OPTION(do_combat_animation, N_("Show combat animation"),
		  N_("If this option is disabled them combat animation "
		     "between units on the mapview will be turned off."),
		  COC_GRAPHICS),
  GEN_BOOL_OPTION_CB(draw_full_citybar, N_("Draw a larger citybar"),
		     N_("If this option is set then instead of just the "
			"city name and attributes, a large amount of data "
			"will be drawn beneach each city in the 'citybar'."),
		     COC_GRAPHICS, mapview_redraw_callback),
  GEN_BOOL_OPTION_CB(draw_unit_shields, N_("Draw shield graphics for units"),
		     N_("If set, then special shield graphics will be drawn "
			"as the flags on units.  If unset, the full flag will "
			"be drawn."),
		     COC_GRAPHICS, mapview_redraw_callback),
  GEN_BOOL_OPTION(ai_manual_turn_done, N_("Manual Turn Done in AI Mode"),
		  N_("If this option is disabled, then you will not have "
		     "to press the turn done button manually when watching "
		     "an AI player."),
		  COC_INTERFACE),
  GEN_BOOL_OPTION(auto_center_on_unit,      N_("Auto Center on Units"),
		  N_("Set this option to have the active unit centered "
		     "automatically when the unit focus changes."),
		  COC_INTERFACE),
  GEN_BOOL_OPTION(auto_center_on_combat,    N_("Auto Center on Combat"),
		  N_("Set this option to have any combat be centered "
		     "automatically.  Disabled this will speed up the time "
		     "between turns but may cause you to miss combat "
		     "entirely."),
		  COC_INTERFACE),
  GEN_BOOL_OPTION(wakeup_focus,             N_("Focus on Awakened Units"),
		  N_("Set this option to have newly awoken units be "
		     "focused automatically."),
		  COC_INTERFACE),
  GEN_BOOL_OPTION(center_when_popup_city,   N_("Center map when Popup city"),
		  N_("If this option is set then when a city dialog is "
		     "popped up, the city will be centered automatically."),
		  COC_INTERFACE),
  GEN_BOOL_OPTION(concise_city_production,  N_("Concise City Production"),
		  N_("Set this option to make the city production (as shown "
		     "in the city dialog) to be more compact."),
		  COC_INTERFACE),
  GEN_BOOL_OPTION(auto_turn_done,           N_("End Turn when done moving"),
		  N_("If this option is set then when all your units are "
		     "done moving the turn will be ended for you "
		     "automatically."),
		  COC_INTERFACE),
  GEN_BOOL_OPTION(ask_city_name,            N_("Prompt for city names"),
		  N_("If this option is disabled then city names will be "
		     "chosen for you automatically by the server."),
		  COC_INTERFACE),
  GEN_BOOL_OPTION(popup_new_cities, N_("Pop up city dialog for new cities"),
		  N_("If this option is set then a newly-founded city will "
		     "havce its city dialog popped up automatically."),
		  COC_INTERFACE),
};
#undef GEN_INT_OPTION
#undef GEN_BOOL_OPTION
#undef GEN_STR_OPTION

int num_options;
client_option *options;

/** View Options: **/

bool draw_city_outlines = TRUE;
bool draw_map_grid = FALSE;
bool draw_city_names = TRUE;
bool draw_city_growth = TRUE;
bool draw_city_productions = FALSE;
bool draw_terrain = TRUE;
bool draw_coastline = FALSE;
bool draw_roads_rails = TRUE;
bool draw_irrigation = TRUE;
bool draw_mines = TRUE;
bool draw_fortress_airbase = TRUE;
bool draw_specials = TRUE;
bool draw_pollution = TRUE;
bool draw_cities = TRUE;
bool draw_units = TRUE;
bool draw_focus_unit = FALSE;
bool draw_fog_of_war = TRUE;
bool draw_borders = TRUE;
bool draw_full_citybar = TRUE;
bool draw_unit_shields = TRUE;
bool player_dlg_show_dead_players = TRUE;

#define VIEW_OPTION(name) { #name, &name }
#define VIEW_OPTION_TERMINATOR { NULL, NULL }

view_option view_options[] = {
  VIEW_OPTION(draw_city_outlines),
  VIEW_OPTION(draw_map_grid),
  VIEW_OPTION(draw_city_names),
  VIEW_OPTION(draw_city_growth),
  VIEW_OPTION(draw_city_productions),
  VIEW_OPTION(draw_terrain),
  VIEW_OPTION(draw_coastline),
  VIEW_OPTION(draw_roads_rails),
  VIEW_OPTION(draw_irrigation),
  VIEW_OPTION(draw_mines),
  VIEW_OPTION(draw_fortress_airbase),
  VIEW_OPTION(draw_specials),
  VIEW_OPTION(draw_pollution),
  VIEW_OPTION(draw_cities),
  VIEW_OPTION(draw_units),
  VIEW_OPTION(draw_focus_unit),
  VIEW_OPTION(draw_fog_of_war),
  VIEW_OPTION(draw_borders),
  VIEW_OPTION(player_dlg_show_dead_players),
  VIEW_OPTION_TERMINATOR
};

#undef VIEW_OPTION
#undef VIEW_OPTION_TERMINATOR

/** Message Options: **/

unsigned int messages_where[E_LAST];

/****************************************************************
  These could be a static table initialisation, except
  its easier to do it this way.
*****************************************************************/
void message_options_init(void)
{
  int out_only[] = { E_IMP_BUY, E_IMP_SOLD, E_UNIT_BUY,
		     E_UNIT_LOST_ATT, E_UNIT_WIN_ATT, E_GAME_START,
		     E_NATION_SELECTED, E_CITY_BUILD, E_NEXT_YEAR,
		     E_CITY_PRODUCTION_CHANGED,
		     E_CITY_MAY_SOON_GROW, E_WORKLIST};
  int all[] = { E_MESSAGE_WALL, E_TUTORIAL };
  int i;

  for(i=0; i<E_LAST; i++) {
    messages_where[i] = MW_MESSAGES;
  }
  for (i = 0; i < ARRAY_SIZE(out_only); i++) {
    messages_where[out_only[i]] = 0;
  }
  for (i = 0; i < ARRAY_SIZE(all); i++) {
    messages_where[all[i]] = MW_MESSAGES | MW_POPUP;
  }

  events_init();
}

/****************************************************************
... 
*****************************************************************/
void message_options_free(void)
{
  events_free();
}

/****************************************************************
... 
*****************************************************************/
static void message_options_load(struct section_file *file, const char *prefix)
{
  int i;

  for (i = 0; i < E_LAST; i++) {
    messages_where[i] =
      secfile_lookup_int_default(file, messages_where[i],
				 "%s.message_where_%02d", prefix, i);
  }
}

/****************************************************************
... 
*****************************************************************/
static void message_options_save(struct section_file *file, const char *prefix)
{
  int i;

  for (i = 0; i < E_LAST; i++) {
    secfile_insert_int_comment(file, messages_where[i],
			       get_event_message_text(i),
			       "%s.message_where_%02d", prefix, i);
  }
}


static void save_cma_preset(struct section_file *file, char *name,
			    const struct cm_parameter *const pparam,
			    int inx);
static void load_cma_preset(struct section_file *file, int inx);

static void save_global_worklist(struct section_file *file, const char *path, 
                                 int wlinx, struct worklist *pwl);

static void load_global_worklist(struct section_file *file, const char *path,
				 int wlinx, struct worklist *pwl);


/****************************************************************
 The "options" file handles actual "options", and also view options,
 message options, city report settings, cma settings, and 
 saved global worklists
*****************************************************************/

/****************************************************************
 Returns pointer to static memory containing name of option file.
 Ie, based on FREECIV_OPT env var, and home dir. (or a
 OPTION_FILE_NAME define defined in config.h)
 Or NULL if problem.
*****************************************************************/
static char *option_file_name(void)
{
  static char name_buffer[256];
  char *name;

  name = getenv("FREECIV_OPT");

  if (name) {
    sz_strlcpy(name_buffer, name);
  } else {
#ifndef OPTION_FILE_NAME
    name = user_home_dir();
    if (!name) {
      append_output_window(_("Cannot find your home directory"));
      return NULL;
    }
    mystrlcpy(name_buffer, name, 231);
    sz_strlcat(name_buffer, "/.civclientrc");
#else
    mystrlcpy(name_buffer,OPTION_FILE_NAME,sizeof(name_buffer));
#endif
  }
  freelog(LOG_VERBOSE, "settings file is %s", name_buffer);
  return name_buffer;
}
  
/****************************************************************
 this loads from the rc file any options which are not ruleset specific 
 it is called on client init.
*****************************************************************/
void load_general_options(void)
{
  struct section_file sf;
  const char * const prefix = "client";
  char *name;
  int i, num;
  view_option *v;

  assert(options == NULL);
  num_options = ARRAY_SIZE(common_options) + num_gui_options;
  options = fc_malloc(num_options * sizeof(*options));
  memcpy(options, common_options, sizeof(common_options));
  memcpy(options + ARRAY_SIZE(common_options), gui_options,
	 num_gui_options * sizeof(*options));

  name = option_file_name();
  if (!name) {
    /* fail silently */
    return;
  }
  if (!section_file_load(&sf, name)) {
    create_default_cma_presets();
    return;  
  }

  /* a "secret" option for the lazy. TODO: make this saveable */
  sz_strlcpy(password, 
             secfile_lookup_str_default(&sf, "", "%s.password", prefix));

  save_options_on_exit =
    secfile_lookup_bool_default(&sf, save_options_on_exit,
				"%s.save_options_on_exit", prefix);
  fullscreen_mode =
    secfile_lookup_bool_default(&sf, fullscreen_mode,
				"%s.fullscreen_mode", prefix);

  for (i = 0; i < num_options; i++) {
    client_option *o = options + i;

    switch (o->type) {
    case COT_BOOL:
      *(o->p_bool_value) =
	  secfile_lookup_bool_default(&sf, *(o->p_bool_value), "%s.%s",
				      prefix, o->name);
      break;
    case COT_INT:
      *(o->p_int_value) =
	  secfile_lookup_int_default(&sf, *(o->p_int_value), "%s.%s",
				      prefix, o->name);
      break;
    case COT_STR:
      mystrlcpy(o->p_string_value,
                     secfile_lookup_str_default(&sf, o->p_string_value, "%s.%s",
                     prefix, o->name), o->string_length);
      break;
    }
  }
  for (v = view_options; v->name; v++) {
    *(v->p_value) =
	secfile_lookup_bool_default(&sf, *(v->p_value), "%s.%s", prefix,
				    v->name);
  }

  message_options_load(&sf, prefix);
  
  /* Players dialog */
  for(i = 1; i < num_player_dlg_columns; i++) {
    bool *show = &(player_dlg_columns[i].show);
    *show = secfile_lookup_bool_default(&sf, *show, "%s.player_dlg_%s", prefix,
                                        player_dlg_columns[i].tagname);
  }
  
  /* Load cma presets. If cma.number_of_presets doesn't exist, don't load 
   * any, the order here should be reversed to keep the order the same */
  num = secfile_lookup_int_default(&sf, -1, "cma.number_of_presets");
  if (num == -1) {
    create_default_cma_presets();
  } else {
    for (i = num - 1; i >= 0; i--) {
      load_cma_preset(&sf, i);
    }
  }
 
  section_file_free(&sf);
}

/****************************************************************
 this loads from the rc file any options which need to know what the 
 current ruleset is. It's called the first time client goes into
 CLIENT_GAME_RUNNING_STATE
*****************************************************************/
void load_ruleset_specific_options(void)
{
  struct section_file sf;
  char *name;
  int i;

  name = option_file_name();
  if (!name) {
    /* fail silently */
    return;
  }
  if (!section_file_load(&sf, name))
    return;

  /* load global worklists */
  for (i = 0; i < MAX_NUM_WORKLISTS; i++) {
    game.player_ptr->worklists[i].is_valid =
	secfile_lookup_bool_default(&sf, FALSE,
				    "worklists.worklist%d.is_valid", i);
    strcpy(game.player_ptr->worklists[i].name,
           secfile_lookup_str_default(&sf, "",
                                      "worklists.worklist%d.name", i));
    load_global_worklist(&sf, "worklists.worklist%d", i, 
                         &(game.player_ptr->worklists[i]));
  }

  /* Load city report columns (which include some ruleset data). */
  for (i = 1; i < num_city_report_spec(); i++) {
    bool *ip = city_report_spec_show_ptr(i);

    *ip = secfile_lookup_bool_default(&sf, *ip, "client.city_report_%s",
				     city_report_spec_tagname(i));
  }

  section_file_free(&sf);
}

/****************************************************************
... 
*****************************************************************/
void save_options(void)
{
  struct section_file sf;
  char *name = option_file_name();
  char output_buffer[256];
  view_option *v;
  int i;

  if(!name) {
    append_output_window(_("Save failed, cannot find a filename."));
    return;
  }

  section_file_init(&sf);
  secfile_insert_str(&sf, VERSION_STRING, "client.version");

  secfile_insert_bool(&sf, save_options_on_exit, "client.save_options_on_exit");
  secfile_insert_bool(&sf, fullscreen_mode, "client.fullscreen_mode");

  for (i = 0; i < num_options; i++) {
    client_option *o = options + i;

    switch (o->type) {
    case COT_BOOL:
      secfile_insert_bool(&sf, *(o->p_bool_value), "client.%s", o->name);
      break;
    case COT_INT:
      secfile_insert_int(&sf, *(o->p_int_value), "client.%s", o->name);
      break;
    case COT_STR:
      secfile_insert_str(&sf, o->p_string_value, "client.%s", o->name);
      break;
    }
  }

  for (v = view_options; v->name; v++) {
    secfile_insert_bool(&sf, *(v->p_value), "client.%s", v->name);
  }

  message_options_save(&sf, "client");

  for (i = 1; i < num_city_report_spec(); i++) {
    secfile_insert_bool(&sf, *(city_report_spec_show_ptr(i)),
		       "client.city_report_%s",
		       city_report_spec_tagname(i));
  }
  
  /* Players dialog */
  for (i = 1; i < num_player_dlg_columns; i++) {
    secfile_insert_bool(&sf, player_dlg_columns[i].show,
                        "client.player_dlg_%s",
                        player_dlg_columns[i].tagname);
  }
  
  /* insert global worklists */
  for(i = 0; i < MAX_NUM_WORKLISTS; i++){
    if (game.player_ptr->worklists[i].is_valid) {
      secfile_insert_bool(&sf, game.player_ptr->worklists[i].is_valid,
			  "worklists.worklist%d.is_valid", i);
      secfile_insert_str(&sf, game.player_ptr->worklists[i].name,
                         "worklists.worklist%d.name", i);
      save_global_worklist(&sf, "worklists.worklist%d", i, 
                           &(game.player_ptr->worklists[i]));
    }
  }


  /* insert cma presets */
  secfile_insert_int_comment(&sf, cmafec_preset_num(),
			     _("If you add a preset by "
			       "hand, also update \"number_of_presets\""),
			     "cma.number_of_presets");
  for (i = 0; i < cmafec_preset_num(); i++) {
    save_cma_preset(&sf, cmafec_preset_get_descr(i),
		    cmafec_preset_get_parameter(i), i);
  }

  /* save to disk */
  if (!section_file_save(&sf, name, 0)) {
    my_snprintf(output_buffer, sizeof(output_buffer),
		_("Save failed, cannot write to file %s"), name);
  } else {
    my_snprintf(output_buffer, sizeof(output_buffer),
		_("Saved settings to file %s"), name);
  }

  append_output_window(output_buffer);
  section_file_free(&sf);
}

/****************************************************************
 Does heavy lifting for looking up a preset.
*****************************************************************/
static void load_cma_preset(struct section_file *file, int inx)
{
  struct cm_parameter parameter;
  const char *name;

  name = secfile_lookup_str_default(file, "preset", 
				    "cma.preset%d.name", inx);
  output_type_iterate(i) {
    parameter.minimal_surplus[i] =
	secfile_lookup_int_default(file, 0, "cma.preset%d.minsurp%d", inx, i);
    parameter.factor[i] =
	secfile_lookup_int_default(file, 0, "cma.preset%d.factor%d", inx, i);
  } output_type_iterate_end;
  parameter.require_happy =
      secfile_lookup_bool_default(file, FALSE, "cma.preset%d.reqhappy", inx);
  parameter.happy_factor =
      secfile_lookup_int_default(file, 0, "cma.preset%d.happyfactor", inx);
  parameter.allow_disorder = FALSE;
  parameter.allow_specialists = TRUE;

  cmafec_preset_add(name, &parameter);
}

/****************************************************************
 Does heavy lifting for inserting a preset.
*****************************************************************/
static void save_cma_preset(struct section_file *file, char *name,
			    const struct cm_parameter *const pparam,
			    int inx)
{
  secfile_insert_str(file, name, "cma.preset%d.name", inx);
  output_type_iterate(i) {
    secfile_insert_int(file, pparam->minimal_surplus[i],
		       "cma.preset%d.minsurp%d", inx, i);
    secfile_insert_int(file, pparam->factor[i],
		       "cma.preset%d.factor%d", inx, i);
  } output_type_iterate_end;
  secfile_insert_bool(file, pparam->require_happy,
		      "cma.preset%d.reqhappy", inx);
  secfile_insert_int(file, pparam->happy_factor,
		     "cma.preset%d.happyfactor", inx);
}

/****************************************************************
 loads global worklist from rc file
*****************************************************************/
static void load_global_worklist(struct section_file *file, const char *path,
				 int wlinx, struct worklist *pwl)
{
  char efpath[64];
  char idpath[64];
  int i;
  bool end = FALSE;

  sz_strlcpy(efpath, path);
  sz_strlcat(efpath, ".wlef%d");
  sz_strlcpy(idpath, path);
  sz_strlcat(idpath, ".wlid%d");

  for (i = 0; i < MAX_LEN_WORKLIST; i++) {
    if (end) {
      pwl->wlefs[i] = WEF_END;
      pwl->wlids[i] = 0;
      section_file_lookup(file, efpath, wlinx, i);
      section_file_lookup(file, idpath, wlinx, i);
    } else {
      pwl->wlefs[i] =
        secfile_lookup_int_default(file, WEF_END, efpath, wlinx, i);
      pwl->wlids[i] =
        secfile_lookup_int_default(file, 0, idpath,wlinx, i);

      if ((pwl->wlefs[i] <= WEF_END) || (pwl->wlefs[i] >= WEF_LAST) ||
          (pwl->wlefs[i] == WEF_UNIT
	   && (pwl->wlids[i] < 0 || pwl->wlids[i] >= game.control.num_unit_types))
	   || ((pwl->wlefs[i] == WEF_IMPR)
	       && !improvement_exists(pwl->wlids[i]))) {
        pwl->wlefs[i] = WEF_END;
        pwl->wlids[i] = 0;
        end = TRUE;
      }
    }
  }
}

/****************************************************************
 saves global worklist to rc file
*****************************************************************/
static void save_global_worklist(struct section_file *file, const char *path,
				 int wlinx, struct worklist *pwl)
{
  char efpath[64];
  char idpath[64];
  int i;

  sz_strlcpy(efpath, path);
  sz_strlcat(efpath, ".wlef%d");
  sz_strlcpy(idpath, path);
  sz_strlcat(idpath, ".wlid%d");

  for (i = 0; i < MAX_LEN_WORKLIST; i++) {
    secfile_insert_int(file, pwl->wlefs[i], efpath, wlinx, i);
    secfile_insert_int(file, pwl->wlids[i], idpath, wlinx, i);
  }
}

/****************************************************************************
  Callback when a mapview graphics option is changed (redraws the canvas).
****************************************************************************/
void mapview_redraw_callback(struct client_option *option)
{
  update_map_canvas_visible();
}
