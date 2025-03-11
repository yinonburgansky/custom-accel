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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CONTROL_POINT_RADIUS 5

typedef struct
{
    double x, y;
} Point;
#define UNPACK(point) (point).x, (point).y

#define PLOT_TYPE_WIDGET (plot_widget_get_type())
G_DECLARE_FINAL_TYPE(PlotWidget, plot_widget, PLOT, WIDGET, GtkDrawingArea)

typedef struct
{
    void (*draw)(PlotWidget *self, cairo_t *cr);
    double (*get_y_value)(PlotWidget *self, double x_value);
    void (*on_button_press)(PlotWidget *self, double x, double y);
    void (*on_button_release)(PlotWidget *self, double x, double y);
    void (*on_motion_notify)(PlotWidget *self, double x, double y);
    void *user_data;
} Curve;

GtkWidget *plot_widget_new(void);
void plot_widget_set_x_axis_top_value(PlotWidget *self, double value);
void plot_widget_set_y_axis_top_value(PlotWidget *self, double value);
void plot_widget_set_current_x_value(PlotWidget *self, double value);

double plot_widget_get_x_axis_top_value(PlotWidget *self);
double plot_widget_get_y_axis_top_value(PlotWidget *self);
double plot_widget_get_current_x_value(PlotWidget *self);

void plot_widget_set_x_axis_label(PlotWidget *self, const char *label);
void plot_widget_set_y_axis_label(PlotWidget *self, const char *label);

void plot_widget_set_curve(PlotWidget *self, Curve *curve);
Curve *plot_widget_get_curve(PlotWidget *self);

Point plot_widget_to_screen(PlotWidget *self, Point point);
Point plot_widget_from_screen(PlotWidget *self, Point point);

double plot_widget_get_y_value(PlotWidget *self, double x);

G_END_DECLS
