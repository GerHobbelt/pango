/* Pango
 * pangocairo-atsuifont.c
 *
 * Copyright (C) 2000-2005 Red Hat Software
 * Copyright (C) 2005-2007 Imendio AB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#import <Cocoa/Cocoa.h>

#include "pangoatsui-private.h"
#include "pangocairo.h"
#include "pangocairo-private.h"
#include "pangocairo-atsui.h"
#include "pangocairo-atsuifont.h"

struct _PangoCairoATSUIFont
{
  PangoATSUIFont font;

  ATSUFontID font_id;
  cairo_font_face_t *font_face;
  cairo_scaled_font_t *scaled_font;
  cairo_matrix_t font_matrix;
  cairo_matrix_t ctm;
  cairo_font_options_t *options;

  double size;
  int absolute_size;

  GSList *metrics_by_lang;
};

struct _PangoCairoATSUIFontClass
{
  PangoATSUIFontClass parent_class;
};



static cairo_font_face_t *pango_cairo_atsui_font_get_font_face (PangoCairoFont *font);

/**
 * pango_cairo_atsui_font_get_atsu_font_id:
 * @cafont: A #PangoCairoATSUIFont
 *
 * Returns the ATSUFontID of a font.
 *
 * Return value: the ATSUFontID associated to @cafont.
 *
 * Since: 1.12
 */
ATSUFontID
pango_cairo_atsui_font_get_atsu_font_id (PangoCairoATSUIFont *cafont)
{
  return cafont->font_id;
}

static gboolean
pango_cairo_atsui_font_install (PangoCairoFont *font,
				cairo_t        *cr)
{
  PangoCairoATSUIFont *cafont = PANGO_CAIRO_ATSUI_FONT (font);

  cairo_set_font_face (cr,
		       pango_cairo_atsui_font_get_font_face (font));

  cairo_set_font_matrix (cr, &cafont->font_matrix);
  cairo_set_font_options (cr, cafont->options);

  return TRUE;
}

static cairo_font_face_t *
pango_cairo_atsui_font_get_font_face (PangoCairoFont *font)
{
  PangoCairoATSUIFont *cafont = PANGO_CAIRO_ATSUI_FONT (font);

  if (!cafont->font_face)
    {
      cafont->font_face = cairo_atsui_font_face_create_for_atsu_font_id (cafont->font_id);

      /* Failure of the above should only occur for out of memory,
       * we can't proceed at that point
       */
      if (!cafont->font_face)
	g_error ("Unable to create ATSUI cairo font face.\nThis means out of memory or a cairo/fontconfig/FreeType bug");
    }

  return cafont->font_face;
}

static cairo_scaled_font_t *
pango_cairo_atsui_font_get_scaled_font (PangoCairoFont *font)
{
  PangoCairoATSUIFont *cafont = PANGO_CAIRO_ATSUI_FONT (font);

  if (!cafont->scaled_font)
    {
      cairo_font_face_t *font_face;

      font_face = pango_cairo_atsui_font_get_font_face (font);
      cafont->scaled_font = cairo_scaled_font_create (font_face,
						      &cafont->font_matrix,
						      &cafont->ctm,
						      cafont->options);

      /* Failure of the above should only occur for out of memory,
       * we can't proceed at that point
       */
      if (!cafont->scaled_font)
	g_error ("Unable to create ATSUI cairo scaled font.\nThis means out of memory or a cairo/fontconfig/FreeType bug");
    }

  return cafont->scaled_font;
}

static void
cairo_font_iface_init (PangoCairoFontIface *iface)
{
  iface->install = pango_cairo_atsui_font_install;
  iface->get_font_face = pango_cairo_atsui_font_get_font_face;
  iface->get_scaled_font = pango_cairo_atsui_font_get_scaled_font;
}

G_DEFINE_TYPE_WITH_CODE (PangoCairoATSUIFont, pango_cairo_atsui_font, PANGO_TYPE_ATSUI_FONT,
    { G_IMPLEMENT_INTERFACE (PANGO_TYPE_CAIRO_FONT, cairo_font_iface_init) });

