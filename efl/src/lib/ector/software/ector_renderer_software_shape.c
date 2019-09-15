#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>
#include <float.h>

#include <Eina.h>
#include <Ector.h>
#include <software/Ector_Software.h>

#include "ector_private.h"
#include "ector_software_private.h"


typedef struct _Ector_Renderer_Software_Shape_Data Ector_Renderer_Software_Shape_Data;
struct _Ector_Renderer_Software_Shape_Data
{
   Ector_Software_Surface_Data         *surface;
   Ector_Renderer_Generic_Shape_Data   *shape;
   Ector_Renderer_Generic_Base_Data    *base;
   Shape_Rle_Data                      *shape_data;
   Shape_Rle_Data                      *outline_data;
};

typedef struct _Outline
{
   SW_FT_Outline ft_outline;
   int points_alloc;
   int contours_alloc;
}Outline;


#define TO_FT_COORD(x) ((x) * 64); // to freetype 26.6 coordinate.

static inline void
_grow_outline_contour(Outline *outline, int num)
{
   if ( outline->ft_outline.n_contours + num > outline->contours_alloc)
     {
        outline->contours_alloc += 5;
        outline->ft_outline.contours = (short *) realloc(outline->ft_outline.contours, outline->contours_alloc * sizeof(short));
     }
}

static inline void
_grow_outline_points(Outline *outline, int num)
{
   if ( outline->ft_outline.n_points + num > outline->points_alloc)
     {
        outline->points_alloc += 50;
        outline->ft_outline.points = (SW_FT_Vector *) realloc(outline->ft_outline.points, outline->points_alloc * sizeof(SW_FT_Vector));
        outline->ft_outline.tags = (char *) realloc(outline->ft_outline.tags, outline->points_alloc * sizeof(char));
     }
}
static Outline *
_outline_create()
{
   Outline *outline = (Outline *) calloc(1, sizeof(Outline));
   outline->points_alloc = 0;
   outline->contours_alloc = 0;
   _grow_outline_contour(outline, 1);
   _grow_outline_points(outline, 1);
   return outline;
}

static
void _outline_destroy(Outline *outline)
{
   if (outline)
     {
        free(outline->ft_outline.points);
        free(outline->ft_outline.tags);
        free(outline->ft_outline.contours);
        free(outline);
        outline = NULL;
     }
}

static void
_outline_move_to(Outline *outline, double x, double y)
{
   SW_FT_Outline *ft_outline = &outline->ft_outline;

   _grow_outline_points(outline, 1);
   ft_outline->points[ft_outline->n_points].x = TO_FT_COORD(x);
   ft_outline->points[ft_outline->n_points].y = TO_FT_COORD(y);
   ft_outline->tags[ft_outline->n_points] = SW_FT_CURVE_TAG_ON;

   if (ft_outline->n_points)
     {
        _grow_outline_contour(outline, 1);
        ft_outline->contours[ft_outline->n_contours] = ft_outline->n_points - 1;
        ft_outline->n_contours++;
     }

   ft_outline->n_points++;
}

static void
_outline_end(Outline *outline)
{
   SW_FT_Outline *ft_outline = &outline->ft_outline;

   _grow_outline_contour(outline, 1);

   if (ft_outline->n_points)
     {
        ft_outline->contours[ft_outline->n_contours] = ft_outline->n_points - 1;
        ft_outline->n_contours++;
     }
}


static void  _outline_line_to(Outline *outline, double x, double y)
{
   SW_FT_Outline *ft_outline = &outline->ft_outline;

   _grow_outline_points(outline, 1);
   ft_outline->points[ft_outline->n_points].x = TO_FT_COORD(x);
   ft_outline->points[ft_outline->n_points].y = TO_FT_COORD(y);
   ft_outline->tags[ft_outline->n_points] = SW_FT_CURVE_TAG_ON;
   ft_outline->n_points++;
}


