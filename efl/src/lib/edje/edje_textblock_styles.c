#include "edje_private.h"
#include "ui_profiler.h"

#define _TTRACE_EDJE_

static int
_edje_font_is_embedded(Edje_File *edf, char *font)
{
   if (!eina_hash_find(edf->fonts, font)) return 0;
   return 1;
}

static void
_edje_format_param_parse(char *item, char **key, char **val)
{
   char *p, *k, *v;

   p = strchr(item, '=');
   if (!p) return;

   k = malloc(p - item + 1);
   strncpy(k, item, p - item);
   k[p - item] = 0;
   *key = k;
   p++;
   v = strdup(p);
   *val = v;
}

static char *
_edje_format_parse(const char **s)
{
   const char *p;
   const char *s1 = NULL;
   const char *s2 = NULL;
   Eina_Bool quote = EINA_FALSE;

   p = *s;
   if ((!p) || (*p == 0)) return NULL;
   for (;;)
     {
        if (!s1)
          {
             if (*p != ' ') s1 = p;
             if (*p == 0) break;
          }
        else if (!s2)
          {
             if (*p == '\'')
               {
                  quote = !quote;
               }

             if ((p > *s) && (p[-1] != '\\') && (!quote))
               {
                  if (*p == ' ') s2 = p;
               }
             if (*p == 0) s2 = p;
          }
        p++;
        if (s1 && s2 && (s2 > s1))
          {
             size_t len = s2 - s1;
             char *ret = malloc(len + 1);
             memcpy(ret, s1, len);
             ret[len] = '\0';
             *s = s2;
             return ret;
          }
     }
   *s = p;
   return NULL;
}

static int
_edje_format_is_param(char *item)
{
   if (strchr(item, '=')) return 1;
   return 0;
}

static char *
_edje_format_reparse(Edje_File *edf, const char *str, Edje_Style_Tag **tag_ret)
{
   Eina_Strbuf *txt, *tmp = NULL;
   char *s2, *item, *ret;
   const char *s;

   txt = eina_strbuf_new();
   s = str;
   while ((item = _edje_format_parse(&s)))
     {
	if (_edje_format_is_param(item))
	  {
	     char *key = NULL, *val = NULL;

	     _edje_format_param_parse(item, &key, &val);
	     if (!strcmp(key, "font_source"))
	       {
		  /* dont allow font sources */
	       }
	     else if (!strcmp(key, "text_class"))
	       {
		  if (tag_ret)
		    (*tag_ret)->text_class = eina_stringshare_add(val);
	       }
	     else if (!strcmp(key, "font_size"))
	       {
		  if (tag_ret)
		    (*tag_ret)->font_size = atof(val);
	       }
	     else if (!strcmp(key, "font")) /* Fix fonts */
	       {
		  if (tag_ret)
		    {
		       if (_edje_font_is_embedded(edf, val))
			 {
			    if (!tmp)
			      tmp = eina_strbuf_new();
			    eina_strbuf_append(tmp, "edje/fonts/");
			    eina_strbuf_append(tmp, val);
			    (*tag_ret)->font = eina_stringshare_add(eina_strbuf_string_get(tmp));
			    eina_strbuf_reset(tmp);
			 }
		       else
			 {
			    (*tag_ret)->font = eina_stringshare_add(val);
			 }
		    }
	       }
#ifdef EFL_FEATURE_CHANGEABLE_UI
             else if (!strcmp(key, "color_class"))
               {
                  if (tag_ret)
                    (*tag_ret)->color_class = eina_stringshare_add(val);
               }
             else if (!strcmp(key, "color"))
               {
                  if (tag_ret)
                    (*tag_ret)->color = eina_stringshare_add(val);
               }
             else if (!strcmp(key, "outline_color"))
               {
                  if (tag_ret)
                    (*tag_ret)->outline_color = eina_stringshare_add(val);
               }
             else if (!strcmp(key, "shadow_color"))
               {
                  if (tag_ret)
                    (*tag_ret)->shadow_color = eina_stringshare_add(val);
               }
#endif
             s2 = eina_str_escape(item);
             if (s2)
               {
                  if (eina_strbuf_length_get(txt)) eina_strbuf_append(txt, " ");
                  eina_strbuf_append(txt, s2);
                  free(s2);
               }
	     free(key);
	     free(val);
	  }
	else
	  {
	     if (eina_strbuf_length_get(txt)) eina_strbuf_append(txt, " ");
	     eina_strbuf_append(txt, item);
	  }
        free(item);
     }
   if (tmp)
     eina_strbuf_free(tmp);
   ret = eina_strbuf_string_steal(txt);
   eina_strbuf_free(txt);
   return ret;
}

