/* mx-window.c */

#include "mx-window.h"
#include "mx-toolbar.h"
#include "mx-focus-manager.h"
#include "mx-private.h"
#include <clutter/x11/clutter-x11.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/cursorfont.h>

static void clutter_container_iface_init (ClutterContainerIface *iface);
static void mx_stylable_iface_init (MxStylableIface *iface);

G_DEFINE_TYPE_WITH_CODE (MxWindow, mx_window, CLUTTER_TYPE_STAGE,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                                clutter_container_iface_init)
                         G_IMPLEMENT_INTERFACE (MX_TYPE_STYLABLE,
                                                mx_stylable_iface_init))

#define WINDOW_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MX_TYPE_WINDOW, MxWindowPrivate))

struct _MxWindowPrivate
{
  guint is_fullscreen : 1;
  guint has_toolbar   : 1;
  guint is_resizing   : 1;
  guint small_screen  : 1;
  guint has_mapped    : 1;
  guint width_set     : 1;
  guint height_set    : 1;

  gint  is_moving;

  CoglHandle resize_grip;
  gfloat     last_width;
  gfloat     last_height;
  gfloat     natural_width;
  gfloat     natural_height;

  gchar *icon_name;

  ClutterActor *toolbar;
  ClutterActor *child;

  gint  drag_x_start;
  gint  drag_y_start;
  gint  drag_win_x_start;
  gint  drag_win_y_start;
  guint drag_width_start;
  guint drag_height_start;

  MxStyle *style;
  gchar   *pseudo_class;
  gchar   *style_class;

  ClutterActor *draggable;

  MxFocusManager *focus_manager;
};

enum
{
  PROP_STYLE = 1,
  PROP_STYLE_CLASS,
  PROP_STYLE_PSEUDO_CLASS,
  PROP_HAS_TOOLBAR,
  PROP_SMALL_SCREEN,
  PROP_ICON_NAME
};

/* clutter container iface implementation */
static void
mx_window_add (ClutterContainer *container,
               ClutterActor     *actor)
{
  MxWindowPrivate *priv = MX_WINDOW (container)->priv;

  if (CLUTTER_IS_CLONE (actor))
    {
      ClutterActor *source = clutter_clone_get_source ((ClutterClone *)actor);

      if (MX_IS_DRAGGABLE (source))
        {
          clutter_actor_set_parent (actor, CLUTTER_ACTOR (container));
          priv->draggable = actor;
        }
    }
  else if (MX_IS_DRAGGABLE (actor))
    {
      clutter_actor_set_parent (actor, CLUTTER_ACTOR (container));
      priv->draggable = actor;
    }
  else
    {
      mx_window_set_child (MX_WINDOW (container), actor);
    }
}

static void
mx_window_remove (ClutterContainer *container,
                  ClutterActor     *actor)
{
  MxWindowPrivate *priv = MX_WINDOW (container)->priv;

  if (priv->child == actor)
    mx_window_set_child (MX_WINDOW (container), NULL);

  if (priv->draggable == actor)
    {
      clutter_actor_unparent (actor);
      priv->draggable = NULL;
    }
}

static void
mx_window_foreach (ClutterContainer *container,
                   ClutterCallback   callback,
                   gpointer          user_data)
{
  MxWindowPrivate *priv = MX_WINDOW (container)->priv;

  if (priv->child)
    callback (priv->child, user_data);
}

static void
clutter_container_iface_init (ClutterContainerIface *iface)
{
  iface->add = mx_window_add;
  iface->remove = mx_window_remove;
  iface->foreach = mx_window_foreach;
}

/* stylable implementation */
static void
mx_window_set_style (MxStylable *stylable,
                     MxStyle    *style)
{
  MxWindowPrivate *priv = MX_WINDOW (stylable)->priv;

  if (priv->style)
    g_object_unref (priv->style);

  priv->style = style;
}

static MxStyle*
mx_window_get_style (MxStylable *stylable)
{
  MxWindowPrivate *priv = MX_WINDOW (stylable)->priv;

  if (!priv->style)
    return priv->style = mx_style_get_default ();
  else
    return priv->style;
}

static void
mx_window_set_style_class (MxStylable  *actor,
                           const gchar *style_class)
{
  MxWindowPrivate *priv;

  priv = MX_WINDOW (actor)->priv;

  if (g_strcmp0 (style_class, priv->style_class))
    {
      g_free (priv->style_class);
      priv->style_class = g_strdup (style_class);

      g_object_notify (G_OBJECT (actor), "style-class");
    }
}

static void
mx_window_set_style_pseudo_class (MxStylable  *actor,
                                   const gchar *pseudo_class)
{
  MxWindowPrivate *priv;

  priv = MX_WINDOW (actor)->priv;

  if (g_strcmp0 (pseudo_class, priv->pseudo_class))
    {
      g_free (priv->pseudo_class);
      priv->pseudo_class = g_strdup (pseudo_class);

      g_object_notify (G_OBJECT (actor), "style-pseudo-class");
    }
}

static const gchar*
mx_window_get_style_pseudo_class (MxStylable *actor)
{
  return ((MxWindow *) actor)->priv->pseudo_class;
}