static Eina_Bool
_outline_close_path(Outline *outline)
{
   SW_FT_Outline *ft_outline = &outline->ft_outline;
   int index ;

   if (ft_outline->n_contours)
     {
        index = ft_outline->contours[ft_outline->n_contours - 1] + 1;
     }
   else
     {
        // first path
        index = 0;
     }

   // make sure there is atleast one point in the current path
   if (ft_outline->n_points == index) return EINA_FALSE;

   // close the path
   _grow_outline_points(outline, 1);
   ft_outline->points[ft_outline->n_points].x = ft_outline->points[index].x;
   ft_outline->points[ft_outline->n_points].y = ft_outline->points[index].y;
   ft_outline->tags[ft_outline->n_points] = SW_FT_CURVE_TAG_ON;
   ft_outline->n_points++;

   return EINA_TRUE;
}


static void  _outline_cubic_to(Outline *outline, double cx1, double cy1, double cx2, double cy2, double x, double y)
{
   SW_FT_Outline *ft_outline = &outline->ft_outline;

   _grow_outline_points(outline, 3);

   ft_outline->points[ft_outline->n_points].x = TO_FT_COORD(cx1);
   ft_outline->points[ft_outline->n_points].y = TO_FT_COORD(cy1);
   ft_outline->tags[ft_outline->n_points] = SW_FT_CURVE_TAG_CUBIC;
   ft_outline->n_points++;

   ft_outline->points[ft_outline->n_points].x = TO_FT_COORD(cx2);
   ft_outline->points[ft_outline->n_points].y = TO_FT_COORD(cy2);
   ft_outline->tags[ft_outline->n_points] = SW_FT_CURVE_TAG_CUBIC;
   ft_outline->n_points++;

   ft_outline->points[ft_outline->n_points].x = TO_FT_COORD(x);
   ft_outline->points[ft_outline->n_points].y = TO_FT_COORD(y);
   ft_outline->tags[ft_outline->n_points] = SW_FT_CURVE_TAG_ON;
   ft_outline->n_points++;
}

static void _outline_transform(Outline *outline, Eina_Matrix3 *m)
{
   int i;
   double x, y;
   SW_FT_Outline *ft_outline = &outline->ft_outline;

   if (m)
     {
        for (i = 0; i < ft_outline->n_points; i++)
          {
             eina_matrix3_point_transform(m,
                                          ft_outline->points[i].x/64,/* convert back to normal coord.*/
                                          ft_outline->points[i].y/64,/* convert back to normal coord.*/
                                          &x, &y);
             ft_outline->points[i].x = TO_FT_COORD(x);
             ft_outline->points[i].y = TO_FT_COORD(y);
          }
     }
}

static Eina_Bool
_parse_commands(const Efl_Gfx_Path_Command *cmds, const double *pts, Outline * outline)
{
   Eina_Bool close_path = EINA_FALSE; 
   for (; *cmds != EFL_GFX_PATH_COMMAND_TYPE_END; cmds++)
     {
        switch (*cmds)
          {
            case EFL_GFX_PATH_COMMAND_TYPE_MOVE_TO:

               _outline_move_to(outline, pts[0], pts[1]);

               pts += 2;
               break;
            case EFL_GFX_PATH_COMMAND_TYPE_LINE_TO:

               _outline_line_to(outline, pts[0], pts[1]);

               pts += 2;
               break;
            case EFL_GFX_PATH_COMMAND_TYPE_CUBIC_TO:

               // Be careful, we do have a different order than
               // freetype first is destination point, followed by
               // the control point. The opposite of cairo.
               _outline_cubic_to(outline,
                                 pts[2], pts[3], pts[4], pts[5], // control points
                                 pts[0], pts[1]); // destination point
               pts += 6;
               break;

            case EFL_GFX_PATH_COMMAND_TYPE_CLOSE:

               close_path = _outline_close_path(outline);
               break;

            case EFL_GFX_PATH_COMMAND_TYPE_LAST:
            case EFL_GFX_PATH_COMMAND_TYPE_END:
               break;
          }
     }
   _outline_end(outline);
   return close_path;
}

