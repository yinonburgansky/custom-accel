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

#include "device-manager.h"
#include <glib.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>
#include <stdio.h>

typedef struct _X11AccelSettingsManager
{
    AccelSettingsManager base;
    Display *display;
} X11AccelSettingsManager;

static gboolean set_property(Display *display, int device_id, Atom property, Atom type, int format,
                             unsigned char *data, int nelements)
{
    XIChangeProperty(display, device_id, property, type, format, XIPropModeReplace, data, nelements);
    XSync(display, False);
    return TRUE;
}

static gboolean get_property(Display *display, int device_id, Atom atom, Atom type, int format,
                             unsigned char **data, unsigned long *nitems)
{
    Atom actual_type;
    int actual_format;
    unsigned long bytes_after;
    int status = XIGetProperty(display, device_id, atom, 0, (~0L), False, type,
                               &actual_type, &actual_format, nitems, &bytes_after, data);
    if (status != Success || actual_type != type || actual_format != format)
    {
        XFree(*data);
        return FALSE;
    }
    return TRUE;
}

static gboolean set_atom_property_int8_array(Display *display, int device_id, Atom atom, uint8_t *values, int nvalues)
{
    return set_property(display, device_id, atom, XA_INTEGER, 8, (unsigned char *)values, nvalues);
}

static gboolean set_atom_property_double(Display *display, int device_id, Atom atom, double value)
{
    float float_value = (float)value;
    Atom float_atom = XInternAtom(display, "FLOAT", False);
    return set_property(display, device_id, atom, float_atom, 32, (unsigned char *)&float_value, 1);
}

static gboolean set_atom_property_double_array(Display *display, int device_id, Atom atom, double *values, int nvalues)
{
    float *float_values = g_new(float, nvalues);
    for (int i = 0; i < nvalues; i++)
    {
        float_values[i] = (float)values[i];
    }
    Atom float_atom = XInternAtom(display, "FLOAT", False);
    gboolean success = set_property(display, device_id, atom, float_atom, 32, (unsigned char *)float_values, nvalues);
    g_free(float_values);
    return success;
}

static gboolean get_atom_property_int8_array(Display *display, int device_id, Atom atom, uint8_t *array, int array_len, int *nitems)
{
    unsigned char *prop = NULL;
    unsigned long nitems_local;
    gboolean success = get_property(display, device_id, atom, XA_INTEGER, 8, &prop, &nitems_local);
    if (!success || !prop)
        return FALSE;

    g_assert(array_len >= nitems_local);
    *nitems = nitems_local;
    memcpy(array, prop, nitems_local * sizeof(uint8_t));
    XFree(prop);

    return success;
}

static gboolean get_atom_property_double_array(Display *display, int device_id, Atom atom, double *array, int array_len, int *nitems)
{
    unsigned char *prop = NULL;
    unsigned long nitems_local;
    Atom float_atom = XInternAtom(display, "FLOAT", False);
    gboolean success = get_property(display, device_id, atom, float_atom, 32, &prop, &nitems_local);
    if (!success || !prop)
        return FALSE;

    g_assert(array_len >= nitems_local);
    *nitems = nitems_local;
    float *float_values = (float *)prop;
    for (int i = 0; i < nitems_local; i++)
        array[i] = (double)float_values[i];

    XFree(prop);
    return success;
}

static gboolean get_atom_property_double(Display *display, int device_id, Atom atom, double *value)
{
    unsigned char *prop = NULL;
    unsigned long nitems;
    Atom float_atom = XInternAtom(display, "FLOAT", False);
    gboolean success = get_property(display, device_id, atom, float_atom, 32, &prop, &nitems);
    if (!success || !prop)
        return FALSE;

    float float_value = *((float *)prop);
    *value = (double)float_value;
    XFree(prop);
    return success;
}

static void x11_get_accel_function_atoms(Display *display, Atom *accel_points_atom, Atom *accel_step_atom,
                                         MovementType movement_type)
{
    char accel_points_atom_name[64];
    char accel_step_atom_name[64];

    snprintf(accel_points_atom_name, sizeof(accel_points_atom_name), "libinput Accel Custom %s Points", MOVEMENT_TYPE_STRINGS[movement_type]);
    snprintf(accel_step_atom_name, sizeof(accel_step_atom_name), "libinput Accel Custom %s Step", MOVEMENT_TYPE_STRINGS[movement_type]);

    *accel_points_atom = XInternAtom(display, accel_points_atom_name, True);
    *accel_step_atom = XInternAtom(display, accel_step_atom_name, True);
}

