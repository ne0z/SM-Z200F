#ifndef EFL_GRAPHICS_UTILS_H_
# define EFL_GRAPHICS_UTILS_H_

/**
 * Copy Path Command List and Point's List
 *
 * @param out_cmd copied command list to be returned
 * @param out_pts copied point's list to be returned
 * @param in_cmd source command list
 * @param in_pts source point's list
 * @return @c EINA_TRUE on success or @c EINA_FALSE on error.
 *
 * @warning @p out_cmd and @p out_pts should be passed with NULL pointers.
 *
 * @since 1.14
 */
EAPI Eina_Bool
efl_gfx_path_dup(Efl_Gfx_Path_Command **out_cmd, double **out_pts,
                 const Efl_Gfx_Path_Command *in_cmd, const double *in_pts);

/**
 * Moves the current point to the given point, 
 * implicitly starting a new subpath and closing the previous one.
 *
 * @param commands command list.
 * @param points point's list.
 * @param x X co-ordinate of the current point.
 * @param y Y co-ordinate of the current point.
 *
 * @see efl_gfx_path_append_close()
 * @since 1.14
 */
EAPI void
efl_gfx_path_append_move_to(Efl_Gfx_Path_Command **commands, double **points,
                            double x, double y);

/**
 * Adds a straight line from the current position to the given endPoint.
 * After the line is drawn, the current position is updated to be at the end
 * point of the line.
 *
 * @note if no current position present, it draws a line to itself, basically
 *       a point.
 *
 * @param commands command list.
 * @param points point's list.
 * @param x X co-ordinate of end point of the line.
 * @param y Y co-ordinate of end point of the line.
 *
 * @see efl_gfx_path_append_move_to()
 * @since 1.14
 */
EAPI void
efl_gfx_path_append_line_to(Efl_Gfx_Path_Command **commands, double **points,
                            double x, double y);

/**
 * Adds a quadratic Bezier curve between the current position and the
 * given end point (x,y) using the control points specified by (ctrl_x, ctrl_y).
 * After the path is drawn, the current position is updated to be at the end
 * point of the path.
 *
 * @param commands command list.
 * @param points point's list.
 * @param x X co-ordinate of end point of the line.
 * @param y Y co-ordinate of end point of the line.
 * @param ctrl_x0 X co-ordinate of control point.
 * @param ctrl_y0 Y co-ordinate of control point.
 * @since 1.14
 *
 */
EAPI void
efl_gfx_path_append_quadratic_to(Efl_Gfx_Path_Command **commands,
                                 double **points, double x, double y,
                                 double ctrl_x, double ctrl_y);

/**
 * Same as efl_gfx_path_append_quadratic_to() api only difference is that it
 * uses the current control point to draw the bezier.
 *
 * @see efl_gfx_path_append_quadratic_to()
 * @since 1.14
 */
EAPI void
efl_gfx_path_append_squadratic_to(Efl_Gfx_Path_Command **commands,
                                  double **points, double x, double y);

/**
 * Adds a cubic Bezier curve between the current position and the
 * given end point (x,y) using the control points specified by
 * (ctrl_x0, ctrl_y0), and (ctrl_x1, ctrl_y1). After the path is drawn,
 * the current position is updated to be at the end point of the path.
 *
 * @param commands command list.
 * @param points point's list.
 * @param x X co-ordinate of end point of the line.
 * @param y Y co-ordinate of end point of the line.
 * @param ctrl_x0 X co-ordinate of 1st control point.
 * @param ctrl_y0 Y co-ordinate of 1st control point.
 * @param ctrl_x1 X co-ordinate of 2nd control point.
 * @param ctrl_y1 Y co-ordinate of 2nd control point.
 *
 * @since 1.14
 */
EAPI void
efl_gfx_path_append_cubic_to(Efl_Gfx_Path_Command **commands, double **points,
                             double x, double y,
                             double ctrl_x0, double ctrl_y0,
                             double ctrl_x1, double ctrl_y1);

/**
 * Same as efl_gfx_path_append_cubic_to() api only difference is that it uses
 * the current control point to draw the bezier.
 *
 * @see efl_gfx_path_append_cubic_to()
 *
 * @since 1.14
 */
EAPI void
efl_gfx_path_append_scubic_to(Efl_Gfx_Path_Command **commands, double **points,
                              double x, double y, double ctrl_x, double ctrl_y);

/**
 * Append an arc that connects from the current point int the point list
 * to the given point (x,y). The arc is defined by the given radius in 
 * x-direction (rx) and radius in y direction (ry) . 
 *
 * @note Use this api if you know the end point's of the arc otherwise
 *       use more convenient function efl_gfx_path_append_arc_to()
 *
 * @param commands command list.
 * @param points point's list.
 * @param x X co-ordinate of end point of the arc.
 * @param y Y co-ordinate of end point of the arc.
 * @param rx radius of arc in x direction.
 * @param ry radius of arc in y direction.
 * @param angle x-axis rotation , normally 0.
 * @param ry radius of arc in y direction.
 * @param large_arc Defines whether to draw the larger arc or smaller arc
 *        joining two point.
 * @param sweep Defines whether the arc will be drawn counter-clockwise or
 *        clockwise from current point to the end point taking into account the
 *        large_arc property.
 *
 * @see efl_gfx_path_append_arc_to()
 * @since 1.14
 */