static const gchar*
mx_window_get_style_class (MxStylable *actor)
{
  return ((MxWindow *) actor)->priv->style_class;
}



static void
mx_stylable_iface_init (MxStylableIface *iface)
{
  static gboolean is_initialised = FALSE;

  if (!is_initialised)
    {
      GParamSpec *pspec;
      ClutterColor bg_color = { 0xff, 0xff, 0xff, 0xff };

      is_initialised = TRUE;

      iface->get_style = mx_window_get_style;
      iface->set_style = mx_window_set_style;
      iface->set_style_class = mx_window_set_style_class;
      iface->get_style_class = mx_window_get_style_class;
      iface->set_style_pseudo_class = mx_window_set_style_pseudo_class;
      iface->get_style_pseudo_class = mx_window_get_style_pseudo_class;

      pspec = clutter_param_spec_color ("background-color",
                                        "Background Color",
                                        "The background color of the window",
                                        &bg_color,
                                        G_PARAM_READWRITE);
      mx_stylable_iface_install_property (iface, MX_TYPE_WINDOW, pspec);

      pspec = g_param_spec_boxed ("x-mx-resize-grip",
                                  "Resize Grip",
                                  "Resize grip used in the corner of the"
                                  " window to allow the user to resize.",
                                  MX_TYPE_BORDER_IMAGE,
                                  G_PARAM_READWRITE);
      mx_stylable_iface_install_property (iface, MX_TYPE_WINDOW, pspec);
    }
}

