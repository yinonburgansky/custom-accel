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

#include <adwaita.h>
#include "device-manager.h"

G_BEGIN_DECLS

#define APPLY_ACCEL_SETTINGS_DIALOG_TYPE (apply_accel_settings_dialog_get_type())
G_DECLARE_FINAL_TYPE(ApplyAccelSettingsDialog, apply_accel_settings_dialog, APPLY, ACCEL_SETTINGS_DIALOG, AdwAlertDialog)

ApplyAccelSettingsDialog *apply_accel_settings_dialog_new(DeviceManager *device_manager);

G_END_DECLS