/* Update all evas_styles which are in an edje
 *
 * @param ed The edje containing styles which need to be updated
 */
void
_edje_textblock_style_all_update(Edje *ed)
{
   Eina_List *l, *ll;
   Edje_Style *stl;
   Eina_Strbuf *txt = NULL;

#ifdef EFL_FEATURE_CHANGEABLE_UI
   EINA_LIST_FOREACH(ed->styles, l, stl)
#else
   if (!ed->file) return;

   EINA_LIST_FOREACH(ed->file->styles, l, stl)
#endif
     {
        Edje_Style_Tag *tag;
        Edje_Text_Class *tc;
#ifdef EFL_FEATURE_CHANGEABLE_UI
        Edje_Color_Class *cc;
#endif
        int found = 0;
        char *fontset = NULL, *fontsource = NULL;

        /* Make sure the style is already defined */
        if (!stl->style) break;

        /* No need to compute it again and again and again */
	if (stl->cache) continue;

        /* Make sure the style contains a text_class */
        EINA_LIST_FOREACH(stl->tags, ll, tag)
          {
             if (tag->text_class)
               {
                  found = 1;
                  break;
               }
#ifdef EFL_FEATURE_CHANGEABLE_UI
             if (tag->color_class)
               {
                  found = 1;
                  break;
               }
#endif
          }

        /* No text classes , goto next style */
        if (!found) continue;
        if (!txt)
          txt = eina_strbuf_new();

        if (_edje_fontset_append)
          fontset = eina_str_escape(_edje_fontset_append);
        fontsource = eina_str_escape(ed->file->path);

        /* Build the style from each tag */
        EINA_LIST_FOREACH(stl->tags, ll, tag)
          {
             if (!tag->key) continue;

             /* Add Tag Key */
             eina_strbuf_append(txt, tag->key);
             eina_strbuf_append(txt, "='");

             /* Configure fonts from text class if it exists */
             tc = _edje_text_class_find(ed, tag->text_class);
#ifdef EFL_FEATURE_CHANGEABLE_UI
             /* Configure color from color class if it exists */
             cc = _edje_color_class_find(ed, tag->color_class);
#endif

             /* Add and Handle tag parsed data */
             eina_strbuf_append(txt, tag->value);

             if (!strcmp(tag->key, "DEFAULT"))
               {
                  if (fontset)
                    {
                       eina_strbuf_append(txt, " ");
                       eina_strbuf_append(txt, "font_fallbacks=");
                       eina_strbuf_append(txt, fontset);
                    }
                  eina_strbuf_append(txt, " ");
                  eina_strbuf_append(txt, "font_source=");
                  eina_strbuf_append(txt, fontsource);
               }
             if (tag->font_size != 0)
               {
                  char font_size[32];

                  if (tc && tc->size)
                    snprintf(font_size, sizeof(font_size), "%f",
                             (double) _edje_text_size_calc(tag->font_size, tc));
                  else
                    snprintf(font_size, sizeof(font_size), "%f",
                             tag->font_size);

                  eina_strbuf_append(txt, " ");
                  eina_strbuf_append(txt, "font_size=");
                  eina_strbuf_append(txt, font_size);
               }
#ifdef EFL_FEATURE_CHANGEABLE_UI
             if (tag->color)
               {
                  const char *f;
                  char *ncolor = NULL;

                  eina_strbuf_append(txt, " ");
                  eina_strbuf_append(txt, "color=");

                  if (cc) f = _edje_color_calc(tag->color,
                                               cc->r, cc->g, cc->b, cc->a,
                                               &ncolor);
                  else f = tag->color;

                  eina_strbuf_append_escaped(txt, f);
                  if (ncolor) free(ncolor);
               }
             if (tag->outline_color)
               {
                  const char *f;
                  char *ncolor = NULL;

                  eina_strbuf_append(txt, " ");
                  eina_strbuf_append(txt, "outline_color=");

                  if (cc) f = _edje_color_calc(tag->outline_color,
                                               cc->r2, cc->g2, cc->b2, cc->a2,
                                               &ncolor);
                  else f = tag->outline_color;

                  eina_strbuf_append_escaped(txt, f);
                  if (ncolor) free(ncolor);
               }
             if (tag->shadow_color)
               {
                  const char *f;
                  char *ncolor = NULL;

                  eina_strbuf_append(txt, " ");
                  eina_strbuf_append(txt, "shadow_color=");

                  if (cc) f = _edje_color_calc(tag->shadow_color,
                                               cc->r3, cc->g3, cc->b3, cc->a3,
                                               &ncolor);
                  else f = tag->shadow_color;

                  eina_strbuf_append_escaped(txt, f);
                  if (ncolor) free(ncolor);
               }
#endif
             /* Add font name last to save evas from multiple loads */
             if (tag->font)
               {
                  const char *f;
                  char *sfont = NULL;

                  eina_strbuf_append(txt, " ");
                  eina_strbuf_append(txt, "font=");

                  if (tc) f = _edje_text_font_get(tag->font, tc->font, &sfont);
                  else f = tag->font;

                  eina_strbuf_append_escaped(txt, f);

                  if (sfont) free(sfont);
               }

             eina_strbuf_append(txt, "'");
          }
        if (fontset) free(fontset);
        if (fontsource) free(fontsource);

        /* Configure the style */
        stl->cache = EINA_TRUE;
        evas_textblock_style_set(stl->style, eina_strbuf_string_get(txt));
        eina_strbuf_reset(txt);
     }
   if (txt)
     eina_strbuf_free(txt);
}

