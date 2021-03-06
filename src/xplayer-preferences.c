/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2001,2002,2003 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 *
 * The Xplayer project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Xplayer. This
 * permission are above and beyond the permissions granted by the GPL license
 * Xplayer is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add exception clause.
 * See license_change file for details.
 *
 */

#include <config.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "bacon-video-widget-enums.h"
#include "xplayer.h"
#include "xplayer-private.h"
#include "xplayer-preferences.h"
#include "xplayer-interface.h"
#include "video-utils.h"
#include "xplayer-subtitle-encoding.h"
#include "xplayer-plugins-engine.h"

#define PWID(x) (GtkWidget *) gtk_builder_get_object (xplayer->prefs_xml, x)
#define POBJ(x) gtk_builder_get_object (xplayer->prefs_xml, x)

/* Callback functions for GtkBuilder */
G_MODULE_EXPORT void checkbutton2_toggled_cb (GtkToggleButton *togglebutton, Xplayer *xplayer);
G_MODULE_EXPORT void audio_screensaver_button_toggled_cb (GtkToggleButton *togglebutton, Xplayer *xplayer);
G_MODULE_EXPORT void visual_menu_changed (GtkComboBox *combobox, Xplayer *xplayer);
G_MODULE_EXPORT void tpw_color_reset_clicked_cb (GtkButton *button, Xplayer *xplayer);
G_MODULE_EXPORT void font_set_cb (GtkFontButton * fb, Xplayer * xplayer);
G_MODULE_EXPORT void encoding_set_cb (GtkComboBox *cb, Xplayer *xplayer);

static void
xplayer_prefs_set_show_visuals (Xplayer *xplayer, gboolean value)
{
	GtkWidget *item;

	g_settings_set_boolean (xplayer->settings, "show-visualizations", value);

	item = PWID ("tpw_visuals_type_label");
	gtk_widget_set_sensitive (item, value);
	item = PWID ("tpw_visuals_type_combobox");
	gtk_widget_set_sensitive (item, value);
	item = PWID ("tpw_visuals_size_label");
	gtk_widget_set_sensitive (item, value);
	item = PWID ("tpw_visuals_size_combobox");
	gtk_widget_set_sensitive (item, value);

	bacon_video_widget_set_show_visualizations
		(BACON_VIDEO_WIDGET (xplayer->bvw), value);
}

void
checkbutton2_toggled_cb (GtkToggleButton *togglebutton, Xplayer *xplayer)
{
	gboolean value;

	value = gtk_toggle_button_get_active (togglebutton);
	xplayer_prefs_set_show_visuals (xplayer, value);
}

void
audio_screensaver_button_toggled_cb (GtkToggleButton *togglebutton, Xplayer *xplayer)
{
	gboolean value;

	value = gtk_toggle_button_get_active (togglebutton);
	g_settings_set_boolean (xplayer->settings, "lock-screensaver-on-audio", value);
}

static void
show_vfx_changed_cb (GSettings *settings, const gchar *key, XplayerObject *xplayer)
{
	GObject *item;

	item = POBJ ("tpw_visuals_checkbutton");
	g_signal_handlers_disconnect_by_func (item,
			checkbutton2_toggled_cb, xplayer);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), g_settings_get_boolean (xplayer->settings, "show-visualizations"));

	g_signal_connect (item, "toggled",
			G_CALLBACK (checkbutton2_toggled_cb), xplayer);
}

static void
disable_kbd_shortcuts_changed_cb (GSettings *settings, const gchar *key, XplayerObject *xplayer)
{
	xplayer->disable_kbd_shortcuts = g_settings_get_boolean (xplayer->settings, "disable-keyboard-shortcuts");
}