int x11_get_device_id(Display *display, Device *device)
{
    char *device_node = device->node;
    int ndevices;
    XIDeviceInfo *devices = XIQueryDevice(display, XIAllDevices, &ndevices);
    int device_id = -1;
    char *property_value;
    size_t nitems;
    Atom property_atom = XInternAtom(display, "Device Node", True);

    for (int i = 0; i < ndevices; i++)
    {
        if (get_property(display, devices[i].deviceid, property_atom, XA_STRING, 8, (unsigned char **)&property_value, &nitems))
        {
            if (g_strcmp0(property_value, device_node) == 0)
            {
                device_id = devices[i].deviceid;
                XFree(property_value);
                break;
            }
            XFree(property_value);
        }
    }

    XIFreeDeviceInfo(devices);
    return device_id;
}

static gboolean x11_set_accel_function(Display *display, int device_id, CustomAccelFunction *custom_accel_function, MovementType movement_type)
{
    Atom accel_points_atom;
    Atom accel_step_atom;
    x11_get_accel_function_atoms(display, &accel_points_atom, &accel_step_atom, movement_type);
    gboolean success = set_atom_property_double_array(display, device_id, accel_points_atom, custom_accel_function->points, custom_accel_function->npoints);
    success = success && set_atom_property_double(display, device_id, accel_step_atom, custom_accel_function->step);
    return success;
}

static gboolean x11_set_accel_settings(AccelSettingsManager *self, Device *device, AccelSettings *settings)
{
    X11AccelSettingsManager *x11_manager = (X11AccelSettingsManager *)self;
    Display *display = x11_manager->display;
    int device_id = x11_get_device_id(display, device);
    if (device_id == -1)
    {
        g_warning("Failed to get device id for device: %s", device->name);
        return FALSE;
    }
    Atom accel_profile_atom = XInternAtom(display, "libinput Accel Profile Enabled", True);
    gboolean success = set_atom_property_int8_array(display, device_id, accel_profile_atom, settings->profile, 3);

    for (int i = 0; i < MOVEMENT_TYPE_COUNT && success; i++)
    {
        CustomAccelFunction *custom_accel_function = &settings->custom_accel_functions[i];
        success = x11_set_accel_function(display, device_id, custom_accel_function, (MovementType)i);
    }

    return success;
}

static gboolean x11_get_accel_function(Display *display, int device_id, CustomAccelFunction *custom_accel_function, MovementType movement_type)
{
    Atom accel_points_atom;
    Atom accel_step_atom;
    x11_get_accel_function_atoms(display, &accel_points_atom, &accel_step_atom, movement_type);

    int points_size;
    gboolean success = get_atom_property_double_array(display, device_id, accel_points_atom, custom_accel_function->points, 64, &points_size);
    custom_accel_function->npoints = points_size;
    success = success && get_atom_property_double(display, device_id, accel_step_atom, &custom_accel_function->step);
    return success;
}

static gboolean x11_get_accel_settings(AccelSettingsManager *self, Device *device, AccelSettings *settings)
{
    X11AccelSettingsManager *x11_manager = (X11AccelSettingsManager *)self;
    Display *display = x11_manager->display;
    int device_id = x11_get_device_id(display, device);
    if (device_id == -1)
    {
        g_warning("Failed to get device id for device: %s", device->name);
        return FALSE;
    }
    Atom accel_profile_atom = XInternAtom(display, "libinput Accel Profile Enabled", True);
    int profile_size;
    gboolean success = get_atom_property_int8_array(display, device_id, accel_profile_atom, settings->profile, 3, &profile_size);

    for (int i = 0; i < MOVEMENT_TYPE_COUNT && success; i++)
    {
        CustomAccelFunction *custom_accel_function = &settings->custom_accel_functions[i];
        success = x11_get_accel_function(display, device_id, custom_accel_function, (MovementType)i);
    }

    return success;
}

void x11_accel_settings_manager_free(AccelSettingsManager *self)
{
    if (!self)
        return;
    X11AccelSettingsManager *x11_manager = (X11AccelSettingsManager *)self;
    if (x11_manager->display)
    {
        XCloseDisplay(x11_manager->display);
    }
    g_free(x11_manager);
}

AccelSettingsManager *x11_accel_settings_manager_new(void)
{
    X11AccelSettingsManager *manager = g_new0(X11AccelSettingsManager, 1);
    manager->base.free = x11_accel_settings_manager_free;
    manager->base.set_accel_settings = x11_set_accel_settings;
    manager->base.get_accel_settings = x11_get_accel_settings;
    manager->display = XOpenDisplay(NULL);
    if (!manager->display)
    {
        g_warning("Failed to open X display");
        g_free(manager);
        return NULL;
    }

    return (AccelSettingsManager *)manager;
}