#ifdef EFL_FEATURE_CHANGEABLE_UI
void
_edje_file_textblock_style_all_update(Edje_File *edf)
{
   Eina_List *l, *ll;
   Edje_Style *stl;
   Eina_Strbuf *txt = NULL;

   if (!edf) return;

   EINA_LIST_FOREACH(edf->styles, l, stl)
     {
        Edje_Style_Tag *tag;
        Edje_Text_Class *tc;
        Edje_Color_Class *cc;
        int found = 0;
        char *fontset = NULL, *fontsource = NULL;

        /* Make sure the style is already defined */
        if (!stl->style) break;

        /* No need to compute it again and again and again */
        if (stl->cache) continue;

        /* Make sure the style contains a text_class */
        EINA_LIST_FOREACH(stl->tags, ll, tag)
          {
             if (tag->text_class)
               {
                  found = 1;
                  break;
               }
             if (tag->color_class)
               {
                  found = 1;
                  break;
               }
          }

        /* No text classes or color classes, goto next style */
        if (!found) continue;
        if (!txt)
          txt = eina_strbuf_new();

        if (_edje_fontset_append)
          fontset = eina_str_escape(_edje_fontset_append);
        fontsource = eina_str_escape(edf->path);

        /* Build the style from each tag */
        EINA_LIST_FOREACH(stl->tags, ll, tag)
          {
             if (!tag->key) continue;

             /* Add Tag Key */
             eina_strbuf_append(txt, tag->key);
             eina_strbuf_append(txt, "='");

             /* Configure fonts from text class if it exists */
             tc = _edje_text_class_find(NULL, tag->text_class);
             /* Configure color from color class if it exists */
             cc = _edje_color_class_find(NULL, tag->color_class);

             /* Add and Ha`ndle tag parsed data */
             eina_strbuf_append(txt, tag->value);

             if (!strcmp(tag->key, "DEFAULT"))
               {
                  if (fontset)
                    {
                       eina_strbuf_append(txt, " ");
                       eina_strbuf_append(txt, "font_fallbacks=");
                       eina_strbuf_append(txt, fontset);
                    }
                  eina_strbuf_append(txt, " ");
                  eina_strbuf_append(txt, "font_source=");
                  eina_strbuf_append(txt, fontsource);
               }
             if (tag->font_size != 0)
               {
                  char font_size[32];

                  if (tc && tc->size)
                    snprintf(font_size, sizeof(font_size), "%f",
                             (double) _edje_text_size_calc(tag->font_size, tc));
                  else
                    snprintf(font_size, sizeof(font_size), "%f",
                             tag->font_size);

                  eina_strbuf_append(txt, " ");
                  eina_strbuf_append(txt, "font_size=");
                  eina_strbuf_append(txt, font_size);
               }
             if (tag->color)
               {
                  const char *f;
                  char *ncolor = NULL;

                  eina_strbuf_append(txt, " ");
                  eina_strbuf_append(txt, "color=");

                  if (cc) f = _edje_color_calc(tag->color,
                                               cc->r, cc->g, cc->b, cc->a,
                                               &ncolor);
                  else f = tag->color;

                  eina_strbuf_append_escaped(txt, f);
                  if (ncolor) free(ncolor);
               }
             if (tag->outline_color)
               {
                  const char *f;
                  char *ncolor = NULL;

                  eina_strbuf_append(txt, " ");
                  eina_strbuf_append(txt, "outline_color=");

                  if (cc) f = _edje_color_calc(tag->outline_color,
                                               cc->r2, cc->g2, cc->b2, cc->a2,
                                               &ncolor);
                  else f = tag->outline_color;

                  eina_strbuf_append_escaped(txt, f);
                  if (ncolor) free(ncolor);
               }
             if (tag->shadow_color)
               {
                  const char *f;
                  char *ncolor = NULL;

                  eina_strbuf_append(txt, " ");
                  eina_strbuf_append(txt, "shadow_color=");

                  if (cc) f = _edje_color_calc(tag->shadow_color,
                                               cc->r3, cc->g3, cc->b3, cc->a3,
                                               &ncolor);
                  else f = tag->shadow_color;

                  eina_strbuf_append_escaped(txt, f);
                  if (ncolor) free(ncolor);
               }
             /* Add font name last to save evas from multiple loads */
             if (tag->font)
               {
                  const char *f;
                  char *sfont = NULL;

                  eina_strbuf_append(txt, " ");
                  eina_strbuf_append(txt, "font=");

                  if (tc) f = _edje_text_font_get(tag->font, tc->font, &sfont);
                  else f = tag->font;

                  eina_strbuf_append_escaped(txt, f);

                  if (sfont) free(sfont);
               }

             eina_strbuf_append(txt, "'");
          }
        if (fontset) free(fontset);
        if (fontsource) free(fontsource);

        /* Configure the style */
        stl->cache = EINA_TRUE;
        evas_textblock_style_set(stl->style, eina_strbuf_string_get(txt));
        eina_strbuf_reset(txt);
     }
   if (txt)
     eina_strbuf_free(txt);
}

