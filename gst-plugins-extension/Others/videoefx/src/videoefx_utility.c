#include "videoefx_utility.h"

gboolean dump = TRUE;
static int _drm_init (AppData *app_data)
{
    startfunc
    if (!app_data || !app_data->display) {
        GST_ERROR("app_data is null");
        return -1;
    }

    int dri2_ev_base = 0;
    int dri2_err_base = 0;
    int dri2_major = 0;
    int dri2_minor = 0;
    char *drv_name = NULL;
    char *dev_name = NULL;
    drm_magic_t magic = 0;
    int fd = -1;

    if(!DRI2QueryExtension(app_data->display, &dri2_ev_base, &dri2_err_base)) {
        GST_ERROR("Failed to get dri2 extension");
        return -1;
    }
    if(!DRI2QueryVersion(app_data->display, &dri2_major, &dri2_minor )) {
        GST_ERROR("Failed to get dri2 version");
        return -1;
    }

    if(!DRI2Connect(app_data->display, app_data->window, &drv_name, &dev_name )) {
        GST_ERROR("Failed to get dri2 version");
        return -1;
    }

    GST_INFO("Driver name is %s and dev name is %s",drv_name, dev_name);

    GST_WARNING("open [%s]", dev_name);
    fd = open(dev_name, O_RDWR);
    IF_FREE(drv_name);
    IF_FREE(dev_name);

    if (fd < 0) {
        GST_ERROR("[DRM] couldn't open");
        return -1;
    }

    GST_WARNING("opened fd [%d]", fd);

    if(drmGetMagic(fd, &magic)) {
        GST_ERROR("[DRM] can't get magic");
        close(fd);
        return -1;
    }
    if (!DRI2Authenticate(app_data->display, app_data->window, (unsigned int)magic)) {
        GST_ERROR("authentication failed");
        close(fd);
        return -1;
    }
    endfunc
    return fd;
}



void create_window(AppData *app_data)
{
    startfunc
    if (!app_data) {
        GST_WARNING("app_data is null");
        return;
    }

    /*create Window*/
    app_data->display = XOpenDisplay ( 0 );
    app_data->window = XDefaultRootWindow(app_data->display);

    GST_WARNING("XDisplay: %d, XWindow: %d", (int)app_data->display, (int)app_data->window);
    endfunc
    return;
}

void destroy_window(AppData *app_data)
{
    if (!app_data) {
        GST_ERROR("app_data is null");
        return;
    }

    GST_WARNING("XDestroyWindow display %d, window %d",
            (int)app_data->display, (int)app_data->window);

    XDestroyWindow(app_data->display, app_data->window);
    XCloseDisplay(app_data->display);
    return;
}

void init_gl_manager(AppData *app_data)
{
    startfunc
    int i;
    if (!app_data) {
        GST_ERROR("AppData is null");
        return ;
    }

    if (app_data->ready_for_glm) {
        GST_WARNING("GLM has been initialized");
        return ;
    }

    /*create fd */
    app_data->fd = _drm_init (app_data);
    if (app_data->fd < 0) {
        GST_INFO("create fd failed, app_data->fd: %d", app_data->fd);
        return ;
    }
    app_data->bufmgr = tbm_bufmgr_init (app_data->fd);

    /*create output pixmap*/
    for (i = 0 ; i < SURFACE_COUNT ; i++) {
        app_data->surface_buffer[i] = tbm_surface_create (app_data->width, app_data->height,TBM_FORMAT_BGRA8888);
        GST_WARNING("pixmap[%d] %p", i, app_data->surface_buffer[i]);
    }
    for (i = 0 ; i < MAX_INPUT_BUFFER ; i++) {
        app_data->in_buf[i].bo = NULL;
        app_data->in_buf[i].surface = NULL;
    }

    app_data->ready_for_glm = TRUE;
    endfunc
    return;
}

void create_gl_manager(AppData *app_data)
{
    int result = 0;
    int i = 0;
    for(i=0; i < SURFACE_COUNT ; i++){
        result = image_filter_set_output_by_tbm_surface(app_data->image_filter_handle, app_data->surface_buffer[i]);
        if(result != IMAGE_FILTER_ERROR_NONE){
            GST_ERROR("Unable to set output surface %d",result);
            return;
        }
    }
    app_data->has_created_glm = TRUE;
    return;
}

