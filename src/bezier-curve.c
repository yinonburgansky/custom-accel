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

#include "plot-widget.h"
#include <math.h>

typedef struct
{
    Curve base;
    Point p1, p2;
    bool dragging;
    int drag_point;
} BezierCurve;

static double clamp(double value, double min, double max)
{
    return fmax(min, fmin(max, value));
}

static double bezier_interpolate(double t, double p0, double p1, double p2, double p3)
{
    double u = 1 - t;
    return u * u * u * p0 + 3 * u * u * t * p1 + 3 * u * t * t * p2 + t * t * t * p3;
}

static double bezier_interpolate_derivative(double t, double p0, double p1, double p2, double p3)
{
    double u = 1 - t;
    return 3 * u * u * (p1 - p0) + 6 * u * t * (p2 - p1) + 3 * t * t * (p3 - p2);
}

static double bezier_get_y_value(PlotWidget *self, double x_value)
{
    BezierCurve *curve = (BezierCurve *)plot_widget_get_curve(self);
    double t = x_value; // Initial guess
    double x, dx;
    int i = 0;

    for (; i < 20; ++i) // Limit the number of iterations
    {
        x = bezier_interpolate(t, 0.0, curve->p1.x, curve->p2.x, 1.0);
        if (fabs(x - x_value) < 1e-6)
            break;

        dx = bezier_interpolate_derivative(t, 0.0, curve->p1.x, curve->p2.x, 1.0);
        t -= (x - x_value) / dx;
        t = clamp(t, 0.0, 1.0);
    }

    return bezier_interpolate(t, 0.0, curve->p1.y, curve->p2.y, 1.0);
}

static void bezier_draw(PlotWidget *self, cairo_t *cr)
{
    BezierCurve *curve = (BezierCurve *)plot_widget_get_curve(self);
    Point p1_screen = plot_widget_to_screen(self, curve->p1);
    Point p2_screen = plot_widget_to_screen(self, curve->p2);
    Point origin_screen = plot_widget_to_screen(self, (Point){0, 0});
    Point end_screen = plot_widget_to_screen(self, (Point){1, 1});

    // Draw semi-transparent lines to handle points
    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.5);
    cairo_move_to(cr, UNPACK(origin_screen));
    cairo_line_to(cr, UNPACK(p1_screen));
    cairo_move_to(cr, UNPACK(p2_screen));
    cairo_line_to(cr, UNPACK(end_screen));
    cairo_stroke(cr);

    // Draw the Bezier curve
    cairo_set_line_width(cr, 4);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_move_to(cr, UNPACK(origin_screen));
    cairo_curve_to(cr,
                   UNPACK(p1_screen),
                   UNPACK(p2_screen),
                   UNPACK(end_screen));
    cairo_stroke(cr);

    // Draw control points
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_arc(cr, UNPACK(p1_screen), CONTROL_POINT_RADIUS, 0, 2 * G_PI);
    cairo_fill(cr);
    cairo_arc(cr, UNPACK(p2_screen), CONTROL_POINT_RADIUS, 0, 2 * G_PI);
    cairo_fill(cr);
}

static void bezier_on_button_press(PlotWidget *self, double x, double y)
{
    BezierCurve *curve = (BezierCurve *)plot_widget_get_curve(self);
    Point p1 = plot_widget_to_screen(self, curve->p1);
    Point p2 = plot_widget_to_screen(self, curve->p2);

    // Check if a control point is clicked
    if (hypot(x - p1.x, y - p1.y) < CONTROL_POINT_RADIUS * 2)
    {
        curve->dragging = TRUE;
        curve->drag_point = 1;
    }
    else if (hypot(x - p2.x, y - p2.y) < CONTROL_POINT_RADIUS * 2)
    {
        curve->dragging = TRUE;
        curve->drag_point = 2;
    }
}

static void bezier_on_button_release(PlotWidget *self, double x, double y)
{
    BezierCurve *curve = (BezierCurve *)plot_widget_get_curve(self);
    curve->dragging = FALSE;
}

static void bezier_on_motion_notify(PlotWidget *self, double x, double y)
{
    BezierCurve *curve = (BezierCurve *)plot_widget_get_curve(self);
    Point p = plot_widget_from_screen(self, (Point){x, y});
    p.x = clamp(p.x, 0.0, 1.0);
    p.y = clamp(p.y, 0.0, 1.0);

    if (curve->dragging)
    {
        if (curve->drag_point == 1)
        {
            curve->p1 = p;
        }
        else if (curve->drag_point == 2)
        {
            curve->p2 = p;
        }
        gtk_widget_queue_draw(GTK_WIDGET(self));
    }
}

Curve *bezier_curve_new(void)
{
    BezierCurve *bezier_curve = g_new0(BezierCurve, 1);
    bezier_curve->base.draw = bezier_draw;
    bezier_curve->base.get_y_value = bezier_get_y_value;
    bezier_curve->base.on_button_press = bezier_on_button_press;
    bezier_curve->base.on_button_release = bezier_on_button_release;
    bezier_curve->base.on_motion_notify = bezier_on_motion_notify;
    bezier_curve->p1 = (Point){0.4, 0.1};
    bezier_curve->p2 = (Point){0.5, 0.5};
    bezier_curve->dragging = FALSE;
    bezier_curve->drag_point = 0;
    return (Curve *)bezier_curve;
}
