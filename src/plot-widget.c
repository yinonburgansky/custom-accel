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
#include <gtk/gtk.h>
#include <string.h>

#define FONT_SIZE 16
#define WIDGET_PADDING_LEFT (FONT_SIZE / 2)
#define WIDGET_PADDING_BOTTOM (FONT_SIZE)
#define WIDGET_PADDING_RIGHT (FONT_SIZE)
#define WIDGET_PADDING_TOP (FONT_SIZE)
#define AXIS_MARKING_COUNT 10
#define AXIS_MARKING_LENGTH 10
#define AXIS_MARKING_PADDING_X 5
#define AXIS_MARKING_PADDING_Y 5
#define AXIS_LABEL_PADDING (FONT_SIZE / 2)

struct _PlotWidget
{
    GtkDrawingArea parent_instance;
    double x_axis_top_value;
    double y_axis_top_value;
    double current_x_value;
    char *x_axis_label;
    char *y_axis_label;
    double plot_width;
    double plot_height;
    double plot_margin_left;
    double plot_margin_top;
    Curve *curve;
};
G_DEFINE_TYPE(PlotWidget, plot_widget, GTK_TYPE_WIDGET)

Point plot_widget_from_screen(PlotWidget *self, Point point)
{
    return (Point){(point.x - self->plot_margin_left) / self->plot_width,
                   1 - (point.y - self->plot_margin_top) / self->plot_height};
}

Point plot_widget_to_screen(PlotWidget *self, Point point)
{
    return (Point){point.x * self->plot_width + self->plot_margin_left,
                   (1 - point.y) * self->plot_height + self->plot_margin_top};
}

static void format_axis_label(double value, char *label, size_t size, bool trim_zeros)
{
    snprintf(label, size, "%.1f", value);
    if (!trim_zeros)
        return;
    size_t len = strlen(label);
    if (len > 2 && strcmp(&label[len - 2], ".0") == 0)
    {
        label[len - 2] = '\0';
    }
}

static void draw_axes(PlotWidget *self, cairo_t *cr, int widget_width, int widget_height)
{
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_font_size(cr, FONT_SIZE);
    // Calculate text extents for axis labels and top values
    cairo_text_extents_t x_label_extents, y_label_extents, x_top_value_extents, y_top_value_extents;
    cairo_text_extents(cr, self->x_axis_label, &x_label_extents);
    cairo_text_extents(cr, self->y_axis_label, &y_label_extents);
    char top_value_label[8];
    format_axis_label(self->x_axis_top_value, top_value_label, sizeof(top_value_label), false);
    cairo_text_extents(cr, top_value_label, &x_top_value_extents);
    format_axis_label(self->y_axis_top_value, top_value_label, sizeof(top_value_label), false);
    cairo_text_extents(cr, top_value_label, &y_top_value_extents);

    // Calculate plot dimensions
    self->plot_margin_left = WIDGET_PADDING_LEFT + y_label_extents.height +
                             AXIS_LABEL_PADDING + y_top_value_extents.width +
                             AXIS_MARKING_PADDING_X + AXIS_MARKING_LENGTH;
    double plot_margin_bottom = WIDGET_PADDING_BOTTOM + x_label_extents.height +
                                AXIS_LABEL_PADDING + x_top_value_extents.height +
                                AXIS_MARKING_PADDING_Y + AXIS_MARKING_LENGTH;
    double plot_margin_right = WIDGET_PADDING_RIGHT + x_top_value_extents.width / 2;
    self->plot_margin_top = WIDGET_PADDING_TOP + y_top_value_extents.height / 2;
    self->plot_width = widget_width - (self->plot_margin_left + plot_margin_right);
    self->plot_height = widget_height - (self->plot_margin_top + plot_margin_bottom);

    // Draw X-axis label
    cairo_move_to(cr,
                  self->plot_margin_left + self->plot_width / 2 - x_label_extents.width / 2,
                  widget_height - WIDGET_PADDING_BOTTOM);
    cairo_show_text(cr, self->x_axis_label);

    // Draw Y-axis label
    cairo_save(cr);
    cairo_rotate(cr, -G_PI / 2);
    cairo_move_to(cr, -(self->plot_margin_top + self->plot_height / 2 + y_label_extents.width / 2), WIDGET_PADDING_LEFT + y_label_extents.height);
    cairo_show_text(cr, self->y_axis_label);
    cairo_restore(cr);

    cairo_save(cr);
    cairo_translate(cr, self->plot_margin_left, self->plot_margin_top);
    // Draw X-axis markings
    for (int i = 0; i <= AXIS_MARKING_COUNT; i++)
    {
        double x = i * self->plot_width / (double)AXIS_MARKING_COUNT;
        cairo_move_to(cr, x, self->plot_height);
        cairo_line_to(cr, x, self->plot_height + AXIS_MARKING_LENGTH);
        cairo_stroke(cr);

        double value = i * self->x_axis_top_value / AXIS_MARKING_COUNT;
        char label[8];
        format_axis_label(value, label, sizeof(label), true);

        cairo_text_extents_t extents;
        cairo_text_extents(cr, label, &extents);
        cairo_move_to(cr, x - extents.width / 2, self->plot_height + AXIS_MARKING_LENGTH + AXIS_MARKING_PADDING_Y + extents.height);
        cairo_show_text(cr, label);
    }

    // Draw Y-axis markings
    for (int i = 0; i <= AXIS_MARKING_COUNT; i++)
    {
        double y = i * self->plot_height / (double)AXIS_MARKING_COUNT;
        cairo_move_to(cr, -AXIS_MARKING_LENGTH, y);
        cairo_line_to(cr, 0, y);
        cairo_stroke(cr);

        double value = self->y_axis_top_value - i * self->y_axis_top_value / AXIS_MARKING_COUNT;
        char label[8];
        format_axis_label(value, label, sizeof(label), true);

        cairo_text_extents_t extents;
        cairo_text_extents(cr, label, &extents);
        cairo_move_to(cr, -AXIS_MARKING_LENGTH - extents.width - AXIS_MARKING_PADDING_X, y + extents.height / 2);
        cairo_show_text(cr, label);
    }

    // Draw X-axis
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_move_to(cr, 0, self->plot_height);
    cairo_line_to(cr, self->plot_width, self->plot_height);
    cairo_stroke(cr);

    // Draw Y-axis
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, 0, self->plot_height);
    cairo_stroke(cr);

    cairo_restore(cr);
}

