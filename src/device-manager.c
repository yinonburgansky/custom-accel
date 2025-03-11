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
#include <libinput.h>
#include <libudev.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <assert.h>

Device *device_new(const gchar *node, const gchar *name)
{
    Device *device = g_new0(Device, 1);
    device->node = g_strdup(node);
    device->name = g_strdup(name);
    device->libinput_device = NULL;
    memset(&device->saved_accel_settings, 0, sizeof(AccelSettings));

    return device;
}

void device_free(Device *device)
{
    if (device)
    {
        g_free(device->node);
        g_free(device->name);
        g_free(device);
    }
}

struct _DeviceManager
{
    struct libinput *libinput_context;
    GIOChannel *gio_channel;
    GList *devices;
    Device *current_device;
    void (*on_speed)(double speed_unaccel, gpointer user_data);
    gpointer user_data;
    AccelSettingsManager *accel_settings_manager;
    MovementType movement_type;
};

static int open_restricted(const char *path, int flags, void *user_data)
{
    int fd = open(path, flags);
    if (fd < 0)
        g_warning("Failed to open %s: %s", path, strerror(errno));
    return fd;
}

static void close_restricted(int fd, void *user_data)
{
    close(fd);
}

static const struct libinput_interface libinput_interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

static void handle_motion(struct libinput *li, struct libinput_event *ev)
{
    DeviceManager *manager = libinput_get_user_data(li);
    if (manager->movement_type != MOVEMENT_TYPE_MOTION || !manager->on_speed)
        return;

    struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
    double dx_unaccel = libinput_event_pointer_get_dx_unaccelerated(p),
           dy_unaccel = libinput_event_pointer_get_dy_unaccelerated(p);

    static uint64_t last_time_usec = 0;
    uint64_t time_usec = libinput_event_pointer_get_time_usec(p);
    double dt_ms = (time_usec - last_time_usec) / 1000.0;
    last_time_usec = time_usec;
    if (dt_ms <= 0)
        return;
    if (dt_ms > 1000)
        dt_ms = 7;

    double speed_unaccel = hypot(dx_unaccel, dy_unaccel) / dt_ms;
    manager->on_speed(speed_unaccel, manager->user_data);
}

static void handle_scroll(struct libinput *li, struct libinput_event *ev)
{
    DeviceManager *manager = libinput_get_user_data(li);
    if (manager->movement_type != MOVEMENT_TYPE_SCROLL || !manager->on_speed)
        return;

    struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
    static uint64_t last_time_usec = 0;
    uint64_t time_usec = libinput_event_pointer_get_time_usec(p);
    double dt_ms = (time_usec - last_time_usec) / 1000.0;
    last_time_usec = time_usec;
    if (dt_ms <= 0)
        return;
    if (dt_ms > 1000)
        dt_ms = 7;

    double dx = 0, dy = 0;
    if (libinput_event_pointer_has_axis(p, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL))
        dx = libinput_event_pointer_get_scroll_value(p, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);

    if (libinput_event_pointer_has_axis(p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL))
        dy = libinput_event_pointer_get_scroll_value(p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);

    double speed_unaccel = hypot(dx, dy) / dt_ms;
    manager->on_speed(speed_unaccel, manager->user_data);
}

static gboolean handle_event_libinput(GIOChannel *source, GIOCondition condition, gpointer data)
{
    struct libinput *li = data;
    struct libinput_event *ev;
    libinput_dispatch(li);

    while ((ev = libinput_get_event(li)))
    {
        switch (libinput_event_get_type(ev))
        {
        case LIBINPUT_EVENT_NONE:
            abort();
        case LIBINPUT_EVENT_POINTER_MOTION:
            handle_motion(li, ev);
            break;
        case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
        case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
        case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS:
            handle_scroll(li, ev);
            break;
        default:
            break;
        }

        libinput_event_destroy(ev);
    }

    return TRUE;
}

