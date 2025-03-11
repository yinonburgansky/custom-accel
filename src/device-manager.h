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

#pragma once

#include <libinput.h>
#include <gtk/gtk.h>

typedef enum
{
    MOVEMENT_TYPE_MOTION,
    MOVEMENT_TYPE_SCROLL,
    MOVEMENT_TYPE_COUNT
} MovementType;

extern const char *MOVEMENT_TYPE_STRINGS[MOVEMENT_TYPE_COUNT];

typedef struct
{
    double step;
    int npoints;
    double points[64];
} CustomAccelFunction;

typedef struct
{
    uint8_t profile[3];
    CustomAccelFunction custom_accel_functions[MOVEMENT_TYPE_COUNT];
} AccelSettings;

typedef struct
{
    gchar *node;
    gchar *name;
    struct libinput_device *libinput_device;
    AccelSettings saved_accel_settings;
} Device;

typedef struct _AccelSettingsManager AccelSettingsManager;
struct _AccelSettingsManager
{
    void (*free)(AccelSettingsManager *self);
    gboolean (*set_accel_settings)(AccelSettingsManager *self, Device *device, AccelSettings *settings);
    gboolean (*get_accel_settings)(AccelSettingsManager *self, Device *device, AccelSettings *settings);
};

typedef struct _DeviceManager DeviceManager;

DeviceManager *device_manager_new(AccelSettingsManager *accel_settings_manager);
void device_manager_free(DeviceManager *manager);
GtkStringList *device_manager_get_device_names(DeviceManager *manager);
void device_manager_set_speed_callback(DeviceManager *manager, void (*on_speed)(double speed, gpointer user_data), gpointer user_data);
gboolean device_manager_set_custom_accel_function(DeviceManager *manager, CustomAccelFunction *custom_accel_function);
void device_manager_set_current_device(DeviceManager *manager, const char *device_name);
gboolean device_manager_restore_accel_settings(DeviceManager *manager);
void device_manager_set_movement_type(DeviceManager *manager, MovementType movement_type);