EAPI void
efl_gfx_path_append_arc_to(Efl_Gfx_Path_Command **commands, double **points,
                           double x, double y,
                           double rx, double ry,
                           double angle,
                           Eina_Bool large_arc, Eina_Bool sweep);

/**
 * Append an arc that occupies the given rectangle, beginning at the specified
 * start_angle znd extending sweep_length degrees counter-clockwise.
 *
 * @note Angles are specified in degrees. Clockwise arcs can be specified
 *       using negative angles.
 *
 *       This function connects the starting point of the arc to the current
 *       position if they are not already connected.After the arc has been
 *       added,the current position is the last point in arc. To draw a line
 *       back to the first point, use the efl_gfx_path_append_close() function.
 *
 * @param commands command list.
 * @param points point's list.
 * @param x X co-ordinate of the rectangle.
 * @param y Y co-ordinate of the rectangle.
 * @param w Width of the rectangle.
 * @param h Height of the rectangle.
 * @param start_angle start point of the arc will be from this angle
 *        (in degrees 0 - 360).
 * @param sweep_length length of the arc from start point (in degree 0 - 360),
 *        minus value for clockwise direction.
 *
 * @see efl_gfx_path_append_close()
 * @see efl_gfx_path_append_arc_to()
 * @since 1.14
 */
EAPI void
efl_gfx_path_append_arc(Efl_Gfx_Path_Command **commands, double **points,
                        double x, double y, double w, double h,
                        double start_angle,double sweep_length);

/**
 * Append the given rectangle with rounded corner to the path.
 *
 * The xr and yr arguments specify the radii of the ellipses defining the
 * corners of the rounded rectangle.
 *
 * @note xr and yr are specified in percentage of half the rectangle's width
 *       and height respectively, and should be in the range 0.0 to 100.0.
 *
 * @note if xr and yr are 0, then it will draw a rectangle without rounded
 *       corner.
 *
 * @param commands command list.
 * @param points point's list.
 * @param x X co-ordinate of the rectangle.
 * @param y Y co-ordinate of the rectangle.
 * @param w Width of the rectangle.
 * @param h Height of the rectangle.
 * @param xr percentage of the half of the width of the rectangle (0 - 100).
 * @param yr percentage of the half of the height of the rectangle (0 - 100).
 * @since 1.14
 *
 */
EAPI void
efl_gfx_path_append_rounded_rect(Efl_Gfx_Path_Command **commands,
                                 double **points, double x, double y, double w,
                                 double h,
                                 double xr,double yr);

/**
 * Closes the current subpath by drawing a line to the beginning of the subpath,
 * automatically starting a new path. The current point of the new path is
 * (0, 0).
 *
 * @note If the subpath does not contain any points, this function does nothing.
 *
 * @param commands command list.
 * @param points point's list.
 *
 * @since 1.14
 */
EAPI void
efl_gfx_path_append_close(Efl_Gfx_Path_Command **commands, double **points);

/**
 * Append a circle with given center and radius.
 *
 * @param commands command list.
 * @param points point's list.
 * @param cx X co-ordinate of the center of the circle.
 * @param y Y co-ordinate of the center of the circle.
 * @param radius radius of the circle.
 *
 * @see efl_gfx_path_append_arc()
 * @since 1.14
 */
EAPI void
efl_gfx_path_append_circle(Efl_Gfx_Path_Command **commands, double **points,
                           double cx, double cy, double radius);

EAPI Eina_Bool
efl_gfx_path_append_svg_path(Efl_Gfx_Path_Command **commands, double **points, const char *svg_path_data);

EAPI void
efl_gfx_path_interpolate(const Efl_Gfx_Path_Command *cmd,
                         double pos_map,
                         const double *from, const double *to, double *r);

/**
 * Compare two path commands whether both command lists are same or not.
 *
 * @param a command list
 * @param b command list
 * @return @c EINA_TRUE if @p a and @p b command list are same, @c EINA_FALSE,
 *         otherwise.
 *
 * @since 1.14
 */
EAPI Eina_Bool
efl_gfx_path_equal_commands(const Efl_Gfx_Path_Command *a,
                            const Efl_Gfx_Path_Command *b);

/**
 * Return the current(last) path and control position.
 *
 * @param cmd command list
 * @param points point's list
 * @param current_x current x position
 * @param current_y current y position
 * @param current_ctrl_x current x control point (just in case CUBIC type)
 * @param current_ctrl_y current y control point (just in case CUBIC type)
 * @return @c EINA_TRUE on success or @c EINA_FALSE if there are no positions to
 *         be returned.
 *
 * @since 1.14
 */
EAPI Eina_Bool
efl_gfx_path_current_get(const Efl_Gfx_Path_Command *cmd,
                         const double *points,
                         double *current_x, double *current_y,
                         double *current_ctrl_x, double *current_ctrl_y);

#endif