DeviceManager *device_manager_new(AccelSettingsManager *accel_settings_manager)
{
    DeviceManager *manager = g_new0(DeviceManager, 1);
    manager->current_device = NULL;
    manager->movement_type = MOVEMENT_TYPE_MOTION;
    manager->accel_settings_manager = accel_settings_manager;

    manager->libinput_context = libinput_path_create_context(&libinput_interface, NULL);
    if (!manager->libinput_context)
    {
        g_warning("Failed to create libinput context");
        g_free(manager);
        return NULL;
    }

    manager->devices = NULL;
    struct udev *udev = udev_new();
    if (!udev)
    {
        g_warning("Failed to create udev context");
        libinput_unref(manager->libinput_context);
        g_free(manager);
        return NULL;
    }

    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    // udev_enumerate_add_match_property(enumerate, "ID_INPUT_MOUSE", "1");
    // udev_enumerate_add_match_property(enumerate, "ID_INPUT_TOUCHPAD", "1");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry *entry;

    udev_list_entry_foreach(entry, devices)
    {
        const char *path = udev_list_entry_get_name(entry);
        struct udev_device *udev_device = udev_device_new_from_syspath(udev, path);

        if (udev_device)
        {
            const char *devnode = udev_device_get_devnode(udev_device);
            if (!devnode)
            {
                udev_device_unref(udev_device);
                continue;
            }
            struct libinput_device *libinput_device = libinput_path_add_device(manager->libinput_context, devnode);
            if (!libinput_device)
            {
                // g_printerr("Failed to add libinput device: %s\n", devnode);
                udev_device_unref(udev_device);
                continue;
            }
            if (!libinput_device_has_capability(libinput_device, LIBINPUT_DEVICE_CAP_POINTER))
            {
                libinput_path_remove_device(libinput_device);
                udev_device_unref(udev_device);
                continue;
            }
            const char *device_name = libinput_device_get_name(libinput_device);
            g_print("Found device: %s, node: %s\n", device_name, devnode);
            Device *device = device_new(devnode, device_name);
            manager->devices = g_list_append(manager->devices, device);
            libinput_path_remove_device(libinput_device);
            udev_device_unref(udev_device);
        }
    }

    udev_enumerate_unref(enumerate);
    udev_unref(udev);

    libinput_set_user_data(manager->libinput_context, manager);

    manager->gio_channel = g_io_channel_unix_new(libinput_get_fd(manager->libinput_context));
    g_io_channel_set_encoding(manager->gio_channel, NULL, NULL);
    g_io_add_watch(manager->gio_channel, G_IO_IN, handle_event_libinput, manager->libinput_context);

    return manager;
}

void device_manager_free(DeviceManager *manager)
{
    if (manager)
    {
        if (manager->libinput_context)
            libinput_unref(manager->libinput_context);
        if (manager->devices)
            g_list_free_full(manager->devices, (GDestroyNotify)device_free);
        if (manager->gio_channel)
            g_io_channel_unref(manager->gio_channel);
        if (manager->accel_settings_manager)
            manager->accel_settings_manager->free(manager->accel_settings_manager);
        g_free(manager);
    }
}

GtkStringList *device_manager_get_device_names(DeviceManager *manager)
{
    GtkStringList *device_names = gtk_string_list_new(NULL);
    for (GList *l = manager->devices; l != NULL; l = l->next)
    {
        Device *device = (Device *)l->data;
        gtk_string_list_append(device_names, device->name);
    }
    return device_names;
}

void device_manager_set_speed_callback(DeviceManager *manager, void (*on_speed)(double speed, gpointer user_data), gpointer user_data)
{
    manager->on_speed = on_speed;
    manager->user_data = user_data;
}

