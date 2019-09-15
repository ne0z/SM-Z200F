/**
 * Simple Evas example illustrating a custom Evas box object
 *
 * You'll need at least one engine built for it (excluding the buffer
 * one). See stdout/stderr for output.
 *
 * @verbatim
 * gcc -o evas-box evas-box.c `pkg-config --libs --cflags evas ecore ecore-evas eina ector eo efl`
 * @endverbatim
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define PACKAGE_EXAMPLES_DIR "."
#endif

#define WIDTH 400
#define HEIGHT 400

#ifndef EFL_BETA_API_SUPPORT
#define EFL_BETA_API_SUPPORT 1
#endif

#ifndef EFL_EO_API_SUPPORT
#define EFL_EO_API_SUPPORT 1
#endif

#include <Eo.h>
#include <Efl.h>
#include <Evas.h>
#include <Ecore.h>
#include <Ecore_Evas.h>

void _rect_add(Efl_Gfx_Path_Command **path_cmd, double **points,int x, int y, int w, int h)
{
   efl_gfx_path_append_move_to(path_cmd, points, x, y);
   efl_gfx_path_append_line_to(path_cmd, points, x + w, y);
   efl_gfx_path_append_line_to(path_cmd, points, x + w, y +h);
   efl_gfx_path_append_line_to(path_cmd, points, x, y +h);
   efl_gfx_path_append_close(path_cmd, points);
}


struct example_data
{
   Ecore_Evas  *ee;
   Evas        *evas;
   Evas_Object *bg;
   Evas_Object *vg;
};

static struct example_data d;

static void
_on_delete(Ecore_Evas *ee EINA_UNUSED)
{
   ecore_main_loop_quit();
}

static void /* adjust canvas' contents on resizes */
_canvas_resize_cb(Ecore_Evas *ee)
{
   int w, h;

   ecore_evas_geometry_get(ee, NULL, NULL, &w, &h);
   evas_object_resize(d.bg, w, h);
   evas_object_resize(d.vg, w, h);
}

