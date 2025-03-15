/* Copyright 2025 Yinon Burgansky
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"
#include "custom-accel-window.h"
#include "device-manager.h"
#include "plot-widget.h"
#include "bezier-curve.c"
#include "apply-accel-settings-dialog.h"
#include "x11-accel-settings-manager.c"

#include <adwaita.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>

struct _CustomAccelWindow
{
	AdwApplicationWindow parent_instance;

	/* Template widgets */
	PlotWidget *plot_widget;
	GtkDropDown *device_dropdown;
	GtkCheckButton *movement_type_button;
	GtkCheckButton *scroll_movement_type_button;
	GtkScale *y_axis_multiplier_scale;
	GtkButton *apply_accel_button;
	Curve *curve;
	DeviceManager *device_manager;
};

G_DEFINE_FINAL_TYPE(CustomAccelWindow, custom_accel_window, ADW_TYPE_APPLICATION_WINDOW)

static void
custom_accel_window_class_init(CustomAccelWindowClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	// Register the PlotWidget type
	g_type_ensure(PLOT_TYPE_WIDGET);

	gtk_widget_class_set_template_from_resource(widget_class, "/io/github/yinonburgansky/CustomAccel/custom-accel-window.ui");
	gtk_widget_class_bind_template_child(widget_class, CustomAccelWindow, plot_widget);
	gtk_widget_class_bind_template_child(widget_class, CustomAccelWindow, device_dropdown);
	gtk_widget_class_bind_template_child(widget_class, CustomAccelWindow, movement_type_button);
	gtk_widget_class_bind_template_child(widget_class, CustomAccelWindow, scroll_movement_type_button);
	gtk_widget_class_bind_template_child(widget_class, CustomAccelWindow, y_axis_multiplier_scale);
	gtk_widget_class_bind_template_child(widget_class, CustomAccelWindow, apply_accel_button);
}

static void update_y_axis_top_value(CustomAccelWindow *self)
{
	double x_axis_top_value = plot_widget_get_x_axis_top_value(self->plot_widget);
	double multiplier = gtk_range_get_value(GTK_RANGE(self->y_axis_multiplier_scale));
	plot_widget_set_y_axis_top_value(self->plot_widget, x_axis_top_value * multiplier);
}

static void on_y_axis_multiplier_value_changed(GtkRange *range, gpointer user_data)
{
	CustomAccelWindow *self = CUSTOM_ACCEL_WINDOW(user_data);
	update_y_axis_top_value(self);
}

static void on_speed(double speed_unaccel, gpointer user_data)
{
	CustomAccelWindow *self = CUSTOM_ACCEL_WINDOW(user_data);
	PlotWidget *plot_widget = self->plot_widget;
	plot_widget_set_current_x_value(plot_widget, speed_unaccel);
	if (speed_unaccel > plot_widget_get_x_axis_top_value(plot_widget))
	{
		plot_widget_set_x_axis_top_value(plot_widget, speed_unaccel);
		update_y_axis_top_value(self);
	}
}

static void reset_plot_widget_axis_values(CustomAccelWindow *self)
{
	// reset top value and speed
	plot_widget_set_x_axis_top_value(self->plot_widget, 1.0);
	update_y_axis_top_value(self);
	plot_widget_set_current_x_value(self->plot_widget, 0.0);
}

static void on_device_dropdown_changed(GtkDropDown *dropdown, GParamSpec *spec, gpointer user_data)
{
	CustomAccelWindow *self = CUSTOM_ACCEL_WINDOW(user_data);
	reset_plot_widget_axis_values(self);

	int selected = gtk_drop_down_get_selected(dropdown);
	if (selected == 0)
	{
		device_manager_set_current_device(self->device_manager, NULL);
	}
	else
	{
		const char *device_name = gtk_string_list_get_string(GTK_STRING_LIST(gtk_drop_down_get_model(dropdown)), selected);
		device_manager_set_current_device(self->device_manager, device_name);
	}
}

