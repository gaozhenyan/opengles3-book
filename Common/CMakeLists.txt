set ( common_src Source/esShader.c 
                 Source/esShapes.c
                 Source/esTransform.c
                 Source/esUtil.c )


# Win32 Platform files
if(WIN32)
    set( common_platform_src Source/Win32/esUtil_win32.c )
    add_library( Common STATIC ${common_src} ${common_platform_src} )
    target_link_libraries( Common ${OPENGLES3_LIBRARY} ${EGL_LIBRARY} )
else()
    set( common_platform_src
         Source/egl.c
         Source/egl.h
         Source/LinuxGbmDrm/common.h
         Source/LinuxGbmDrm/esUtil_gbm_drm.c
         Source/LinuxGbmDrm/ta/ta.c
         Source/LinuxGbmDrm/ta/ta.h
         Source/LinuxGbmDrm/ta/ta_talloc.c
         Source/LinuxGbmDrm/ta/ta_talloc.h
         Source/LinuxGbmDrm/ta/ta_utils.c
         Source/LinuxGbmDrm/drm/drm_atomic.c
         Source/LinuxGbmDrm/drm/drm_atomic.h
         Source/LinuxGbmDrm/drm/drm_common.c
         Source/LinuxGbmDrm/drm/drm_common.h
         Source/LinuxGbmDrm/drm/drm_log.c
         Source/LinuxGbmDrm/drm/drm_log.h
         Source/LinuxGbmDrm/drm/drm_prime.c
         Source/LinuxGbmDrm/drm/drm_prime.h
         )
    add_library( Common STATIC ${common_src} ${common_platform_src} )
    target_link_libraries( Common ${OPENGLES3_LIBRARY} ${EGL_LIBRARY})
endif()

#set( common_platform_src Source/LinuxGbmDrm//esUtil_gbm_drm.c )
#add_library( Common STATIC ${common_src} ${common_platform_src} )
#target_link_libraries( Common ${OPENGLES3_LIBRARY} ${EGL_LIBRARY} ${DRM_LIBRARY} ${X11_LIBRARIES} ${M_LIB} )

             