static void
mx_window_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  MxWindowPrivate *priv = MX_WINDOW (object)->priv;

  switch (property_id)
    {
    case PROP_STYLE:
      g_value_set_object (value, priv->style);
      break;

    case PROP_STYLE_CLASS:
      g_value_set_string (value, priv->style_class);
      break;

    case PROP_STYLE_PSEUDO_CLASS:
      g_value_set_string (value, priv->pseudo_class);
      break;

    case PROP_HAS_TOOLBAR:
      g_value_set_boolean (value, priv->has_toolbar);
      break;

    case PROP_SMALL_SCREEN:
      g_value_set_boolean (value, priv->small_screen);
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, priv->icon_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
mx_window_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  MxStylable *win = MX_STYLABLE (object);


  switch (property_id)
    {
    case PROP_STYLE:
      mx_window_set_style (win, MX_STYLE (g_value_get_object (value)));
      break;

    case PROP_STYLE_CLASS:
      mx_window_set_style_class (win, g_value_get_string (value));
      break;

    case PROP_STYLE_PSEUDO_CLASS:
      mx_window_set_style_pseudo_class (win, g_value_get_string (value));
      break;

    case PROP_HAS_TOOLBAR:
      mx_window_set_has_toolbar (MX_WINDOW (object),
                                 g_value_get_boolean (value));
      break;

    case PROP_SMALL_SCREEN:
      mx_window_set_small_screen (MX_WINDOW (object),
                                  g_value_get_boolean (value));
      break;

    case PROP_ICON_NAME:
      mx_window_set_icon_name (MX_WINDOW (object),
                               g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
mx_window_dispose (GObject *object)
{
  G_OBJECT_CLASS (mx_window_parent_class)->dispose (object);
}

static void
mx_window_finalize (GObject *object)
{
  MxWindowPrivate *priv = MX_WINDOW (object)->priv;

  if (priv->resize_grip)
    {
      cogl_handle_unref (priv->resize_grip);
      priv->resize_grip = NULL;
    }

  g_free (priv->icon_name);

  G_OBJECT_CLASS (mx_window_parent_class)->finalize (object);
}




static void
mx_window_get_size (MxWindow *self,
                    gfloat   *width_p,
                    gfloat   *height_p,
                    gfloat   *pref_width_p,
                    gfloat   *pref_height_p)
{
  gboolean has_border;
  gfloat width, pref_width;
  MxWindowPrivate *priv = self->priv;

  pref_width = width = 0;

  has_border = priv->has_toolbar &&
               !(priv->small_screen ||
                 clutter_stage_get_fullscreen (CLUTTER_STAGE (self)));

  if (priv->toolbar)
    clutter_actor_get_preferred_width (priv->toolbar,
                                       -1,
                                       &width,
                                       &pref_width);

  if (priv->child)
    {
      gfloat child_min_width, child_pref_width;
      clutter_actor_get_preferred_width (priv->child,
                                         -1,
                                         &child_min_width,
                                         &child_pref_width);
      if (child_min_width > width)
        width = child_min_width;
      if (child_pref_width > pref_width)
        pref_width = child_pref_width;
    }

  if (width_p)
    *width_p = width + (has_border ? 2 : 0);
  if (pref_width_p)
    *pref_width_p = pref_width + (has_border ? 2 : 0);

  if (!height_p && !pref_height_p)
    return;

  if (height_p)
    *height_p = 0;
  if (pref_height_p)
    *pref_height_p = 0;

  if (priv->toolbar)
    clutter_actor_get_preferred_height (priv->toolbar,
                                        width,
                                        height_p,
                                        pref_height_p);

  if (priv->child)
    {
      gfloat child_min_height, child_pref_height;
      clutter_actor_get_preferred_height (priv->child,
                                          width,
                                          &child_min_height,
                                          &child_pref_height);
      if (height_p)
        *height_p += child_min_height;
      if (pref_height_p)
        *pref_height_p += child_pref_height;
    }

  if (has_border)
    {
      if (height_p)
        *height_p += 2;
      if (pref_height_p)
        *pref_height_p += 2;
    }
}

typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long inputMode;
    unsigned long status;
} PropMotifWmHints;

static void
mx_window_set_wm_hints (MxWindow *window)
{
  const gchar *icon_name;
  Display *dpy;
  Window win;

  static Atom motif_wm_hints_atom = None;
  static Atom net_wm_icon = None;

  MxWindowPrivate *priv = window->priv;

  dpy = clutter_x11_get_default_display ();
  win = clutter_x11_get_stage_window (CLUTTER_STAGE (window));

  if (win == None)
    return;

  if (!motif_wm_hints_atom)
    motif_wm_hints_atom = XInternAtom (dpy, "_MOTIF_WM_HINTS", False);

  /* Remove/add the window decorations */
  if (motif_wm_hints_atom)
    {
      PropMotifWmHints new_hints = {0,};
      PropMotifWmHints *hints;

      hints = &new_hints;

      hints->flags = 0x2;
      hints->functions = 0x0;
      hints->decorations = priv->has_toolbar ? 0x0 : 0x1;

      XChangeProperty (dpy, win, motif_wm_hints_atom, motif_wm_hints_atom,
                       32, PropModeReplace, (guchar*) hints,
                       sizeof(PropMotifWmHints)/ sizeof (long));
    }

  if (!net_wm_icon)
    net_wm_icon = XInternAtom (dpy, "_NET_WM_ICON", False);

  /* Set the window icon */
  icon_name = priv->icon_name ? priv->icon_name : g_get_prgname ();
  if (icon_name && net_wm_icon)
    {
      guint width, height;
      CoglHandle texture;
      guchar *data;
      gint size;

      /* Lookup icon for program name */
      texture = mx_icon_theme_lookup (mx_icon_theme_get_default (),
                                      icon_name, 32);
      if (!texture)
        return;

      /* Get window icon size */
      width = cogl_texture_get_width (texture);
      height = cogl_texture_get_height (texture);
      size = cogl_texture_get_data (texture,
                                    COGL_PIXEL_FORMAT_ARGB_8888,
                                    width * 4,
                                    NULL);
      if (!size)
        {
          g_warning ("Unable to get texture data in "
                     "correct format for window icon");
          cogl_handle_unref (texture);
          return;
        }

      /* Get the window icon */
      data = g_malloc (size + (sizeof (int) * 2));
      ((int *)data)[0] = width;
      ((int *)data)[1] = height;
      cogl_texture_get_data (texture,
                             COGL_PIXEL_FORMAT_ARGB_8888,
                             width * 4,
                             data + (sizeof (int) * 2));
      cogl_handle_unref (texture);

      /* Set the property */
      XChangeProperty (dpy, win, net_wm_icon, XA_CARDINAL,
                       32, PropModeReplace, data,
                       (width * height) + 2);

      g_free (data);
    }
}

static void
mx_window_map (ClutterActor *actor)
{
  MxWindow *window;
  MxWindowPrivate *priv;

  CLUTTER_ACTOR_CLASS (mx_window_parent_class)->map (actor);

  window = MX_WINDOW (actor);
  priv = window->priv;

  clutter_actor_map (priv->toolbar);

  if (priv->child)
    clutter_actor_map (priv->child);

  if (priv->draggable)
    clutter_actor_map (priv->draggable);

  mx_window_set_wm_hints (window);
}

static void
mx_window_unmap (ClutterActor *actor)
{
  MxWindowPrivate *priv;

  priv = MX_WINDOW (actor)->priv;

  clutter_actor_unmap (priv->toolbar);

  if (priv->draggable)
    clutter_actor_unmap (priv->draggable);

  CLUTTER_ACTOR_CLASS (mx_window_parent_class)->unmap (actor);
}

static void
mx_window_paint (ClutterActor *actor)
{
  MxWindowPrivate *priv;
  gfloat width, height;

  priv = MX_WINDOW (actor)->priv;

  CLUTTER_ACTOR_CLASS (mx_window_parent_class)->paint (actor);

  if (priv->child)
    clutter_actor_paint (priv->child);

  clutter_actor_paint (priv->toolbar);

  if (priv->draggable)
    clutter_actor_paint (priv->draggable);

  /* If we're in small-screen or fullscreen mode, or we don't have the toolbar,
   * we don't want a frame or a resize handle.
   */
  if (!priv->has_toolbar || priv->small_screen ||
      clutter_stage_get_fullscreen (CLUTTER_STAGE (actor)))
    return;

  /* paint frame */

  clutter_actor_get_size (actor, &width, &height);
  cogl_set_source_color4f (0.2, 0.2, 0.2, 1);

  cogl_rectangle (0, 0, width, 1);
  cogl_rectangle (0, height - 1, width, height);

  cogl_rectangle (0, 1, 1, height - 1);
  cogl_rectangle (width - 1, 1, width, height - 1);

  if (priv->resize_grip)
    {
      guint rheight, rwidth;

      cogl_set_source_texture (priv->resize_grip);
      rwidth = cogl_texture_get_width (priv->resize_grip);
      rheight = cogl_texture_get_height (priv->resize_grip);
      cogl_rectangle (width - rwidth - 1, height - rheight - 1,
                      width - 1, height - 1);
    }
}

static void
mx_window_pick (ClutterActor       *actor,
                const ClutterColor *color)
{
  MxWindowPrivate *priv;

  CLUTTER_ACTOR_CLASS (mx_window_parent_class)->pick (actor, color);

  priv = MX_WINDOW (actor)->priv;

  /* Don't pick while we're moving/resizing */
  if (priv->is_moving != -1)
    return;

  if (priv->has_toolbar)
    clutter_actor_paint (priv->toolbar);

  if (priv->child)
    clutter_actor_paint (priv->child);
}

static void
mx_window_allocate (ClutterActor           *actor,
                    const ClutterActorBox  *box,
                    ClutterAllocationFlags  flags)
{
  ClutterActorBox toolbarbox, childbox;
  MxPadding padding;
  MxWindow *window;
  MxWindowPrivate *priv;
  gfloat width, height, toolbar_height;

  CLUTTER_ACTOR_CLASS (mx_window_parent_class)->allocate (actor, box, flags);

  window = MX_WINDOW (actor);
  priv = window->priv;

  if (!priv->has_toolbar || priv->small_screen ||
      clutter_stage_get_fullscreen (CLUTTER_STAGE (actor)))
      padding.top = padding.right = padding.bottom = padding.left = 0;
  else
      padding.top = padding.right = padding.bottom = padding.left = 1;

  if (priv->has_toolbar)
    {
      clutter_actor_get_preferred_height (priv->toolbar, (box->x2 - box->x1), NULL,
                                          &toolbar_height);

      toolbarbox.x1 = padding.left;
      toolbarbox.y1 = padding.top;
      toolbarbox.x2 = (box->x2 - box->x1) - padding.right;
      toolbarbox.y2 = toolbarbox.y1 + toolbar_height;

      clutter_actor_allocate (priv->toolbar, &toolbarbox, flags);
    }
  else
    {
      toolbarbox.y2 = padding.top;
      toolbar_height = 0;
    }

  if (priv->child)
    {
      childbox.x1 = padding.left;
      childbox.y1 = toolbarbox.y2;
      childbox.x2 = (box->x2 - box->x1) - padding.right;
      childbox.y2 = (box->y2 - box->y1) - padding.bottom;
      clutter_actor_allocate (priv->child, &childbox, flags);
    }

  if (priv->draggable)
    clutter_actor_allocate_preferred_size (priv->draggable, flags);

  /* Return if we're fullscreen, messing with the window size
   * in fullscreen mode can cause odd race conditions.
   */
  if (clutter_stage_get_fullscreen (CLUTTER_STAGE (actor)))
    return;

  if (!priv->has_mapped)
    {
      Window win;
      Display *dpy;

      win = clutter_x11_get_stage_window (CLUTTER_STAGE (actor));
      dpy = clutter_x11_get_default_display ();

      priv->has_mapped = TRUE;

      if (priv->small_screen)
        {
          XRRScreenResources *res;

          res = XRRGetScreenResourcesCurrent (dpy, win);

          XMoveResizeWindow (dpy, win,
                             0, 0,
                             res->modes[res->nmode].width,
                             res->modes[res->nmode].height);

          XRRFreeScreenResources (res);
        }
      else
        {
          /* Set the initial size of the window - if the user has set
           * a dimension, it will be used, otherwise the preferred size
           * will be used.
           */
          mx_window_get_size (window, NULL, NULL, &width, &height);

          if (priv->width_set)
            width = priv->natural_width + 2;
          if (priv->height_set)
            height = priv->natural_height + 2;

          XResizeWindow (dpy, win, width, height);
        }
    }
#if CLUTTER_CHECK_VERSION(1,2,0)
  else
    {
      /* Update minimum size */
      mx_window_get_size (window, &width, &height, NULL, NULL);
      if (width < 1.0)
        width = 1.0;
      if (height < 1.0)
        height = 1.0;
      clutter_stage_set_minimum_size (CLUTTER_STAGE (window),
                                      (guint)width,
                                      (guint)height);
    }
#endif
}

static gboolean
mx_window_button_press_event (ClutterActor       *actor,
                              ClutterButtonEvent *event)
{
  unsigned int width, height, border_width, depth, mask;
  Window win, root, child;
  int x, y, win_x, win_y;
  MxWindowPrivate *priv;
  Display *dpy;

  priv = MX_WINDOW (actor)->priv;

  /* Bail out early in no-toolbar, small-screen or fullscreen mode */
  if (!priv->has_toolbar || priv->small_screen ||
      clutter_stage_get_fullscreen (CLUTTER_STAGE (actor)))
    return FALSE;

  /* We're already moving/resizing */
  if (priv->is_moving != -1)
    return FALSE;

  /* We only care about the first mouse button */
  if (event->button != 1)
    return FALSE;

  priv->is_moving = clutter_input_device_get_device_id (event->device);

  win = clutter_x11_get_stage_window (CLUTTER_STAGE (actor));
  dpy = clutter_x11_get_default_display ();

  /* Get the initial width/height */
  XGetGeometry (dpy, win, &root, &x, &y, &width, &height,
                &border_width, &depth);

  priv->drag_win_x_start = x;
  priv->drag_win_y_start = y;
  priv->drag_width_start = width;
  priv->drag_height_start = height;

  /* Get the initial cursor position */
  XQueryPointer (dpy, root, &root, &child, &x, &y, &win_x, &win_y, &mask);

  priv->drag_x_start = x;
  priv->drag_y_start = y;

  /* Disable motion events on other actors */
  clutter_set_motion_events_enabled (FALSE);

  /* Grab the mouse so that we receive the release if the cursor
   * goes off-stage.
   */
  clutter_grab_pointer_for_device (actor, priv->is_moving);

  return TRUE;
}

static void
mx_window_button_release (MxWindow *window)
{
  MxWindowPrivate *priv = window->priv;

  if (priv->is_moving != -1)
    {
      clutter_ungrab_pointer_for_device (priv->is_moving);
      clutter_set_motion_events_enabled (TRUE);
      priv->is_moving = -1;
    }
}

static gboolean
mx_window_button_release_event (ClutterActor       *actor,
                                ClutterButtonEvent *event)
{
  MxWindow *window = MX_WINDOW (actor);
  MxWindowPrivate *priv = window->priv;

  if ((clutter_input_device_get_device_id (event->device) == priv->is_moving) &&
      (event->button == 1))
    {
      mx_window_button_release (window);
      return TRUE;
    }

  return FALSE;
}

static gboolean
mx_window_captured_event (ClutterActor *actor,
                          ClutterEvent *event)
{
  MxWindowPrivate *priv = MX_WINDOW (actor)->priv;

  switch (event->type)
    {
    case CLUTTER_MOTION:
      /* Check if we're over the resize handle */
      if ((priv->is_moving == -1) && priv->has_toolbar && !priv->small_screen &&
          !clutter_stage_get_fullscreen (CLUTTER_STAGE (actor)))
        {
          gint x, y;
          Window win;
          Display *dpy;
          gfloat height, width;
          guint rheight, rwidth;

          static Cursor csoutheast = 0;
          ClutterMotionEvent *mevent = &event->motion;

          win = clutter_x11_get_stage_window (CLUTTER_STAGE (actor));
          dpy = clutter_x11_get_default_display ();

          clutter_actor_get_size (actor, &width, &height);

          x = mevent->x;
          y = mevent->y;

          /* Create the resize cursor */
          if (!csoutheast)
            csoutheast = XCreateFontCursor (dpy, XC_bottom_right_corner);

          rwidth = cogl_texture_get_width (priv->resize_grip);
          rheight = cogl_texture_get_height (priv->resize_grip);

          /* Set the cursor if necessary */
          if (x > width - rwidth && y > height - rheight)
            {
              if (!priv->is_resizing)
                {
                  XDefineCursor (dpy, win, csoutheast);
                  priv->is_resizing = TRUE;
                }
              return TRUE;
            }
          else
            {
              if (priv->is_resizing)
                {
                  XUndefineCursor (dpy, win);
                  priv->is_resizing = FALSE;
                }
              return FALSE;
            }
        }

    case CLUTTER_BUTTON_PRESS:
      /* We want resizing to happen even if there are active widgets
       * underneath the resize-handle.
       */
      if (priv->is_resizing)
        return mx_window_button_press_event (actor, &event->button);
      else
        return FALSE;

    default:
      return FALSE;
    }
}

static gboolean
mx_window_motion_event (ClutterActor       *actor,
                        ClutterMotionEvent *event)
{
  gint offsetx, offsety;
  gint x, y, winx, winy;
  guint mask;
  MxWindow *window;
  MxWindowPrivate *priv;
  Window win, root_win, root, child;
  Display *dpy;
  gfloat height, width;

  window = MX_WINDOW (actor);
  priv = window->priv;

  /* Ignore motion events while in small-screen mode, fullscreen mode,
   * if we have no toolbar, or if they're not from our grabbed device.
   */
  if ((!priv->has_toolbar) ||
      (priv->small_screen) ||
      (clutter_stage_get_fullscreen (CLUTTER_STAGE (actor))) ||
      (clutter_input_device_get_device_id (event->device) != priv->is_moving))
    return FALSE;

  /* Check if the mouse button is still down - if the user releases the
   * mouse button while outside of the stage (which can happen), we don't
   * get the release event.
   */
  if (!(event->modifier_state & CLUTTER_BUTTON1_MASK))
    {
      mx_window_button_release (window);
      return TRUE;
    }

  win = clutter_x11_get_stage_window (CLUTTER_STAGE (actor));
  dpy = clutter_x11_get_default_display ();

  clutter_actor_get_size (actor, &width, &height);

  x = event->x;
  y = event->y;

  /* Move/resize the window if we're dragging */
  offsetx = priv->drag_x_start;
  offsety = priv->drag_y_start;

  root_win = clutter_x11_get_root_window ();
  XQueryPointer (dpy, root_win, &root, &child, &x, &y, &winx, &winy, &mask);

  if (priv->is_resizing)
    {
      XRRScreenResources *res;
      gfloat min_width, min_height;

      mx_window_get_size (window, &min_width, &min_height, NULL, NULL);

      x = MAX (priv->drag_width_start + (x - priv->drag_x_start), min_width);
      y = MAX (priv->drag_height_start + (y - priv->drag_y_start), min_height);

      res = XRRGetScreenResourcesCurrent (dpy, win);
      width = res->modes[res->nmode].width;
      height = res->modes[res->nmode].height;
      XRRFreeScreenResources (res);

      width = MIN (x, width - priv->drag_win_x_start);
      height = MIN (y, height - priv->drag_win_y_start);

#if !CLUTTER_CHECK_VERSION(1,2,0)
      /* Set the natural width/height so ClutterStageX11 won't try to
       * resize us back to our minimum size.
       */
      priv->natural_width = width;
      priv->natural_height = height;

      XMoveResizeWindow (dpy, win,
                         priv->drag_win_x_start, priv->drag_win_y_start,
                         width, height);
#else
      clutter_actor_set_size (actor, width, height);
#endif
    }
  else
    XMoveWindow (dpy, win,
                 MAX (0, priv->drag_win_x_start + x - offsetx),
                 MAX (0, priv->drag_win_y_start + y - offsety));

  return TRUE;
}

static void
style_changed_cb (MxStyle *style, MxWindow *window)
{
  MxWindowPrivate *priv = window->priv;
  MxBorderImage *grip_filename;
  ClutterColor *color;

  if (priv->resize_grip)
    {
      cogl_handle_unref (priv->resize_grip);
      priv->resize_grip = NULL;
    }

  mx_stylable_get (MX_STYLABLE (window),
                   "x-mx-resize-grip", &grip_filename,
                   "background-color", &color,
                   NULL);

  if (color)
    {
      clutter_stage_set_color (CLUTTER_STAGE (window), color);
      clutter_color_free (color);
    }

  if (grip_filename)
    {
      priv->resize_grip = cogl_texture_new_from_file (grip_filename->uri,
                                                      COGL_TEXTURE_NONE,
                                                      COGL_PIXEL_FORMAT_ANY,
                                                      NULL);
      if (priv->resize_grip == COGL_INVALID_HANDLE)
        {
          priv->resize_grip = NULL;
          g_warning ("Error loading resize grip image");
        }

      g_boxed_free (MX_TYPE_BORDER_IMAGE, grip_filename);

    }
}

#if !CLUTTER_CHECK_VERSION(1,2,0)
static void
mx_window_get_preferred_width (ClutterActor *self,
                               gfloat        for_height,
                               gfloat       *min_width_p,
                               gfloat       *nat_width_p)
{
  if (clutter_stage_get_fullscreen (CLUTTER_STAGE (self)))
    CLUTTER_ACTOR_CLASS (mx_window_parent_class)->
      get_preferred_width (self, for_height, min_width_p, nat_width_p);
  else
    {
      gfloat min_width;
      MxWindowPrivate *priv = MX_WINDOW (self)->priv;

      mx_window_get_size (MX_WINDOW (self), &min_width, NULL, NULL, NULL);
      if (min_width_p)
        *min_width_p = min_width;
      if (nat_width_p)
        *nat_width_p = priv->natural_width ? priv->natural_width : min_width;
    }
}

static void
mx_window_get_preferred_height (ClutterActor *self,
                                gfloat        for_width,
                                gfloat       *min_height_p,
                                gfloat       *nat_height_p)
{
  if (clutter_stage_get_fullscreen (CLUTTER_STAGE (self)))
    CLUTTER_ACTOR_CLASS (mx_window_parent_class)->
      get_preferred_height (self, for_width, min_height_p, nat_height_p);
  else
    {
      gfloat min_height;
      MxWindowPrivate *priv = MX_WINDOW (self)->priv;

      mx_window_get_size (MX_WINDOW (self), NULL, &min_height, NULL, NULL);
      if (min_height_p)
        *min_height_p = min_height;
      if (nat_height_p)
        *nat_height_p = priv->natural_height ?
                          priv->natural_height : min_height;
    }
}
#endif

static void
mx_window_realize_cb (ClutterActor *actor)
{
  gboolean width_set, height_set;

  MxWindow *window = MX_WINDOW (actor);
  MxWindowPrivate *priv = window->priv;

  /* See if the user has set a size on the window to use on initial map */
  g_object_get (G_OBJECT (window),
                "natural-width", &priv->natural_width,
                "natural-width-set", &width_set,
                "natural-height", &priv->natural_height,
                "natural-height-set", &height_set,
                NULL);

  priv->width_set = width_set;
  priv->height_set = height_set;
}

static void
mx_window_class_init (MxWindowClass *klass)
{
  GParamSpec *pspec;

  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MxWindowPrivate));

  object_class->get_property = mx_window_get_property;
  object_class->set_property = mx_window_set_property;
  object_class->dispose = mx_window_dispose;
  object_class->finalize = mx_window_finalize;

  actor_class->map = mx_window_map;
  actor_class->unmap = mx_window_unmap;
  actor_class->paint = mx_window_paint;
  actor_class->pick = mx_window_pick;
  actor_class->allocate = mx_window_allocate;
  actor_class->button_press_event = mx_window_button_press_event;
  actor_class->button_release_event = mx_window_button_release_event;
  actor_class->captured_event = mx_window_captured_event;
  actor_class->motion_event = mx_window_motion_event;
#if !CLUTTER_CHECK_VERSION(1,2,0)
  actor_class->get_preferred_width = mx_window_get_preferred_width;
  actor_class->get_preferred_height = mx_window_get_preferred_height;
#endif

  /* stylable interface properties */
  g_object_class_override_property (object_class, PROP_STYLE, "style");
  g_object_class_override_property (object_class, PROP_STYLE_CLASS,
                                    "style-class");
  g_object_class_override_property (object_class, PROP_STYLE_PSEUDO_CLASS,
                                    "style-pseudo-class");

  pspec = g_param_spec_boolean ("has-toolbar",
                                "Has toolbar",
                                "Window should have a toolbar.",
                                TRUE,
                                MX_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_HAS_TOOLBAR, pspec);

  pspec = g_param_spec_boolean ("small-screen",
                                "Small screen",
                                "Window should occupy the entire screen "
                                "contents, without explicitly setting "
                                "itself fullscreen.",
                                FALSE,
                                MX_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SMALL_SCREEN, pspec);

  pspec = g_param_spec_string ("icon-name",
                               "Icon name",
                               "Icon name to use for the window icon.",
                               NULL,
                               MX_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ICON_NAME, pspec);
}