static void
lock_screensaver_on_audio_changed_cb (GSettings *settings, const gchar *key, XplayerObject *xplayer)
{
	GObject *item, *radio;
	gboolean value;

	item = POBJ ("tpw_audio_toggle_button");
	g_signal_handlers_disconnect_by_func (item,
					      audio_screensaver_button_toggled_cb, xplayer);

	value = g_settings_get_boolean (xplayer->settings, "lock-screensaver-on-audio");
	if (value != FALSE) {
		radio = POBJ ("tpw_audio_toggle_button");
	} else {
		radio = POBJ ("tpw_video_toggle_button");
	}
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

	g_signal_connect (item, "toggled",
			  G_CALLBACK (audio_screensaver_button_toggled_cb), xplayer);
}

void
visual_menu_changed (GtkComboBox *combobox, Xplayer *xplayer)
{
	GList *list;
	const gchar *name;
	int i;

	i = gtk_combo_box_get_active (combobox);
	list = bacon_video_widget_get_visualization_list (xplayer->bvw);
	name = g_list_nth_data (list, i);

	g_settings_set_string (xplayer->settings, "visualization-name", name);
	bacon_video_widget_set_visualization (xplayer->bvw, name);
}

void
tpw_color_reset_clicked_cb (GtkButton *button, Xplayer *xplayer)
{
	guint i;
	const char *scales[] = {
		"tpw_bright_scale",
		"tpw_contrast_scale",
		"tpw_saturation_scale",
		"tpw_hue_scale"
	};

	for (i = 0; i < G_N_ELEMENTS (scales); i++) {
		GtkRange *item;
		item = GTK_RANGE (POBJ (scales[i]));
		gtk_range_set_value (item, 65535/2);
	}
}

void
font_set_cb (GtkFontButton * fb, Xplayer * xplayer)
{
	const gchar *font;

	font = gtk_font_button_get_font_name (fb);
	g_settings_set_string (xplayer->settings, "subtitle-font", font);
}

void
encoding_set_cb (GtkComboBox *cb, Xplayer *xplayer)
{
	const gchar *encoding;

	encoding = xplayer_subtitle_encoding_get_selected (cb);
	if (encoding)
		g_settings_set_string (xplayer->settings, "subtitle-encoding", encoding);
}

static void
font_changed_cb (GSettings *settings, const gchar *key, XplayerObject *xplayer)
{
	gchar *font;
	GtkFontButton *item;

	item = GTK_FONT_BUTTON (POBJ ("font_sel_button"));
	font = g_settings_get_string (settings, "subtitle-font");
	gtk_font_button_set_font_name (item, font);
	bacon_video_widget_set_subtitle_font (xplayer->bvw, font);
	g_free (font);
}

static void
encoding_changed_cb (GSettings *settings, const gchar *key, XplayerObject *xplayer)
{
	gchar *encoding;
	GtkComboBox *item;

	item = GTK_COMBO_BOX (POBJ ("subtitle_encoding_combo"));
	encoding = g_settings_get_string (settings, "subtitle-encoding");
	xplayer_subtitle_encoding_set (item, encoding);
	bacon_video_widget_set_subtitle_encoding (xplayer->bvw, encoding);
	g_free (encoding);
}

static gboolean
int_enum_get_mapping (GValue *value, GVariant *variant, GEnumClass *enum_class)
{
	GEnumValue *enum_value;
	const gchar *nick;

	g_return_val_if_fail (G_IS_ENUM_CLASS (enum_class), FALSE);

	nick = g_variant_get_string (variant, NULL);
	enum_value = g_enum_get_value_by_nick (enum_class, nick);

	if (enum_value == NULL)
		return FALSE;

	g_value_set_int (value, enum_value->value);

	return TRUE;
}

static GVariant *
int_enum_set_mapping (const GValue *value, const GVariantType *expected_type, GEnumClass *enum_class)
{
	GEnumValue *enum_value;

	g_return_val_if_fail (G_IS_ENUM_CLASS (enum_class), NULL);

	enum_value = g_enum_get_value (enum_class, g_value_get_int (value));

	if (enum_value == NULL)
		return NULL;

	return g_variant_new_string (enum_value->value_nick);
}