void device_manager_set_current_device(DeviceManager *manager, const char *device_name)
{
    g_assert(manager);
    if (manager->current_device && manager->current_device->libinput_device)
    {
        libinput_path_remove_device(manager->current_device->libinput_device);
        manager->current_device->libinput_device = NULL;
    }

    manager->current_device = NULL;
    if (!device_name)
        return;
    for (GList *l = manager->devices; l != NULL; l = l->next)
    {
        Device *device = (Device *)l->data;
        if (g_strcmp0(device->name, device_name) == 0)
        {
            manager->current_device = device;
            break;
        }
    }

    if (!manager->current_device)
    {
        g_warning("Device manager did not found device: %s", device_name);
        return;
    }

    manager->current_device->libinput_device = libinput_path_add_device(manager->libinput_context, manager->current_device->node);
    if (!manager->current_device->libinput_device)
    {
        g_warning("Failed to add libinput device: %s", manager->current_device->node);
        manager->current_device = NULL;
    }
}

void print_accel_function(CustomAccelFunction *custom_accel_function, MovementType movement_type)
{
    printf("%s Accel function: step: %.3f, points(%d): ", MOVEMENT_TYPE_STRINGS[movement_type],
           custom_accel_function->step, custom_accel_function->npoints);
    for (int i = 0; i < custom_accel_function->npoints; i++)
    {
        printf("%.3f, ", custom_accel_function->points[i]);
    }
    printf("\n");
}

void print_accel_settings(AccelSettings *accel_settings)
{
    printf("Accel Profile: %d, %d, %d\n", accel_settings->profile[0], accel_settings->profile[1], accel_settings->profile[2]);
    for (int i = 0; i < MOVEMENT_TYPE_COUNT; i++)
    {
        print_accel_function(&accel_settings->custom_accel_functions[i], (MovementType)i);
    }
    printf("\n");
}

gboolean device_manager_set_custom_accel_function(DeviceManager *manager, CustomAccelFunction *custom_accel_function)
{
    if (!manager->current_device)
    {
        g_warning("Setting custom accel function: No current device set");
        return FALSE;
    }
    // Save current accel settings
    if (!manager->accel_settings_manager->get_accel_settings(manager->accel_settings_manager, manager->current_device, &manager->current_device->saved_accel_settings))
    {
        g_warning("Failed to get accel settings for device: %s", manager->current_device->name);
        return FALSE;
    }
    g_print("Saved accel settings for device: %s\n", manager->current_device->name);
    print_accel_settings(&manager->current_device->saved_accel_settings);

    AccelSettings new_settings = manager->current_device->saved_accel_settings;
    new_settings.custom_accel_functions[manager->movement_type] = *custom_accel_function;
    memcpy(new_settings.profile, (uint8_t[]){0, 0, 1}, sizeof(new_settings.profile));

    g_print("New accel settings for device: %s\n", manager->current_device->name);
    print_accel_settings(&new_settings);

    if (!manager->accel_settings_manager->set_accel_settings(manager->accel_settings_manager, manager->current_device, &new_settings))
    {
        g_warning("Failed to set accel settings for device: %s", manager->current_device->name);
        return FALSE;
    }

    return TRUE;
}

gboolean device_manager_restore_accel_settings(DeviceManager *manager)
{
    if (!manager->current_device)
    {
        g_warning("Restoring accel settings: No current device set");
        return FALSE;
    }
    if (!manager->accel_settings_manager->set_accel_settings(manager->accel_settings_manager, manager->current_device, &manager->current_device->saved_accel_settings))
    {
        g_warning("Failed to restore accel settings for device: %s", manager->current_device->name);
        return FALSE;
    }

    g_print("Restored accel settings for device: %s\n", manager->current_device->name);
    print_accel_settings(&manager->current_device->saved_accel_settings);

    return TRUE;
}

void device_manager_set_movement_type(DeviceManager *manager, MovementType movement_type)
{
    manager->movement_type = movement_type;
}

const char *MOVEMENT_TYPE_STRINGS[MOVEMENT_TYPE_COUNT] = {
    "Motion",
    "Scroll",
};