static void
mx_window_fullscreen_set_cb (MxWindow *self)
{
  MxWindowPrivate *priv = self->priv;

  /* If we're in small-screen mode, make sure the size gets reset
   * correctly.
   */
  if (!clutter_stage_get_fullscreen (CLUTTER_STAGE (self)) &&
      priv->small_screen)
    priv->has_mapped = FALSE;

  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}

static void
mx_window_init (MxWindow *self)
{
  MxWindowPrivate *priv;

  priv = self->priv = WINDOW_PRIVATE (self);

  priv->is_moving = -1;

  priv->toolbar = mx_toolbar_new ();
  clutter_actor_set_parent (priv->toolbar, CLUTTER_ACTOR (self));

  priv->has_toolbar = TRUE;

  priv->focus_manager = mx_focus_manager_get_for_stage (CLUTTER_STAGE (self));

  priv->style = mx_style_get_default ();

  g_signal_connect (priv->style, "changed",
                    G_CALLBACK (style_changed_cb), self);

  style_changed_cb (NULL, self);

  clutter_stage_set_user_resizable (CLUTTER_STAGE (self), TRUE);

  g_signal_connect (self, "notify::fullscreen-set",
                    G_CALLBACK (mx_window_fullscreen_set_cb), NULL);
  g_signal_connect (self, "realize",
                    G_CALLBACK (mx_window_realize_cb), NULL);

#if CLUTTER_CHECK_VERSION(1,2,0)
  g_object_set (G_OBJECT (self), "use-alpha", TRUE, NULL);
#endif

  /* connect the notifiers for the stylable */
  mx_stylable_connect_change_notifiers (MX_STYLABLE (self));
}