void destroy_gl_manager(AppData *app_data){

    int i=0;

    app_data->width = 0;
    app_data->height = 0;

    for (i = 0 ; i < SURFACE_COUNT ; i++) {
        GST_WARNING("destroying surface[%d] %p", i, app_data->surface_buffer[i]);
        tbm_surface_destroy(app_data->surface_buffer[i]);
    }
    for (i = 0 ; i < MAX_INPUT_BUFFER ; i++) {
        if(app_data->in_buf[i].bo != NULL && app_data->in_buf[i].surface != NULL){
            GST_WARNING("destroying surface of input buffers[%d] %p", i, app_data->in_buf[i].surface);
            tbm_surface_destroy(app_data->in_buf[i].surface);
            app_data->in_buf[i].bo = NULL;
        }
    }
    GST_WARNING("tbm_bufmgr_deinit %p", app_data->bufmgr);
    if (app_data->bufmgr) {
        tbm_bufmgr_deinit (app_data->bufmgr);
        app_data->bufmgr = NULL;
    }

    close (app_data->fd);
    app_data->fd = -1;
    return;
}

AppData* init_app_data(){
    startfunc
    AppData *app_data = NULL;
    int i= 0;

    app_data = (AppData*)malloc(sizeof(AppData));
    if (!app_data) {
        GST_ERROR("Alloc AppData failed");
        return NULL;
    }

    app_data->has_created_glm = FALSE;

    app_data->surface = NULL;
    app_data->input_bo = NULL;
    app_data->pix_index = 0;
    app_data->output_surface = NULL;

    app_data->buf_share_method = -1;

    for (i = 0 ; i < SURFACE_COUNT ; i++) {
        app_data->surface_using[i] = FALSE;
    }

    create_window(app_data);
    endfunc
    return app_data;
}


void destroy_app_data(AppData *app_data){
    int i = 0;
    if (!app_data) {
        GST_ERROR("app_data is null");
        return ;
    }
    /*destroy window*/

    destroy_window(app_data);
    destroy_gl_manager(app_data);
    IF_FREE(app_data);
}


gint get_available_pixmap_id(AppData *app_data)
{
    gint id = -1;

    if (!app_data) {
        GST_ERROR("AppData is null");
        return id;
    }
    for (; app_data->pix_index < SURFACE_COUNT ;) {
        if (app_data->surface_using[/*i*/app_data->pix_index] == FALSE) {
            app_data->surface_using[app_data->pix_index] = TRUE;
            id = app_data->pix_index;
            app_data->pix_index = app_data->pix_index+1;
            app_data->pix_index = app_data->pix_index%SURFACE_COUNT;
            break;
        }
        GST_INFO("Looping for available pixmap");
        app_data->pix_index = (app_data->pix_index +1)% SURFACE_COUNT;
    }
    return id;
}

int tbm_export_buffer(AppData *app_data, tbm_bo bo, tbm_surface_info_s surface_info, tbm_surface_h *psurface)
{
    int i = 0;
    int ret = FALSE;

    if (!app_data) {
        GST_ERROR("app_data is NULL");
        return FALSE;
    }

    if (app_data->fd <= 0 || bo == NULL || !psurface) {
        GST_ERROR("invalid drm fd[%d] bo[%p] buffer[%p] error is %s", app_data->fd, bo, psurface, strerror(app_data->fd));
        return FALSE;
    }

    /* check converted bo */
    for (i = 0 ; i < MAX_INPUT_BUFFER ; i++) {
        if (bo == app_data->in_buf[i].bo) {
            GST_DEBUG("already converted bo[%p], return index[%d] surface",
                    bo, i);
            *psurface = app_data->in_buf[i].surface;
            return TRUE;
        }
    }

    *psurface = tbm_surface_internal_create_with_bos (&surface_info,&bo,1);
    if (*psurface == NULL) {
        GST_ERROR("failed to create native buffer. bo[%p]", bo);
        return FALSE;
    }

    /* keep bo */
    for (i = 0 ; i < MAX_INPUT_BUFFER ; i++) {
        if (app_data->in_buf[i].bo == NULL) {
            app_data->in_buf[i].bo = bo;
            app_data->in_buf[i].surface = *psurface;
            GST_INFO("keep[index:%d] bo[%p] surface [%p]", i, bo, *psurface);
            break;
        }
    }

    GST_DEBUG("done - bo %p, surface %p", bo, *psurface);
    return TRUE;
}