static void
vector_set(int x, int y, int w, int h)
{
   Efl_Gfx_Path_Command *path_cmd = NULL;
   double *points = NULL;
   int vg_w = w, vg_h = h;

   //Create VG Object

   Evas_Object *tmp = evas_object_rectangle_add(d.evas);
   evas_object_resize(tmp, vg_w, vg_h);
   evas_object_color_set(tmp, 100, 80,50, 100);
   evas_object_move(tmp, x,y);
   evas_object_show(tmp);

   d.vg = evas_object_vg_add(d.evas);
   evas_object_resize(d.vg, vg_w, vg_h);
   evas_object_move(d.vg, x,y);
   evas_object_show(d.vg);
   evas_object_clip_set(d.vg, tmp);

     // Applying map on the evas_object_vg
//   Evas_Map *m = evas_map_new(4);
//   evas_map_smooth_set(m, EINA_TRUE);
//   evas_map_util_points_populate_from_object_full(m, d.vg, 0);
//   evas_map_util_rotate(m, 10, 0,0);
//   evas_object_map_enable_set(d.vg, EINA_TRUE);
//   evas_object_map_set(d.vg, m);

   // apply some transformation
   double radian = 30.0 * 2 * 3.141 / 360.0;
   Eina_Matrix3 matrix;
   eina_matrix3_rotate(&matrix, radian);

   Evas_VG_Node *root = evas_object_vg_root_node_get(d.vg);
   //eo_do(root, evas_vg_node_transformation_set(&matrix));

   Evas_VG_Node *bg = eo_add(EVAS_VG_SHAPE_CLASS, root);
   _rect_add(&path_cmd, &points, 0, 0 , vg_w, vg_h);
   eo_do(bg,
         evas_vg_node_origin_set(0, 0),
         efl_gfx_shape_stroke_width_set(1.0),
         efl_gfx_color_set(128, 128, 128, 80),
         efl_gfx_shape_path_set(path_cmd, points));  


   free(path_cmd);
   free(points);
   path_cmd = NULL;
   points = NULL;

   Evas_VG_Node *shape = eo_add(EVAS_VG_SHAPE_CLASS, root);
   Evas_VG_Node *rgradient = eo_add(EVAS_VG_GRADIENT_RADIAL_CLASS, root);
   Evas_VG_Node *lgradient = eo_add(EVAS_VG_GRADIENT_LINEAR_CLASS, root);
    efl_gfx_path_append_move_to(&path_cmd, &points, 50, 50);
    efl_gfx_path_append_arc(&path_cmd, &points, 0, 0, 100, 100, 120, 300);
    efl_gfx_path_append_close(&path_cmd, &points);

    Efl_Gfx_Gradient_Stop stops[3];
    stops[0].r = 255;
    stops[0].g = 0;
    stops[0].b = 0;
    stops[0].a = 255;
    stops[0].offset = 0;
    stops[1].r = 0;
    stops[1].g = 255;
    stops[1].b = 0;
    stops[1].a = 255;
    stops[1].offset = 0.5;
    stops[2].r = 0;
    stops[2].g = 0;
    stops[2].b = 255;
    stops[2].a = 255;
    stops[2].offset = 1;

  eo_do(rgradient,
        evas_vg_node_origin_set(10,10),
        efl_gfx_gradient_stop_set(stops, 3),
        efl_gfx_gradient_spread_set(EFL_GFX_GRADIENT_SPREAD_REFLECT),
        efl_gfx_gradient_stop_set(stops, 3),
        efl_gfx_gradient_radial_center_set(30, 30),
        efl_gfx_gradient_radial_radius_set(80)
        );

    eo_do(lgradient,
        evas_vg_node_origin_set(10,10),
        efl_gfx_gradient_stop_set(stops, 3),
        efl_gfx_gradient_spread_set(EFL_GFX_GRADIENT_SPREAD_REFLECT),
        efl_gfx_gradient_stop_set(stops, 3),
        efl_gfx_gradient_linear_start_set(10,10),
        efl_gfx_gradient_linear_end_set(50,50)
        );

   eo_do(shape,
         evas_vg_node_origin_set(10, 10),
         //evas_vg_shape_fill_set(rgradient),
         efl_gfx_shape_stroke_scale_set(2.0),
         efl_gfx_shape_stroke_width_set(1.0),
         //efl_gfx_color_set(0, 0, 255, 255),
         efl_gfx_shape_stroke_color_set(0, 0, 255, 128),
         efl_gfx_shape_path_set(path_cmd, points));


   free(path_cmd);
   free(points);
   path_cmd = NULL;
   points = NULL;

   Evas_VG_Node *rect = eo_add(EVAS_VG_SHAPE_CLASS, root);
   _rect_add(&path_cmd, &points, 0, 0 , 100, 100);
   eo_do(rect,
         evas_vg_node_origin_set(100, 100),
         evas_vg_shape_fill_set(lgradient),
         efl_gfx_shape_stroke_width_set(2.0),
         efl_gfx_shape_stroke_join_set(EFL_GFX_JOIN_ROUND),
         efl_gfx_shape_stroke_color_set(255, 255, 255, 255),
         efl_gfx_shape_path_set(path_cmd, points));

   free(path_cmd);
   free(points);
   path_cmd = NULL;
   points = NULL;


   Evas_VG_Node *rect1 = eo_add(EVAS_VG_SHAPE_CLASS, root);
   _rect_add(&path_cmd, &points, 0, 0 , 70, 70);
   eo_do(rect1,
         evas_vg_node_origin_set(50, 70),
         efl_gfx_shape_stroke_scale_set(2),
         efl_gfx_shape_stroke_width_set(8.0),
         efl_gfx_shape_stroke_join_set(EFL_GFX_JOIN_ROUND),
         efl_gfx_shape_stroke_color_set(0, 100, 80, 100),
         efl_gfx_shape_path_set(path_cmd, points));

   free(path_cmd);
   free(points);
   path_cmd = NULL;
   points = NULL;




   Evas_VG_Node *circle = eo_add(EVAS_VG_SHAPE_CLASS, root);
   efl_gfx_path_append_move_to(&path_cmd, &points, 125, 50);
   efl_gfx_path_append_arc(&path_cmd, &points, 0, 0, 250, 100, 30, 300);
   eo_do(circle,
         evas_vg_shape_fill_set(lgradient),
         //evas_vg_node_transformation_set(&matrix),
         evas_vg_node_origin_set(50,50),
         efl_gfx_color_set(255, 0, 0, 50),
         efl_gfx_shape_path_set(path_cmd, points));

   free(path_cmd);
   free(points);
   path_cmd = NULL;
   points = NULL;

   // Foreground
   Evas_VG_Node *fg = eo_add(EVAS_VG_SHAPE_CLASS, root);
   _rect_add(&path_cmd, &points, 0, 0 , vg_w, vg_h);
   eo_do(fg,
         evas_vg_node_origin_set(0, 0),
         efl_gfx_shape_stroke_width_set(5.0),
         efl_gfx_shape_stroke_join_set(EFL_GFX_JOIN_ROUND),
         efl_gfx_shape_stroke_color_set(255, 255, 0, 70),
         efl_gfx_shape_path_set(path_cmd, points));

   free(path_cmd);
   free(points);
   path_cmd = NULL;
   points = NULL;

}

int
main(void)
{
   if (!ecore_evas_init())
     return EXIT_FAILURE;

   /* this will give you a window with an Evas canvas under the first
    * engine available */
   d.ee = ecore_evas_new(NULL, 0, 0, WIDTH, HEIGHT, NULL);
   if (!d.ee)
     goto error;

   ecore_evas_callback_delete_request_set(d.ee, _on_delete);
   ecore_evas_callback_resize_set(d.ee, _canvas_resize_cb);
   ecore_evas_show(d.ee);

   d.evas = ecore_evas_get(d.ee);

   d.bg = evas_object_rectangle_add(d.evas);
   evas_object_color_set(d.bg, 70, 70, 70, 255); /* white bg */
   evas_object_show(d.bg);

   _canvas_resize_cb(d.ee);

   vector_set(50, 50, 300 ,300);
   //vector_set(30, 90, 300 ,300);

   ecore_main_loop_begin();
   ecore_evas_shutdown();
   return 0;

error:
   ecore_evas_shutdown();
   return -1;
}