MxWindow *
mx_window_new (void)
{
  return g_object_new (MX_TYPE_WINDOW, NULL);
}

void
mx_window_set_child (MxWindow     *window,
                     ClutterActor *actor)
{
  MxWindowPrivate *priv;

  g_return_if_fail (MX_IS_WINDOW (window));
  g_return_if_fail (actor == NULL || CLUTTER_IS_ACTOR (actor));

  priv = window->priv;

  if (priv->child != actor)
    {
      if (priv->child)
        {
          ClutterActor *old_child = priv->child;

          g_object_ref (old_child);

          priv->child = NULL;
          clutter_actor_unparent (old_child);

          g_signal_emit_by_name (window, "actor-removed", old_child);

          g_object_unref (old_child);
        }

      if (actor)
        {
          priv->child = actor;
          clutter_actor_set_parent (actor, CLUTTER_ACTOR (window));
          g_signal_emit_by_name (window, "actor-added", priv->child);
        }
    }

}

ClutterActor*
mx_window_get_child (MxWindow *window)
{
  g_return_val_if_fail (MX_IS_WINDOW (window), NULL);

  return window->priv->child;
}

void
mx_window_set_has_toolbar (MxWindow *window,
                           gboolean  toolbar)
{
  MxWindowPrivate *priv;

  g_return_if_fail (MX_IS_WINDOW (window));

  priv = window->priv;

  if (window->priv->has_toolbar != toolbar)
    {
      window->priv->has_toolbar = toolbar;

      if (!toolbar)
        clutter_actor_hide (priv->toolbar);
      else
        clutter_actor_show (priv->toolbar);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (window));

      g_object_notify (G_OBJECT (window), "has-toolbar");

      /* Remove/add window decorations */
      mx_window_set_wm_hints (window);
    }
}