static void
pango_cairo_atsui_font_get_glyph_extents (PangoFont        *font,
					  PangoGlyph        glyph,
					  PangoRectangle   *ink_rect,
					  PangoRectangle   *logical_rect)
{
  PangoCairoFont *cfont = (PangoCairoFont *)font;
  cairo_scaled_font_t *scaled_font;
  cairo_font_extents_t font_extents;
  cairo_text_extents_t extents;
  cairo_glyph_t cairo_glyph;

  scaled_font = pango_cairo_atsui_font_get_scaled_font (PANGO_CAIRO_FONT (font));

  if (logical_rect)
    cairo_scaled_font_extents (scaled_font, &font_extents);

  cairo_glyph.index = glyph;
  cairo_glyph.x = 0;
  cairo_glyph.y = 0;

  if (glyph == PANGO_GLYPH_EMPTY)
    {
      if (ink_rect)
	{
	  ink_rect->x = ink_rect->y = ink_rect->width = ink_rect->height = 0;
	}
      if (logical_rect)
	{
	  logical_rect->x = 0;
	  logical_rect->y = - font_extents.ascent * PANGO_SCALE;
	  logical_rect->width = 0;
	  logical_rect->height = (font_extents.ascent + font_extents.descent) * PANGO_SCALE;
	}
    }
  else if (glyph & PANGO_GLYPH_UNKNOWN_FLAG)
    {
      /* space for the hex box */
      _pango_cairo_get_glyph_extents_missing(cfont, glyph, ink_rect, logical_rect);
    }
  else
    {
      cairo_scaled_font_glyph_extents (scaled_font,
				       &cairo_glyph, 1, &extents);

      if (ink_rect)
	{
	  ink_rect->x = extents.x_bearing * PANGO_SCALE;
	  ink_rect->y = extents.y_bearing * PANGO_SCALE;
	  ink_rect->width = extents.width * PANGO_SCALE;
	  ink_rect->height = extents.height * PANGO_SCALE;
	}

      if (logical_rect)
	{
	  logical_rect->x = 0;
	  logical_rect->y = - font_extents.ascent * PANGO_SCALE;
	  logical_rect->width = extents.x_advance * PANGO_SCALE;
	  logical_rect->height = (font_extents.ascent + font_extents.descent) * PANGO_SCALE;
	}
    }
}

static int
max_glyph_width (PangoLayout *layout)
{
  int max_width = 0;
  GSList *l, *r;

  for (l = pango_layout_get_lines_readonly (layout); l; l = l->next)
    {
      PangoLayoutLine *line = l->data;

      for (r = line->runs; r; r = r->next)
	{
	  PangoGlyphString *glyphs = ((PangoGlyphItem *)r->data)->glyphs;
	  int i;

	  for (i = 0; i < glyphs->num_glyphs; i++)
	    if (glyphs->glyphs[i].geometry.width > max_width)
	      max_width = glyphs->glyphs[i].geometry.width;
	}
    }

  return max_width;
}

static PangoFontMetrics *
create_metrics_for_context (PangoFont    *font,
			    PangoContext *context)
{
  PangoCairoATSUIFont *cafont = PANGO_CAIRO_ATSUI_FONT (font);
  ATSFontRef ats_font;
  ATSFontMetrics ats_metrics;
  PangoFontMetrics *metrics;
  PangoFontDescription *font_desc;
  PangoLayout *layout;
  PangoRectangle extents;
  PangoLanguage *language = pango_context_get_language (context);
  const char *sample_str = pango_language_get_sample_string (language);

  ats_font = FMGetATSFontRefFromFont (cafont->font_id);

  ATSFontGetHorizontalMetrics (ats_font, kATSOptionFlagsDefault, &ats_metrics);

  metrics = pango_font_metrics_new ();

  metrics->ascent = ats_metrics.ascent * cafont->size * PANGO_SCALE;
  metrics->descent = -ats_metrics.descent * cafont->size * PANGO_SCALE;

  metrics->underline_position = ats_metrics.underlinePosition * cafont->size * PANGO_SCALE;
  metrics->underline_thickness = ats_metrics.underlineThickness * cafont->size * PANGO_SCALE;

  metrics->strikethrough_position = metrics->ascent / 3;
  metrics->strikethrough_thickness = ats_metrics.underlineThickness * cafont->size * PANGO_SCALE;

  layout = pango_layout_new (context);
  font_desc = pango_font_describe_with_absolute_size (font);
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_set_text (layout, sample_str, -1);
  pango_layout_get_extents (layout, NULL, &extents);

  metrics->approximate_char_width = extents.width / g_utf8_strlen (sample_str, -1);

  pango_layout_set_text (layout, "0123456789", -1);
  metrics->approximate_digit_width = max_glyph_width (layout);

  pango_font_description_free (font_desc);
  g_object_unref (layout);

  return metrics;
}