OutputData* process_input_buffer(AppData *app_data, GstBuffer *buffer){

    OutputData *output = NULL;
    if (!app_data->has_created_glm &&
            app_data->width > 0 && app_data->height > 0) {
        GST_WARNING("init_gl_manager call");
        init_gl_manager(app_data);
        GST_WARNING("create_gl_manager call");
        create_gl_manager(app_data);
    }

    int available_pixmap_id = -1;
    tbm_surface_h surface_output = NULL;
    MMVideoBuffer *imgb = NULL;
    tbm_bo input_bo = NULL;
    tbm_surface_h input_buffer = NULL;

    tbm_surface_info_s tsurf_info;
    unsigned int gem_handle = 0;
    int j = 0;
    int ret ;

    if(buffer == NULL){
        GST_WARNING("buffer is null");
        goto exit;
    }

    MMVideoBuffer *scmn_imgb = NULL;
    GstMapInfo mapInfo;
    GstMemory *img_buf_memory = NULL;
    if (buffer && gst_buffer_n_memory(buffer) > 1){
        img_buf_memory = gst_buffer_peek_memory(buffer,1);
        gst_memory_map(img_buf_memory,&mapInfo,GST_MAP_READ);
        imgb = (MMVideoBuffer *)mapInfo.data;
        gst_memory_unmap(img_buf_memory,&mapInfo);
    }

    if(imgb == NULL){
        GST_WARNING("imgb is null");
        goto exit;
    }

    tsurf_info.format = TBM_FORMAT_NV12;
    tsurf_info.width = imgb->width[0];
    tsurf_info.height = imgb->height[0];
    tsurf_info.bpp = tbm_surface_internal_get_bpp(tsurf_info.format);
    tsurf_info.num_planes = tbm_surface_internal_get_num_planes(tsurf_info.format);

    tsurf_info.planes[0].offset = 0;
    tsurf_info.planes[0].size = imgb->stride_width[0] * imgb->stride_height[0];
    tsurf_info.planes[0].stride = imgb->stride_width[0];
    tsurf_info.size = tsurf_info.planes[0].size;


    for (j = 1; j < tsurf_info.num_planes; j++)
    {
        tsurf_info.planes[j].size = imgb->stride_width[j] * imgb->stride_height[j];
        tsurf_info.planes[j].offset = tsurf_info.planes[j-1].size;
        tsurf_info.planes[j].stride = imgb->stride_width[j];
        tsurf_info.size = tsurf_info.size +  tsurf_info.planes[j].size;
        GST_WARNING("Going through plane %d",j);
    }

    if (!tbm_export_buffer(app_data, imgb->handle.bo[0], tsurf_info, &input_buffer)) {
        GST_WARNING("tbm_export_buffer failed");
        goto exit;
    }
    if (input_buffer == NULL) {
        GST_ERROR("Input Buffer is NULL");
        goto exit;
    }

    GST_INFO("Stride width is %d and height is %d width is %d and height is %d",imgb->stride_width[0],imgb->stride_height[0],imgb->width[0],imgb->height[0]);
    int result;

    available_pixmap_id = get_available_pixmap_id(app_data);

    GST_INFO("available_pixmap_id %d", available_pixmap_id);

    if(available_pixmap_id < 0)
    {
        GST_ERROR("Invalid pixmap id. Potential errors in initialization");
        return NULL;
    }
    surface_output = app_data->surface_buffer[available_pixmap_id];

    result =  image_filter_set_input_by_tbm_surface(app_data->image_filter_handle, input_buffer);

    if(result != IMAGE_FILTER_ERROR_NONE){
        GST_ERROR("Unable to set input surface %d",result);
        goto exit;
    }

    result = image_filter_set_output_by_tbm_surface(app_data->image_filter_handle, surface_output);
    if(result != IMAGE_FILTER_ERROR_NONE){
        GST_ERROR("Unable to set output surface %d",result);
        goto exit;
    }

    result = image_filter_apply(app_data->image_filter_handle);
    if(result != IMAGE_FILTER_ERROR_NONE){
        GST_ERROR("Unable to apply %d",result);
        goto exit;
    }

    /*get output data*/
    output = (OutputData *)malloc (sizeof (OutputData));
    if (!output) {
        GST_ERROR("Allocate OutputData failed");
        goto exit;
    }
    memset(output, 0x0, sizeof(OutputData));
    output->surface = surface_output;
    output->pixmap_id = available_pixmap_id;
    output->buf_share_method = app_data->buf_share_method;
    if (buffer) {
        output->timestamp = GST_BUFFER_TIMESTAMP(buffer);
        output->duration = GST_BUFFER_DURATION(buffer);
    }
    GST_INFO("handling buffer END ==================================");
exit:
    return output;

}