gboolean
mx_window_get_has_toolbar (MxWindow *window)
{
  g_return_val_if_fail (MX_IS_WINDOW (window), FALSE);

  return window->priv->has_toolbar;
}

MxToolbar *
mx_window_get_toolbar (MxWindow *window)
{
  g_return_val_if_fail (MX_IS_WINDOW (window), NULL);

  return (MxToolbar*) window->priv->toolbar;
}

gboolean
mx_window_get_small_screen (MxWindow *window)
{
  g_return_val_if_fail (MX_IS_WINDOW (window), FALSE);

  return window->priv->small_screen;
}

void
mx_window_set_small_screen (MxWindow *window, gboolean small_screen)
{
  MxWindowPrivate *priv;

  g_return_if_fail (MX_IS_WINDOW (window));

  priv = window->priv;

  if (priv->small_screen != small_screen)
    {
      ClutterStage *stage = CLUTTER_STAGE (window);
      Window win = clutter_x11_get_stage_window (stage);
      Display *dpy = clutter_x11_get_default_display ();

      priv->small_screen = small_screen;

      /* If there's no window, we're not mapped yet - we'll resize
       * on map.
       */
      if (win == None)
        return;

      /* In case we were in the middle of a move/resize */
      if (priv->is_moving != -1)
        {
          clutter_ungrab_pointer_for_device (priv->is_moving);
          clutter_set_motion_events_enabled (TRUE);
          priv->is_moving = -1;
          if (priv->is_resizing)
            {
              XUndefineCursor (dpy, win);
              priv->is_resizing = FALSE;
            }
        }

      if (small_screen)
        {
          if (!clutter_stage_get_fullscreen (CLUTTER_STAGE (window)))
            {
              int width, height;
              XRRScreenResources *res;

              clutter_actor_get_size (CLUTTER_ACTOR (window),
                                      &priv->last_width,
                                      &priv->last_height);

              /* Move/size ourselves to the size of the screen. We could
               * also set ourselves as not resizable, but a WM that respects
               * our small-screen mode won't give the user controls to
               * modify the window, and if it does, just let them.
               */
              res = XRRGetScreenResourcesCurrent (dpy, win);
              width = res->modes[res->nmode].width;
              height = res->modes[res->nmode].height;
              XRRFreeScreenResources (res);

              XMoveResizeWindow (dpy, win, 0, 0, width, height);
            }
        }
      else
        {
          /* If we started off in small-screen mode, our last size won't
           * be known, so use the preferred size.
           */
          if (!priv->last_width && !priv->last_height)
            mx_window_get_size (window,
                                NULL, NULL,
                                &priv->last_width, &priv->last_height);

          clutter_actor_set_size (CLUTTER_ACTOR (window),
                                  priv->last_width,
                                  priv->last_height);
        }

      g_object_notify (G_OBJECT (window), "small-screen");
    }
}

