/*
 * Copyright 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

/**
 * SECTION:mx-types
 * @short_description: type definitions used throughout Mx
 *
 * Common types for MxWidgets.
 */


#if !defined(MX_H_INSIDE) && !defined(MX_COMPILATION)
#error "Only <mx/mx.h> can be included directly.h"
#endif

#ifndef __MX_TYPES_H__
#define __MX_TYPES_H__

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define MX_TYPE_BORDER_IMAGE          (mx_border_image_get_type ())
#define MX_TYPE_PADDING               (mx_padding_get_type ())

typedef struct _MxBorderImage MxBorderImage;
typedef struct _MxPadding     MxPadding;


GType mx_border_image_get_type (void) G_GNUC_CONST;

/**
 * MxBorderImage:
 * @uri: uri of a supported image file
 *
 * @top: top border slice width
 * @right: right border slice width
 * @bottom: bottom border slice width
 * @left: bottom border slice width
 */
struct _MxBorderImage
{
  gchar *uri;

  gint top;
  gint right;
  gint bottom;
  gint left;
};

/**
 * MxPadding:
 * @top: padding from the top
 * @right: padding from the right
 * @bottom: padding from the bottom
 * @left: padding from the left
 *
 * The padding from the internal border of the parent container.
 */
struct _MxPadding
{
  gfloat top;
  gfloat right;
  gfloat bottom;
  gfloat left;
};

GType mx_padding_get_type (void) G_GNUC_CONST;



typedef enum {
  MX_ALIGN_START,
  MX_ALIGN_MIDDLE,
  MX_ALIGN_END
} MxAlign;

G_END_DECLS

#endif /* __MX_TYPES_H__ */