static Edje_Style *
_edje_textblock_style_copy(Edje_Style *stl)
{
   Edje_Style *new_stl;
   Eina_List *l;
   Edje_Style_Tag *tag;
   Edje_Style_Tag *new_tag;

   new_stl = calloc(1, sizeof(Edje_Style));
   if (!new_stl) return NULL;

   new_stl->name = (char *)eina_stringshare_add(stl->name);
   new_stl->style = evas_textblock_style_new();
   evas_textblock_style_set(new_stl->style, NULL);

   EINA_LIST_FOREACH(stl->tags, l, tag)
     {
        new_tag = calloc(1, sizeof(Edje_Style_Tag));
        if (!new_tag) continue;

        new_tag->key = eina_stringshare_add(tag->key);
        new_tag->value = eina_stringshare_add(tag->value);
        new_tag->font = eina_stringshare_add(tag->font);
        new_tag->font_size = tag->font_size;
        new_tag->text_class = eina_stringshare_add(tag->text_class);
        new_tag->color_class = eina_stringshare_add(tag->color_class);
        new_tag->color = eina_stringshare_add(tag->color);
        new_tag->outline_color = eina_stringshare_add(tag->outline_color);
        new_tag->shadow_color = eina_stringshare_add(tag->shadow_color);

        new_stl->tags = eina_list_append(new_stl->tags, new_tag);
     }

   return new_stl;
}