void
mx_window_get_window_position (MxWindow *window, gint *x, gint *y)
{
  unsigned int width, height, border_width, depth;
  MxWindowPrivate *priv;
  Window win, root_win;
  ClutterStage *stage;
  int win_x, win_y;
  Display *dpy;

  g_return_if_fail (MX_IS_WINDOW (window));

  priv = window->priv;
  stage = CLUTTER_STAGE (window);

  if (priv->small_screen || clutter_stage_get_fullscreen (stage))
    {
      if (x)
        *x = 0;
      if (y)
        *y = 0;
      return;
    }

  win = clutter_x11_get_stage_window (stage);
  dpy = clutter_x11_get_default_display ();

  XGetGeometry (dpy, win,
                &root_win,
                &win_x, &win_y,
                &width, &height,
                &border_width,
                &depth);

  if (x)
    *x = win_x;
  if (y)
    *y = win_y;
}

void
mx_window_set_window_position (MxWindow *window, gint x, gint y)
{
  Window win;
  Display *dpy;
  ClutterStage *stage;
  MxWindowPrivate *priv;

  g_return_if_fail (MX_IS_WINDOW (window));

  priv = window->priv;
  stage = CLUTTER_STAGE (window);

  /* Don't try to move a full-screen/small-screen window */
  if (priv->small_screen || clutter_stage_get_fullscreen (stage))
    return;

  win = clutter_x11_get_stage_window (stage);
  dpy = clutter_x11_get_default_display ();

  XMoveWindow (dpy, win, x, y);
}

void
mx_window_set_icon_name (MxWindow *window, const gchar *icon_name)
{
  MxWindowPrivate *priv;

  g_return_if_fail (MX_IS_WINDOW (window));

  priv = window->priv;

  if (priv->icon_name && icon_name && g_str_equal (priv->icon_name, icon_name))
    return;
  if (!priv->icon_name && !icon_name)
    return;

  g_free (priv->icon_name);
  priv->icon_name = g_strdup (icon_name);

  g_object_notify (G_OBJECT (window), "icon-name");

  mx_window_set_wm_hints (window);
}

const gchar *
mx_window_get_icon_name (MxWindow *window)
{
  g_return_val_if_fail (MX_IS_WINDOW (window), NULL);
  return window->priv->icon_name;
}

ClutterStage *
mx_window_get_clutter_stage (MxWindow *window)
{
  g_return_val_if_fail (MX_IS_WINDOW (window), NULL);

  return CLUTTER_STAGE (window);
}