typedef struct
{
  const char *sample_str;
  PangoFontMetrics *metrics;
} PangoATSUIMetricsInfo;

static PangoFontMetrics *
pango_cairo_atsui_font_get_metrics (PangoFont        *font,
				    PangoLanguage    *language)
{
  PangoCairoATSUIFont *cafont = PANGO_CAIRO_ATSUI_FONT (font);
  PangoATSUIMetricsInfo *info;
  GSList *tmp_list;
  const char *sample_str = pango_language_get_sample_string (language);

  tmp_list = cafont->metrics_by_lang;
  while (tmp_list)
    {
      info = tmp_list->data;

      if (info->sample_str == sample_str)    /* We _don't_ need strcmp */
	break;

      tmp_list = tmp_list->next;
    }

  if (!tmp_list)
    {
      PangoContext *context;

      if (!pango_font_get_font_map (font))
	return pango_font_metrics_new ();

      info = g_slice_new0 (PangoATSUIMetricsInfo);

      cafont->metrics_by_lang = g_slist_prepend (cafont->metrics_by_lang,
						 info);

      info->sample_str = sample_str;

      context = pango_context_new ();
      pango_context_set_font_map (context, pango_font_get_font_map (font));
      pango_context_set_language (context, language);
      pango_cairo_context_set_font_options (context, cafont->options);

      info->metrics = create_metrics_for_context (font, context);

      g_object_unref (context);
    }

  return pango_font_metrics_ref (info->metrics);
}

static PangoFontDescription *
pango_cairo_atsui_font_describe_absolute (PangoFont *font)
{
  PangoFontDescription *desc;
  PangoATSUIFont *afont = PANGO_ATSUI_FONT (font);
  PangoCairoATSUIFont *cafont = PANGO_CAIRO_ATSUI_FONT (font);

  desc = pango_font_description_copy (afont->desc);
  pango_font_description_set_absolute_size (desc, cafont->absolute_size);

  return desc;
}

static void
free_metrics_info (PangoATSUIMetricsInfo *info)
{
  pango_font_metrics_unref (info->metrics);
  g_slice_free (PangoATSUIMetricsInfo, info);
}

static void
pango_cairo_atsui_font_finalize (GObject *object)
{
  PangoCairoATSUIFont *cafont = PANGO_CAIRO_ATSUI_FONT (object);

  g_slist_foreach (cafont->metrics_by_lang, (GFunc)free_metrics_info, NULL);
  g_slist_free (cafont->metrics_by_lang);

  cairo_font_face_destroy (cafont->font_face);
  cairo_scaled_font_destroy (cafont->scaled_font);
  cairo_font_options_destroy (cafont->options);

  G_OBJECT_CLASS (pango_cairo_atsui_font_parent_class)->finalize (object);
}

static void
pango_cairo_atsui_font_class_init (PangoCairoATSUIFontClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PangoFontClass *font_class = PANGO_FONT_CLASS (class);

  object_class->finalize = pango_cairo_atsui_font_finalize;

  font_class->get_metrics = pango_cairo_atsui_font_get_metrics;
  font_class->get_glyph_extents = pango_cairo_atsui_font_get_glyph_extents;
  font_class->describe_absolute = pango_cairo_atsui_font_describe_absolute;
}

