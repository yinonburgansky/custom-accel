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

#include "apply-accel-settings-dialog.h"
#include "device-manager.h"
#include <adwaita.h>

struct _ApplyAccelSettingsDialog
{
    AdwAlertDialog parent_instance;
    DeviceManager *device_manager;
    int timer_seconds;
    guint update_timer_id;
    guint restore_timer_id;
};
G_DEFINE_TYPE(ApplyAccelSettingsDialog, apply_accel_settings_dialog, ADW_TYPE_ALERT_DIALOG)

static void remove_timeouts(ApplyAccelSettingsDialog *dialog)
{
    if (dialog->update_timer_id > 0)
    {
        g_source_remove(dialog->update_timer_id);
        dialog->update_timer_id = 0;
    }
    if (dialog->restore_timer_id > 0)
    {
        g_source_remove(dialog->restore_timer_id);
        dialog->restore_timer_id = 0;
    }
}

static gboolean on_timeout_restore_settings(gpointer user_data)
{
    ApplyAccelSettingsDialog *dialog = APPLY_ACCEL_SETTINGS_DIALOG(user_data);
    device_manager_restore_accel_settings(dialog->device_manager);
    remove_timeouts(dialog);
    adw_dialog_close(ADW_DIALOG(dialog));
    return FALSE; // Stop the timeout
}

static void update_timer_label(ApplyAccelSettingsDialog *dialog)
{
    adw_alert_dialog_format_body(ADW_ALERT_DIALOG(dialog),
                                 "The acceleration settings will be restored in %d seconds.",
                                 dialog->timer_seconds);
}

static gboolean on_update_timer_label(gpointer user_data)
{
    ApplyAccelSettingsDialog *dialog = APPLY_ACCEL_SETTINGS_DIALOG(user_data);
    if (dialog->timer_seconds > 0)
    {
        dialog->timer_seconds--;
        update_timer_label(dialog);
        return TRUE; // Continue the timeout
    }
    return FALSE; // Stop the timeout
}

static void on_dialog_response(AdwAlertDialog *dialog, const char *response_id, gpointer user_data)
{
    ApplyAccelSettingsDialog *self = APPLY_ACCEL_SETTINGS_DIALOG(user_data);
    if (g_strcmp0(response_id, "restore") == 0)
    {
        device_manager_restore_accel_settings(self->device_manager);
    }
    remove_timeouts(self);
    adw_dialog_close(ADW_DIALOG(self));
}

static void apply_accel_settings_dialog_class_init(ApplyAccelSettingsDialogClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(widget_class, "/io/github/yinonburgansky/CustomAccel/apply-accel-settings-dialog.ui");
}

static void apply_accel_settings_dialog_init(ApplyAccelSettingsDialog *self)
{
    gtk_widget_init_template(GTK_WIDGET(self));
    self->timer_seconds = 10;
    self->update_timer_id = 0;
    self->restore_timer_id = 0;
    self->update_timer_id = g_timeout_add_seconds(1, on_update_timer_label, self);
    self->restore_timer_id = g_timeout_add_seconds(10, on_timeout_restore_settings, self);
    g_signal_connect(self, "response", G_CALLBACK(on_dialog_response), self);
    update_timer_label(self);
}

ApplyAccelSettingsDialog *apply_accel_settings_dialog_new(DeviceManager *device_manager)
{
    ApplyAccelSettingsDialog *dialog = g_object_new(APPLY_ACCEL_SETTINGS_DIALOG_TYPE, NULL);
    dialog->device_manager = device_manager;
    return dialog;
}