static Eina_Bool
_generate_stroke_data(Ector_Renderer_Software_Shape_Data *pd)
{
   if (pd->outline_data) return EINA_FALSE;

   if (!pd->shape->stroke.fill &&
       ((pd->shape->stroke.color.a == 0) || pd->shape->stroke.width < 0.01))
     return EINA_FALSE;

   return EINA_TRUE;
}

static Eina_Bool
_generate_shape_data(Ector_Renderer_Software_Shape_Data *pd)
{
   if (pd->shape_data) return EINA_FALSE;

   if (!pd->shape->fill && (pd->base->color.a == 0)) return EINA_FALSE;

   return EINA_TRUE;
}

static void 
_update_rle(Eo *obj, Ector_Renderer_Software_Shape_Data *pd)
{
   const Efl_Gfx_Path_Command *cmds = NULL;
   const double *pts = NULL;
   eo_do(obj, efl_gfx_shape_path_get(&cmds, &pts));
   if (cmds && (_generate_stroke_data(pd) || _generate_shape_data(pd)))
     {
        Eina_Bool close_path;
        Outline * outline = _outline_create();
        close_path = _parse_commands(cmds, pts, outline);
        _outline_transform(outline, pd->base->m);

        //shape data generation 
        if (_generate_shape_data(pd))
         pd->shape_data = ector_software_rasterizer_generate_rle_data(pd->surface->software, &outline->ft_outline);

        //stroke data generation
        if (_generate_stroke_data(pd))
          {
             ector_software_rasterizer_stroke_set(pd->surface->software, (pd->shape->stroke.width * pd->shape->stroke.scale), pd->shape->stroke.cap,
                                                  pd->shape->stroke.join);
             pd->outline_data = ector_software_rasterizer_generate_stroke_rle_data(pd->surface->software, &outline->ft_outline, close_path);
          }
        _outline_destroy(outline);
     }
}

static Eina_Bool
_ector_renderer_software_shape_ector_renderer_generic_base_prepare(Eo *obj, Ector_Renderer_Software_Shape_Data *pd)
{
   // FIXME: shouldn't that be part of the shape generic implementation ?
   if (pd->shape->fill)
     eo_do(pd->shape->fill, ector_renderer_prepare());
   if (pd->shape->stroke.fill)
     eo_do(pd->shape->stroke.fill, ector_renderer_prepare());
   if (pd->shape->stroke.marker)
     eo_do(pd->shape->stroke.marker, ector_renderer_prepare());

   // shouldn't that be moved to the software base object
   if (!pd->surface)
     {
        Eo *parent;
        eo_do(obj, parent = eo_parent_get());
        if (!parent) return EINA_FALSE;
        pd->surface = eo_data_xref(parent, ECTOR_SOFTWARE_SURFACE_CLASS, obj);
        if (!pd->surface) return EINA_FALSE;
     }
   return EINA_TRUE;
}

static Eina_Bool
_ector_renderer_software_shape_ector_renderer_generic_base_draw(Eo *obj EINA_UNUSED, Ector_Renderer_Software_Shape_Data *pd, Ector_Rop op, Eina_Array *clips, unsigned int mul_col)
{
   int x, y;

   // do lazy creation of rle
   _update_rle(obj, pd);

   // adjust the offset
   x = pd->surface->x + (int)pd->base->origin.x;
   y = pd->surface->y + (int)pd->base->origin.y;

   // fill the span_data structure
   ector_software_rasterizer_clip_rect_set(pd->surface->software, clips);
   ector_software_rasterizer_transform_set(pd->surface->software, pd->base->m);

   if (pd->shape->fill)
     {
        eo_do(pd->shape->fill, ector_renderer_software_base_fill());
        ector_software_rasterizer_draw_rle_data(pd->surface->software, x, y, mul_col, op, pd->shape_data);
     }
   else
     {
        if (pd->base->color.a > 0)
          {
             ector_software_rasterizer_color_set(pd->surface->software, pd->base->color.r, pd->base->color.g, pd->base->color.b, pd->base->color.a);
             ector_software_rasterizer_draw_rle_data(pd->surface->software, x, y, mul_col, op, pd->shape_data);
          }
     }

   if (pd->shape->stroke.fill)
     {
        eo_do(pd->shape->stroke.fill, ector_renderer_software_base_fill());
        ector_software_rasterizer_draw_rle_data(pd->surface->software, x, y, mul_col, op, pd->outline_data);
     }
   else
     {
        if (pd->shape->stroke.color.a > 0)
          {
             ector_software_rasterizer_color_set(pd->surface->software,
                                                 pd->shape->stroke.color.r, pd->shape->stroke.color.g,
                                                 pd->shape->stroke.color.b, pd->shape->stroke.color.a);
             ector_software_rasterizer_draw_rle_data(pd->surface->software, x, y, mul_col, op, pd->outline_data);
          }
     }

   return EINA_TRUE;
}