static void
_edje_textblock_styles_cache_add(Edje *ed, Edje_Style *stl)
{
   Eina_List *l;
   Edje_Style *new_stl;
   Eina_Bool found;

   if (!stl) return;

   EINA_LIST_FOREACH(ed->styles, l, new_stl)
     {
        found = EINA_FALSE;
        if (!strcmp(stl->name, new_stl->name))
          {
             found = EINA_TRUE;
             break;
          }
     }
   if (!found)
     {
        new_stl = _edje_textblock_style_copy(stl);
        if (new_stl)
          {
             ed->styles = eina_list_append(ed->styles, new_stl);
             _edje_textblock_style_all_update(ed);
          }
     }
}

static void
_edje_textblock_styles_cache_del(Edje *ed, Edje_Style *stl)
{
   Eina_List *l;
   Edje_Style *new_stl;
   Eina_Bool found;

   if (!stl) return;

   EINA_LIST_FOREACH(ed->styles, l, new_stl)
     {
        found = EINA_FALSE;
        if (!strcmp(stl->name, new_stl->name))
          {
             found = EINA_TRUE;
             break;
          }
     }
   if (found)
     {
        Edje_Style_Tag *tag;

        EINA_LIST_FREE(new_stl->tags, tag)
          {
             if (tag->value)  eina_stringshare_del(tag->value);
             if (tag->key) eina_stringshare_del(tag->key);
             if (tag->text_class) eina_stringshare_del(tag->text_class);
             if (tag->font) eina_stringshare_del(tag->font);
             if (tag->color_class) eina_stringshare_del(tag->color_class);
             if (tag->color) eina_stringshare_del(tag->color);
             if (tag->outline_color) eina_stringshare_del(tag->outline_color);
             if (tag->shadow_color) eina_stringshare_del(tag->shadow_color);
             free(tag);
          }
        if (new_stl->name) eina_stringshare_del(new_stl->name);
        if (new_stl->style) evas_textblock_style_free(new_stl->style);
        free(new_stl);

        ed->styles = eina_list_remove_list(ed->styles, l);
     }
}
#endif

static inline Edje_Style *
_edje_textblock_style_search(Edje *ed, const char *style)
{
   Edje_Style *stl = NULL;
   Eina_List *l;

   if (!style) return NULL;

   EINA_LIST_FOREACH(ed->file->styles, l, stl)
     {
        if ((stl->name) &&
            (stl->name == style || !strcmp(stl->name, style))) break;
        stl = NULL;
     }

   return stl;
}

static inline void
_edje_textblock_style_member_add(Edje *ed, Edje_Style *stl)
{
   Edje_Style_Tag *tag;
   Eina_List *l;

   if (!stl) return ;

   EINA_LIST_FOREACH(stl->tags, l, tag)
     {
        if (tag->text_class)
          _edje_text_class_member_add(ed, tag->text_class);
#ifdef EFL_FEATURE_CHANGEABLE_UI
        if (tag->color_class)
          _edje_color_class_member_add(ed, tag->color_class);

        if (tag->text_class || tag->color_class)
          _edje_textblock_styles_cache_add(ed, stl);
#endif
     }
}

static inline void
_edje_textblock_style_member_del(Edje *ed, Edje_Style *stl)
{
   Edje_Style_Tag *tag;
   Eina_List *l;

   if (!stl) return ;

   EINA_LIST_FOREACH(stl->tags, l, tag)
     {
        if (tag->text_class)
          _edje_text_class_member_del(ed, tag->text_class);
#ifdef EFL_FEATURE_CHANGEABLE_UI
        if (tag->color_class)
          _edje_color_class_member_del(ed, tag->color_class);

        if (tag->text_class || tag->color_class)
          _edje_textblock_styles_cache_del(ed, stl);
#endif
     }
}

