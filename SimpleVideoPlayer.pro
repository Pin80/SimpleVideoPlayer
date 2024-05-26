TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt


# ALTERNATE FFMPEG LIBRARY LINK:
# Added create_prl option
# CONFIG += link_pkgconfig
# PKGCONFIG += libavcodec libavdevice libavfilter libavformat libavutil libswresample libswscale
#note: -lXxf86vm - problematic
# Packages which should be installed:
# 1) sudo apt-get install libglfw3
# 2) sudo apt-get install libglfw3-dev

CONFIG += link_pkgconfig
PKGCONFIG += libpulse libpulse-mainloop-glib libpulse-simple

DEFINES += CRITICAL_ERROR_CODE=-1
DEFINES += LOG_FILE_NAME='\\"videoplayer.log\\"'
DEFINES += ERRLOG_FILE_NAME='\\"videoplayer_error.log\\"'
DEFINES += SCREENSHOT_FILE_NAME='\\"screenshot\\"'
# 1 - Alsa 2 - Pulse
DEFINES += SOUND_WAY=2
# Size of pulseaudio buffer
DEFINES += DEFAULT_INIT_MAX_LATENCY=300000 # in usecs
DEFINES += MIN_SOUNDTIME_PERIOD=1000 # in usecs
DEFINES += MAX_SOUNDTIME_PERIOD=1000000 # in usecs
DEFINES += DEBUG_MODE
#DEFINES += ALSAASYNCMODE # for alsa
# Allowed delay for sound data circle megabuffer
DEFINES += GOOD_BUFFERS_TIME=100000 #usecs
DEFINES += TEMPORARY_FILE='\\"/tmp/fmt.tmp\\"'
DEFINES += SETTINGS_FILE='\\"./settings/settings.json\\"'
DEFINES += MIN_SCREEN_WIDTH=640
DEFINES += MIN_SCREEN_HEIGHT=480
DEFINES += SIMDJSON_EXCEPTIONS=0
unix {
    target.path = /usr/lib
    INSTALLS += target
# Included pathes
    INCLUDEPATH+=/opt/ffmpeg/v51_release/FFmpeg/build/include
    INCLUDEPATH+=/usr/include/x86_64-linux-gnu
    INCLUDEPATH+=/usr/local/include
    INCLUDEPATH+=/opt/glfw/glfw-3.3.8/include
# Included libraries
# C library requires  headers in "extern "C" block for valid linking
	LIBS += /home/user/MyBuild/build_test_ffmpeg6/ffmpeg.a
# General purpose
    LIBS+=/opt/zlib/zlib-1.2.13/libz.a
    LIBS+=-ldl
    LIBS+=/usr/lib/x86_64-linux-gnu/liblzma.a
# Alsa
    LIBS += -lasound
# Opengl
    LIBS += /opt/glfw/glfw-3.3.8/build2/src/libglfw3.a
    LIBS += -lGLU
    LIBS += -lGL
# General purpose
    LIBS += -lXrandr
    LIBS += -lXi
    LIBS += -lXinerama
    LIBS += -lX11
    LIBS += -lrt
    LIBS += -dl
    LIBS += -pthread
}
SOURCES += \
        application.cpp \
        common.cpp \
        glad/glad.c \
        json_parser/simdjson.cpp \
        main.cpp \
        mainwindow.cpp \
        settings.cpp \
        sounda.cpp \
        soundp.cpp \
        video_reader.cpp

DISTFILES += \
    Makefile.ffmpeg \
    create_link_to_res_in_build_dir.sh \
    known_issues.txt \
    pantry.txt \
    settings/settings.json

# Build libraw
!exists( $$/home/user/MyBuild/build_test_ffmpeg6/ffmpeg.a )
{
	system( make -f Makefile.ffmpeg )
}

HEADERS += \
	application.h \
	common.h \
	glad/glad.h \
	json_parser/simdjson.h \
	mainwindow.h \
        nostd/string_view.hpp \
	settings.h \
	sounda.h \
	soundp.h \
        video_reader.h \