static void
pango_cairo_atsui_font_init (PangoCairoATSUIFont *cafont)
{
}

PangoATSUIFont *
_pango_cairo_atsui_font_new (PangoCairoATSUIFontMap     *cafontmap,
			     PangoContext               *context,
			     PangoATSUIFace             *face,
			     const PangoFontDescription *desc)
{
  const char *postscript_name;
  gboolean synthesize_italic = FALSE;
  PangoCairoATSUIFont *cafont;
  PangoATSUIFont *afont;
  CFStringRef cfstr;
  ATSFontRef font_ref;
  const PangoMatrix *pango_ctm;
  ATSUFontID font_id;
  double dpi;

  postscript_name = _pango_atsui_face_get_postscript_name (face);
  
  cfstr = CFStringCreateWithCString (NULL, postscript_name,
				     kCFStringEncodingUTF8);
  font_ref = ATSFontFindFromPostScriptName (cfstr, kATSOptionFlagsDefault);
  CFRelease (cfstr);

  /* We synthesize italic in two cases. The first is when
   * NSFontManager has handed out a face that it claims has italic but
   * it doesn't. The other is when an italic face is requested that
   * doesn't have a real version.
   */
  if (font_ref == kATSFontRefUnspecified && 
      pango_font_description_get_style (desc) != PANGO_STYLE_NORMAL)
    {
      NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
      NSString *nsname;
      NSFont *nsfont, *converted_font;
      gdouble size;

      size = (double) pango_font_description_get_size (desc) / PANGO_SCALE;

      nsname = [NSString stringWithUTF8String:postscript_name];
      nsfont = [NSFont fontWithName:nsname size:size];

      converted_font = [[NSFontManager sharedFontManager] convertFont:nsfont
			toHaveTrait:NSUnitalicFontMask];
      font_ref = ATSFontFindFromPostScriptName ((CFStringRef) [converted_font fontName],
						kATSOptionFlagsDefault);

      [pool release];

      synthesize_italic = TRUE;
    }
  else if (_pango_atsui_face_get_synthetic_italic (face))
    synthesize_italic = TRUE;

  if (font_ref == kATSFontRefUnspecified)
    return NULL;

  font_id = FMGetFontFromATSFontRef (font_ref);
  if (!font_id)
    return NULL;

  cafont = g_object_new (PANGO_TYPE_CAIRO_ATSUI_FONT, NULL);
  afont = PANGO_ATSUI_FONT (cafont);

  afont->desc = pango_font_description_copy (desc);
  afont->face = face;

  cafont->size = (double) pango_font_description_get_size (desc) / PANGO_SCALE;
  cafont->font_id = font_id;

  if (context)
    {
      dpi = pango_cairo_context_get_resolution (context);

      if (dpi <= 0)
	dpi = cafontmap->dpi;
    }
  else
    dpi = cafontmap->dpi;

  cafont->absolute_size = cafont->size;

  if (!pango_font_description_get_size_is_absolute (desc))
    cafont->size *= dpi / 72.;

  cairo_matrix_init_scale (&cafont->font_matrix, cafont->size, cafont->size);
  pango_ctm = pango_context_get_matrix (context);
  if (pango_ctm)
    cairo_matrix_init (&cafont->ctm,
		       pango_ctm->xx,
		       pango_ctm->yx,
		       pango_ctm->xy,
		       pango_ctm->yy,
		       0., 0.);
  else
    cairo_matrix_init_identity (&cafont->ctm);

  if (synthesize_italic)
    {
      cairo_matrix_t shear, result;

      /* Apply a shear matrix, matching what Cocoa does. */
      cairo_matrix_init (&shear,
			 1, 0, 
			 0.25, 1,
			 0, 0);

      cairo_matrix_multiply (&result, &shear, &cafont->font_matrix);
      cafont->font_matrix = result;
    }

  cafont->options = cairo_font_options_copy (_pango_cairo_context_get_merged_font_options (context));

  return afont;
}