void
_edje_textblock_styles_add(Edje *ed, Edje_Real_Part *ep)
{
   Edje_Part *pt = ep->part;
   Edje_Part_Description_Text *desc;
   Edje_Style *stl = NULL;
   const char *style;
   unsigned int i;

   if (pt->type != EDJE_PART_TYPE_TEXTBLOCK) return;

#if defined(_TTRACE_EDJE_)
   UPF_TRACE_BEGIN_F(UPF_TRACE_TAG_EDJE, "%p", ed);
#endif

   /* if text class exists in the textblock styles for this part,
      add the edje to the tc member list */
   desc = (Edje_Part_Description_Text *)pt->default_desc;
   style = edje_string_get(&desc->text.style);
   stl = _edje_textblock_style_search(ed, style);
   _edje_textblock_style_member_add(ed, stl);

   /* If any other classes exist add them */
   for (i = 0; i < pt->other.desc_count; ++i)
     {
        desc = (Edje_Part_Description_Text *)pt->other.desc[i];
        style = edje_string_get(&desc->text.style);
        stl = _edje_textblock_style_search(ed, style);
        _edje_textblock_style_member_add(ed, stl);
     }

#if defined(_TTRACE_EDJE_)
   UPF_TRACE_END(UPF_TRACE_TAG_EDJE);
#endif
}

void
_edje_textblock_styles_del(Edje *ed, Edje_Part *pt)
{
   Edje_Part_Description_Text *desc;
   Edje_Style *stl = NULL;
   const char *style;
   unsigned int i;

   if (pt->type != EDJE_PART_TYPE_TEXTBLOCK) return;

   desc = (Edje_Part_Description_Text *)pt->default_desc;
   style = edje_string_get(&desc->text.style);
   stl = _edje_textblock_style_search(ed, style);
   _edje_textblock_style_member_del(ed, stl);

   for (i = 0; i < pt->other.desc_count; ++i)
     {
        desc = (Edje_Part_Description_Text *)pt->other.desc[i];
        style = edje_string_get(&desc->text.style);
        stl = _edje_textblock_style_search(ed, style);
        _edje_textblock_style_member_del(ed, stl);
     }
}

#ifdef EFL_FEATURE_CHANGEABLE_UI
void
_edje_textblock_styles_cache_free(Edje *ed, const char *class_name)
{
   Eina_List *l, *ll;
   Edje_Style *stl;

   if (!class_name) return;

   EINA_LIST_FOREACH(ed->styles, l, stl)
     {
        Edje_Style_Tag *tag;
        Eina_Bool found = EINA_FALSE;

        EINA_LIST_FOREACH(stl->tags, ll, tag)
          {
             if (!tag->text_class && !tag->color_class) continue;

             if (tag->text_class && !strcmp(tag->text_class, class_name))
               {
                  found = EINA_TRUE;
                  break;
               }
             if (tag->color_class && !strcmp(tag->color_class, class_name))
               {
                  found = EINA_TRUE;
                  break;
               }
          }
        if (found)
          stl->cache = EINA_FALSE;
     }
}
#else
void
_edje_textblock_styles_cache_free(Edje *ed, const char *text_class)
{
   Eina_List *l, *ll;
   Edje_Style *stl;

   if (!ed->file) return;
   if (!text_class) return;

   EINA_LIST_FOREACH(ed->file->styles, l, stl)
     {
        Edje_Style_Tag *tag;
        Eina_Bool found = EINA_FALSE;

        EINA_LIST_FOREACH(stl->tags, ll, tag)
          {
             if (!tag->text_class) continue;

             if (!strcmp(tag->text_class, text_class))
               {
                  found = EINA_TRUE;
                  break;
               }
          }
        if (found)
          stl->cache = EINA_FALSE;
     }
}
#endif

/* When we get to here the edje file had been read into memory
 * the name of the style is established as well as the name and
 * data for the tags.  This function will create the Evas_Style
 * object for each style. The style is composed of a base style
 * followed by a list of tags.
 */