static Eina_Bool
_ector_renderer_software_shape_ector_renderer_software_base_fill(Eo *obj EINA_UNUSED, Ector_Renderer_Software_Shape_Data *pd EINA_UNUSED)
{
   // FIXME: let's find out how to fill a shape with a shape later.
   // I need to read SVG specification and see how to map that with software.
   return EINA_FALSE;
}

static void
_ector_renderer_software_shape_efl_gfx_shape_path_set(Eo *obj, Ector_Renderer_Software_Shape_Data *pd,
                                                      const Efl_Gfx_Path_Command *op, const double *points)
{
   if (pd->shape_data) ector_software_rasterizer_destroy_rle_data(pd->shape_data);
   if (pd->outline_data) ector_software_rasterizer_destroy_rle_data(pd->outline_data);
   pd->shape_data = NULL;
   pd->outline_data = NULL;

   eo_do_super(obj, ECTOR_RENDERER_SOFTWARE_SHAPE_CLASS, efl_gfx_shape_path_set(op, points));
}


static Eina_Bool
_ector_renderer_software_shape_path_changed(void *data, Eo *obj EINA_UNUSED, const Eo_Event_Description *desc EINA_UNUSED,
                                            void *event_info EINA_UNUSED)
{
   Ector_Renderer_Software_Shape_Data *pd = data;
   
   if (pd->shape_data) ector_software_rasterizer_destroy_rle_data(pd->shape_data);
   if (pd->outline_data) ector_software_rasterizer_destroy_rle_data(pd->outline_data);
   
   pd->shape_data = NULL;
   pd->outline_data = NULL;

   return EINA_TRUE;
}

Eo *
_ector_renderer_software_shape_eo_base_constructor(Eo *obj, Ector_Renderer_Software_Shape_Data *pd)
{
   obj = eo_do_super_ret(obj, ECTOR_RENDERER_SOFTWARE_SHAPE_CLASS, obj, eo_constructor());
   pd->shape = eo_data_xref(obj, ECTOR_RENDERER_GENERIC_SHAPE_MIXIN, obj);
   pd->base = eo_data_xref(obj, ECTOR_RENDERER_GENERIC_BASE_CLASS, obj);
   eo_do(obj,
         eo_event_callback_add(EFL_GFX_PATH_CHANGED, _ector_renderer_software_shape_path_changed, pd));

   return obj;
}

void
_ector_renderer_software_shape_eo_base_destructor(Eo *obj, Ector_Renderer_Software_Shape_Data *pd)
{
   Eo *parent;
   //FIXME, As base class  destructor can't call destructor of mixin class.
   // call explicit API to free shape data.
   eo_do(obj, efl_gfx_shape_reset());

   if (pd->shape_data) ector_software_rasterizer_destroy_rle_data(pd->shape_data);
   if (pd->outline_data) ector_software_rasterizer_destroy_rle_data(pd->outline_data);

   eo_do(obj, parent = eo_parent_get());
   eo_data_xunref(parent, pd->surface, obj);

   eo_data_xunref(obj, pd->shape, obj);
   eo_data_xunref(obj, pd->base, obj);
   eo_do_super(obj, ECTOR_RENDERER_SOFTWARE_SHAPE_CLASS, eo_destructor());
}


#include "ector_renderer_software_shape.eo.c"