static void draw_current_x_value(PlotWidget *self, cairo_t *cr)
{
    g_assert(self->curve);
    double x_value = self->current_x_value / self->x_axis_top_value;
    double y_value = plot_widget_get_y_value(self, x_value);
    Point current_value_screen = plot_widget_to_screen(self, (Point){x_value, y_value});
    cairo_set_source_rgb(cr, 0, 0, 1);
    cairo_move_to(cr, UNPACK(plot_widget_to_screen(self, (Point){x_value, 0})));
    cairo_line_to(cr, UNPACK(current_value_screen));
    cairo_stroke(cr);
    cairo_arc(cr, UNPACK(current_value_screen), CONTROL_POINT_RADIUS, 0, 2 * G_PI);
    cairo_fill(cr);
}

static void on_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
    PlotWidget *self = PLOT_WIDGET(widget);
    int widget_width = gtk_widget_get_width(widget);
    int widget_height = gtk_widget_get_height(widget);
    // Create a cairo context from the snapshot
    cairo_t *cr = gtk_snapshot_append_cairo(snapshot, &GRAPHENE_RECT_INIT(0, 0, widget_width, widget_height));

    // background color
    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    cairo_paint(cr);

    draw_axes(self, cr, widget_width, widget_height);
    if (self->curve && self->curve->draw)
    {
        self->curve->draw(self, cr);
        draw_current_x_value(self, cr);
    }

    cairo_destroy(cr);
}

static gboolean on_button_press(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    (void)gesture;
    (void)n_press;
    PlotWidget *self = PLOT_WIDGET(user_data);
    if (self->curve && self->curve->on_button_press)
        self->curve->on_button_press(self, x, y);
    return TRUE;
}

static void on_button_release(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    (void)gesture;
    (void)n_press;
    PlotWidget *self = PLOT_WIDGET(user_data);
    if (self->curve && self->curve->on_button_release)
        self->curve->on_button_release(self, x, y);
}

static void on_motion_notify(GtkEventControllerMotion *controller, double x, double y, gpointer user_data)
{
    (void)controller;
    PlotWidget *self = PLOT_WIDGET(user_data);
    if (self->curve && self->curve->on_motion_notify)
        self->curve->on_motion_notify(self, x, y);
}

void plot_widget_set_x_axis_top_value(PlotWidget *self, double value)
{
    self->x_axis_top_value = value;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void plot_widget_set_y_axis_top_value(PlotWidget *self, double value)
{
    self->y_axis_top_value = value;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void plot_widget_set_current_x_value(PlotWidget *self, double value)
{
    self->current_x_value = value;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

double plot_widget_get_x_axis_top_value(PlotWidget *self)
{
    return self->x_axis_top_value;
}

double plot_widget_get_y_axis_top_value(PlotWidget *self)
{
    return self->y_axis_top_value;
}

double plot_widget_get_current_x_value(PlotWidget *self)
{
    return self->current_x_value;
}

void plot_widget_set_x_axis_label(PlotWidget *self, const char *label)
{
    g_free(self->x_axis_label);
    self->x_axis_label = g_strdup(label);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void plot_widget_set_y_axis_label(PlotWidget *self, const char *label)
{
    g_free(self->y_axis_label);
    self->y_axis_label = g_strdup(label);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void plot_widget_set_curve(PlotWidget *self, Curve *curve)
{
    self->curve = curve;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

Curve *plot_widget_get_curve(PlotWidget *self)
{
    return self->curve;
}

double plot_widget_get_y_value(PlotWidget *self, double x)
{
    return self->curve->get_y_value(self, x);
}

static void plot_widget_init(PlotWidget *self)
{
    self->curve = NULL;
    self->x_axis_top_value = 1;
    self->y_axis_top_value = 1;
    self->current_x_value = 0;
    self->x_axis_label = g_strdup("X Axis");
    self->y_axis_label = g_strdup("Y Axis");

    GtkGesture *click = gtk_gesture_click_new();
    g_signal_connect(click, "pressed", G_CALLBACK(on_button_press), self);
    g_signal_connect(click, "released", G_CALLBACK(on_button_release), self);
    gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(click));

    GtkEventController *motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(on_motion_notify), self);
    gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(motion));

    // Ensure the widget is drawn initially
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void plot_widget_finalize(GObject *object)
{
    PlotWidget *self = PLOT_WIDGET(object);
    g_free(self->x_axis_label);
    g_free(self->y_axis_label);
    G_OBJECT_CLASS(plot_widget_parent_class)->finalize(object);
}

static void plot_widget_class_init(PlotWidgetClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->snapshot = on_snapshot;
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = plot_widget_finalize;
}

GtkWidget *plot_widget_new(void)
{
    PlotWidget *widget = g_object_new(PLOT_TYPE_WIDGET, NULL);
    plot_widget_init(widget);
    return GTK_WIDGET(widget);
}