void
_edje_textblock_style_parse_and_fix(Edje_File *edf)
{
   Eina_Strbuf *txt = NULL;
   Eina_List *l, *ll;
   Edje_Style *stl;

   EINA_LIST_FOREACH(edf->styles, l, stl)
     {
	Edje_Style_Tag *tag;
	char *fontset = NULL, *fontsource = NULL, *ts;

	if (stl->style) break;

	if (!txt)
	  txt = eina_strbuf_new();

	stl->style = evas_textblock_style_new();
	evas_textblock_style_set(stl->style, NULL);

	if (_edje_fontset_append)
	  fontset = eina_str_escape(_edje_fontset_append);
	fontsource = eina_str_escape(edf->path);

	/* Build the style from each tag */
	EINA_LIST_FOREACH(stl->tags, ll, tag)
	  {
	     if (!tag->key) continue;

	     /* Add Tag Key */
	     eina_strbuf_append(txt, tag->key);
	     eina_strbuf_append(txt, "='");

	     ts = _edje_format_reparse(edf, tag->value, &(tag));

	     /* Add and Handle tag parsed data */
	     if (ts)
	       {
		  if (eet_dictionary_string_check(eet_dictionary_get(edf->ef), tag->value) == 0)
		    eina_stringshare_del(tag->value);
		  tag->value = eina_stringshare_add(ts);
		  eina_strbuf_append(txt, tag->value);
		  free(ts);
	       }

	     if (!strcmp(tag->key, "DEFAULT"))
	       {
		  if (fontset)
		    {
		       eina_strbuf_append(txt, " ");
		       eina_strbuf_append(txt, "font_fallbacks=");
		       eina_strbuf_append(txt, fontset);
		    }
		  eina_strbuf_append(txt, " ");
		  eina_strbuf_append(txt, "font_source=");
		  eina_strbuf_append(txt, fontsource);
	       }
	     if (tag->font_size > 0)
	       {
		  char font_size[32];

		  snprintf(font_size, sizeof(font_size), "%f", tag->font_size);
		  eina_strbuf_append(txt, " ");
		  eina_strbuf_append(txt, "font_size=");
		  eina_strbuf_append(txt, font_size);
	       }
#ifdef EFL_FEATURE_CHANGEABLE_UI
             if (tag->color)
               {
                  eina_strbuf_append(txt, " ");
                  eina_strbuf_append(txt, "color=");
                  eina_strbuf_append_escaped(txt, tag->color);
               }
             if (tag->outline_color)
               {
                  eina_strbuf_append(txt, " ");
                  eina_strbuf_append(txt, "outline_color=");
                  eina_strbuf_append_escaped(txt, tag->outline_color);
               }
             if (tag->shadow_color)
               {
                  eina_strbuf_append(txt, " ");
                  eina_strbuf_append(txt, "shadow_color=");
                  eina_strbuf_append_escaped(txt, tag->shadow_color);
               }
#endif
	     /* Add font name last to save evas from multiple loads */
	     if (tag->font)
	       {
		  eina_strbuf_append(txt, " ");
		  eina_strbuf_append(txt, "font=");
		  eina_strbuf_append_escaped(txt, tag->font);
	       }
	     eina_strbuf_append(txt, "'");
	  }
	if (fontset) free(fontset);
	if (fontsource) free(fontsource);

	/* Configure the style */
	evas_textblock_style_set(stl->style, eina_strbuf_string_get(txt));
	eina_strbuf_reset(txt);
     }
   if (txt)
     eina_strbuf_free(txt);
}

void
_edje_textblock_style_cleanup(Edje_File *edf)
{
   Edje_Style *stl;

   EINA_LIST_FREE(edf->styles, stl)
     {
        Edje_Style_Tag *tag;

        EINA_LIST_FREE(stl->tags, tag)
	  {
	     if (tag->value && eet_dictionary_string_check(eet_dictionary_get(edf->ef), tag->value) == 0)
               eina_stringshare_del(tag->value);
             if (edf->free_strings)
               {
                  if (tag->key) eina_stringshare_del(tag->key);
/*                FIXME: Find a proper way to handle it. */
                  if (tag->text_class) eina_stringshare_del(tag->text_class);
                  if (tag->font) eina_stringshare_del(tag->font);
#ifdef EFL_FEATURE_CHANGEABLE_UI
                  if (tag->color_class) eina_stringshare_del(tag->color_class);
                  if (tag->color) eina_stringshare_del(tag->color);
                  if (tag->outline_color) eina_stringshare_del(tag->outline_color);
                  if (tag->shadow_color) eina_stringshare_del(tag->shadow_color);
#endif
               }
             free(tag);
	  }
        if (edf->free_strings && stl->name) eina_stringshare_del(stl->name);
	if (stl->style) evas_textblock_style_free(stl->style);
	free(stl);
     }
}