static void
visualization_quality_writable_changed_cb (GSettings *settings, const gchar *key, XplayerObject *xplayer)
{
	gboolean writable, show_visualizations;

	if (strcmp (key, "visualization-quality") != 0)
		return;

	writable = g_settings_is_writable (settings, key);
	show_visualizations = g_settings_get_boolean (settings, "show-visualizations");

	/* Only enable the size combobox if the visualization-quality setting is writable, and visualizations are enabled */
	gtk_widget_set_sensitive (PWID ("tpw_visuals_size_combobox"), writable && show_visualizations);
}

static void
prefer_dark_theme_changed_cb (GSettings *settings,
							  const gchar *key,
							  XplayerObject *xplayer) {

	GtkSettings *gtk_settings;
	gboolean prefer_dark_theme;

	g_return_if_fail (0 == strcmp (key, "prefer-dark-theme"));

	prefer_dark_theme = g_settings_get_boolean (settings, "prefer-dark-theme");
	gtk_settings = gtk_settings_get_default ();

	g_object_set (G_OBJECT (gtk_settings), "gtk-application-prefer-dark-theme", prefer_dark_theme, NULL);

}

void
xplayer_setup_preferences (Xplayer *xplayer)
{
	GtkWidget *menu, *content_area, *bvw;
	gboolean show_visuals, lock_screensaver_on_audio;
	guint i, hidden;
	char *visual, *font, *encoding;
	GList *list, *l;
	GtkWidget *widget;
	GObject *item;

	static struct {
		const char *name;
		BvwVideoProperty prop;
		const char *label;
		const gchar *key;
		const gchar *adjustment;
	} props[4] = {
		{ "tpw_contrast_scale", BVW_VIDEO_CONTRAST, "tpw_contrast_label", "contrast", "tpw_contrast_adjustment" },
		{ "tpw_saturation_scale", BVW_VIDEO_SATURATION, "tpw_saturation_label", "saturation", "tpw_saturation_adjustment" },
		{ "tpw_bright_scale", BVW_VIDEO_BRIGHTNESS, "tpw_brightness_label", "brightness", "tpw_bright_adjustment" },
		{ "tpw_hue_scale", BVW_VIDEO_HUE, "tpw_hue_label", "hue", "tpw_hue_adjustment" }
	};

	g_return_if_fail (xplayer->settings != NULL);

	bvw = xplayer_get_video_widget (xplayer);

	/* Work-around builder dialogue not parenting properly for
	 * On top windows */
	widget = PWID ("tpw_notebook");
	xplayer->prefs = gtk_dialog_new_with_buttons (_("Preferences"),
			GTK_WINDOW (xplayer->win),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE,
			GTK_RESPONSE_ACCEPT,
			NULL);
	gtk_container_set_border_width (GTK_CONTAINER (xplayer->prefs), 5);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (xplayer->prefs));
	gtk_box_set_spacing (GTK_BOX (content_area), 2);
	gtk_widget_reparent (widget, content_area);
	gtk_widget_show_all (content_area);
	widget = PWID ("xplayer_preferences_window");
	gtk_widget_destroy (widget);

	g_signal_connect (G_OBJECT (xplayer->prefs), "response",
			G_CALLBACK (gtk_widget_hide), NULL);
	g_signal_connect (G_OBJECT (xplayer->prefs), "delete-event",
			G_CALLBACK (gtk_widget_hide_on_delete), NULL);
        g_signal_connect (xplayer->prefs, "destroy",
                          G_CALLBACK (gtk_widget_destroyed), &xplayer->prefs);

	/* Remember position */
	item = POBJ ("tpw_remember_position_checkbutton");
	g_settings_bind (xplayer->settings, "remember-position", item, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (xplayer->settings, "remember-position", xplayer, "remember-position", G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	/* Auto-resize */
	item = POBJ ("tpw_display_checkbutton");
	g_settings_bind (xplayer->settings, "auto-resize", item, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (xplayer->settings, "auto-resize", bvw, "auto-resize", G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	/* Screensaver audio locking */
	lock_screensaver_on_audio = g_settings_get_boolean (xplayer->settings, "lock-screensaver-on-audio");
	if (lock_screensaver_on_audio != FALSE)
		item = POBJ ("tpw_audio_toggle_button");
	else
		item = POBJ ("tpw_video_toggle_button");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), TRUE);
	g_signal_connect (xplayer->settings, "changed::lock-screensaver-on-audio", (GCallback) lock_screensaver_on_audio_changed_cb, xplayer);

	/* Disable deinterlacing */
	item = POBJ ("tpw_no_deinterlace_checkbutton");
	g_settings_bind (xplayer->settings, "disable-deinterlacing", item, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (xplayer->settings, "disable-deinterlacing", bvw, "deinterlacing",
	                 G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY | G_SETTINGS_BIND_INVERT_BOOLEAN);

	/* Prefer dark theme */
	item = POBJ ("tpw_prefer_dark_theme_checkbutton");
	g_settings_bind (xplayer->settings, "prefer-dark-theme", item, "active", G_SETTINGS_BIND_DEFAULT);
	g_signal_connect(xplayer->settings, "changed::prefer-dark-theme", (GCallback) prefer_dark_theme_changed_cb, xplayer);

	/* Enable visuals */
	item = POBJ ("tpw_visuals_checkbutton");
	show_visuals = g_settings_get_boolean (xplayer->settings, "show-visualizations");

	g_signal_handlers_disconnect_by_func (item, checkbutton2_toggled_cb, xplayer);
	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (item), show_visuals);
	xplayer_prefs_set_show_visuals (xplayer, show_visuals);
	g_signal_connect (item, "toggled", G_CALLBACK (checkbutton2_toggled_cb), xplayer);

	g_signal_connect (xplayer->settings, "changed::show-visualizations", (GCallback) show_vfx_changed_cb, xplayer);

	/* Auto-load subtitles */
	item = POBJ ("tpw_auto_subtitles_checkbutton");
	g_settings_bind (xplayer->settings, "autoload-subtitles", item, "active", G_SETTINGS_BIND_DEFAULT);

	/* Auto-load external chapters */
	item = POBJ ("tpw_auto_chapters_checkbutton");
	g_settings_bind (xplayer->settings, "autoload-chapters", item, "active", G_SETTINGS_BIND_DEFAULT);

	/* Visuals list */
	list = bacon_video_widget_get_visualization_list (xplayer->bvw);
	menu = gtk_menu_new ();
	gtk_widget_show (menu);

	visual = g_settings_get_string (xplayer->settings, "visualization-name");
	if (*visual == '\0') {
		g_free (visual);
		visual = g_strdup ("goom");
	}

	item = POBJ ("tpw_visuals_type_liststore");

	i = 0;
	for (l = list; l != NULL; l = l->next) {
		const char *name = l->data;
		GtkTreeIter iter;

		gtk_list_store_append (GTK_LIST_STORE (item), &iter);
		gtk_list_store_set (GTK_LIST_STORE (item), &iter,
				    0, name, -1);

		if (strcmp (name, visual) == 0) {
			GObject *combobox;

			combobox = POBJ ("tpw_visuals_type_combobox");
			gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), i);
		}

		i++;
	}
	g_free (visual);

	/* Visualisation quality. We have to bind the writability separately, as the sensitivity of the size combobox is also affected by whether
	 * visualizations are enabled. */
	item = POBJ ("tpw_visuals_size_combobox");
	g_settings_bind (xplayer->settings, "visualization-quality", bvw, "visualization-quality",
	                 G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);
	g_settings_bind_with_mapping (xplayer->settings, "visualization-quality", item, "active",
	                              G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY,
	                              (GSettingsBindGetMapping) int_enum_get_mapping, (GSettingsBindSetMapping) int_enum_set_mapping,
	                              g_type_class_ref (BVW_TYPE_VISUALIZATION_QUALITY), (GDestroyNotify) g_type_class_unref);
	g_signal_connect (xplayer->settings, "writable-changed::visualization-quality", (GCallback) visualization_quality_writable_changed_cb, xplayer);

	/* Brightness and all */
	hidden = 0;
	for (i = 0; i < G_N_ELEMENTS (props); i++) {
		int prop_value;

		item = POBJ (props[i].adjustment);
		g_settings_bind (xplayer->settings, props[i].key, item, "value", G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (xplayer->settings, props[i].key, bvw, props[i].key, G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

		prop_value = bacon_video_widget_get_video_property (xplayer->bvw, props[i].prop);
		if (prop_value < 0) {
			/* The property's unsupported, so hide the widget and its label */
			item = POBJ (props[i].name);
			gtk_range_set_value (GTK_RANGE (item), (gdouble) 65535/2);
			gtk_widget_hide (GTK_WIDGET (item));
			item = POBJ (props[i].label);
			gtk_widget_hide (GTK_WIDGET (item));
			hidden++;
		}
	}

	/* If all the properties have been hidden, hide their section box */
	if (hidden == G_N_ELEMENTS (props)) {
		item = POBJ ("tpw_bright_contr_vbox");
		gtk_widget_hide (GTK_WIDGET (item));
	}

	/* Sound output type */
	item = POBJ ("tpw_sound_output_combobox");
	g_settings_bind (xplayer->settings, "audio-output-type", bvw, "audio-output-type",
	                 G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);
	g_settings_bind_with_mapping (xplayer->settings, "audio-output-type", item, "active", G_SETTINGS_BIND_DEFAULT,
	                              (GSettingsBindGetMapping) int_enum_get_mapping, (GSettingsBindSetMapping) int_enum_set_mapping,
	                              g_type_class_ref (BVW_TYPE_AUDIO_OUTPUT_TYPE), (GDestroyNotify) g_type_class_unref);

	/* Subtitle font selection */
	item = POBJ ("font_sel_button");
	gtk_font_button_set_title (GTK_FONT_BUTTON (item),
				   _("Select Subtitle Font"));
	font = g_settings_get_string (xplayer->settings, "subtitle-font");
	if (*font != '\0') {
		gtk_font_button_set_font_name (GTK_FONT_BUTTON (item), font);
		bacon_video_widget_set_subtitle_font (xplayer->bvw, font);
	}
	g_free (font);
	g_signal_connect (xplayer->settings, "changed::subtitle-font", (GCallback) font_changed_cb, xplayer);

	/* Subtitle encoding selection */
	item = POBJ ("subtitle_encoding_combo");
	xplayer_subtitle_encoding_init (GTK_COMBO_BOX (item));
	encoding = g_settings_get_string (xplayer->settings, "subtitle-encoding");
	/* Make sure the default is UTF-8 */
	if (*encoding == '\0') {
		g_free (encoding);
		encoding = g_strdup ("UTF-8");
	}
	xplayer_subtitle_encoding_set (GTK_COMBO_BOX(item), encoding);
	if (encoding && strcasecmp (encoding, "") != 0) {
		bacon_video_widget_set_subtitle_encoding (xplayer->bvw, encoding);
	}
	g_free (encoding);
	g_signal_connect (xplayer->settings, "changed::subtitle-encoding", (GCallback) encoding_changed_cb, xplayer);

	/* Disable keyboard shortcuts */
	xplayer->disable_kbd_shortcuts = g_settings_get_boolean (xplayer->settings, "disable-keyboard-shortcuts");
	g_signal_connect (xplayer->settings, "changed::disable-keyboard-shortcuts", (GCallback) disable_kbd_shortcuts_changed_cb, xplayer);

	g_object_unref (bvw);
}

void
xplayer_preferences_visuals_setup (Xplayer *xplayer)
{
	char *visual;

	visual = g_settings_get_string (xplayer->settings, "visualization-name");
	if (*visual == '\0') {
		g_free (visual);
		visual = g_strdup ("goom");
	}

	bacon_video_widget_set_visualization (xplayer->bvw, visual);
	g_free (visual);
}