static void on_apply_accel_button_clicked(GtkButton *button, gpointer user_data)
{
	CustomAccelWindow *self = CUSTOM_ACCEL_WINDOW(user_data);
	// set up a custom accel formula for the currently selected device
	double x_axis_top_value = plot_widget_get_x_axis_top_value(self->plot_widget);
	double y_axis_top_value = plot_widget_get_y_axis_top_value(self->plot_widget);
	CustomAccelFunction custom_accel_function = {0};
	custom_accel_function.npoints = 64;
	custom_accel_function.step = 1.0 / (custom_accel_function.npoints - 1);

	for (int i = 0; i < custom_accel_function.npoints; i++)
	{
		custom_accel_function.points[i] = self->curve->get_y_value(self->plot_widget, i * custom_accel_function.step) * y_axis_top_value;
	}
	custom_accel_function.step *= x_axis_top_value;

	if (!device_manager_set_custom_accel_function(self->device_manager, &custom_accel_function))
	{
		AdwDialog *dialog = adw_alert_dialog_new("Failed to set custom acceleration function for the selected device", NULL);
		adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
									   "cancel", "_Cancel",
									   NULL);
		adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "cancel");
		adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
		adw_dialog_present(dialog, GTK_WIDGET(self));
		return;
	}

	ApplyAccelSettingsDialog *dialog = apply_accel_settings_dialog_new(self->device_manager);
	adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(self));
}

static void custom_accel_window_set_movement_type(CustomAccelWindow *self, MovementType movement_type)
{
	device_manager_set_movement_type(self->device_manager, movement_type);
	reset_plot_widget_axis_values(self);
	switch (movement_type)
	{
	case MOVEMENT_TYPE_MOTION:
		plot_widget_set_x_axis_label(self->plot_widget, "Device Speed (u/ms)");
		plot_widget_set_y_axis_label(self->plot_widget, "Pointer Speed (px/ms)");
		break;
	case MOVEMENT_TYPE_SCROLL:
		plot_widget_set_x_axis_label(self->plot_widget, "Device Speed (u/ms)");
		plot_widget_set_y_axis_label(self->plot_widget, "Scroll Speed (px/ms)");
		break;
	default:
		g_assert_not_reached();
		break;
	}
}

static void on_movement_type_toggled(GtkCheckButton *button, gpointer user_data)
{
	CustomAccelWindow *self = CUSTOM_ACCEL_WINDOW(user_data);
	if (gtk_check_button_get_active(button))
	{
		custom_accel_window_set_movement_type(self, MOVEMENT_TYPE_MOTION);
	}
}

static void on_scroll_movement_type_toggled(GtkCheckButton *button, gpointer user_data)
{
	CustomAccelWindow *self = CUSTOM_ACCEL_WINDOW(user_data);
	if (gtk_check_button_get_active(button))
	{
		custom_accel_window_set_movement_type(self, MOVEMENT_TYPE_SCROLL);
	}
}

static void
custom_accel_window_init(CustomAccelWindow *self)
{
	gtk_widget_init_template(GTK_WIDGET(self));
	self->curve = bezier_curve_new();
	plot_widget_set_curve(self->plot_widget, self->curve);

	// Initialize device manager
	AccelSettingsManager *accel_settings_manager = x11_accel_settings_manager_new();
	self->device_manager = device_manager_new(accel_settings_manager);
	if (!self->device_manager)
	{
		g_warning("Failed to initialize device manager");
		return;
	}

	GtkStringList *device_names = device_manager_get_device_names(self->device_manager);
	if (!device_names)
	{
		g_warning("No input devices found");
		return;
	}

	GtkStringList *dropdown_model = GTK_STRING_LIST(gtk_drop_down_get_model(self->device_dropdown));
	for (int i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(device_names)); i++)
	{
		const char *device_name = gtk_string_list_get_string(device_names, i);
		gtk_string_list_append(dropdown_model, device_name);
	}

	device_manager_set_speed_callback(self->device_manager, on_speed, self);
	custom_accel_window_set_movement_type(self, MOVEMENT_TYPE_MOTION);

	g_signal_connect(self->device_dropdown, "notify::selected", G_CALLBACK(on_device_dropdown_changed), self);
	g_signal_connect(self->movement_type_button, "toggled", G_CALLBACK(on_movement_type_toggled), self);
	g_signal_connect(self->scroll_movement_type_button, "toggled", G_CALLBACK(on_scroll_movement_type_toggled), self);
	g_signal_connect(self->y_axis_multiplier_scale, "value-changed", G_CALLBACK(on_y_axis_multiplier_value_changed), self);
	g_signal_connect(self->apply_accel_button, "clicked", G_CALLBACK(on_apply_accel_button_clicked), self);
}
