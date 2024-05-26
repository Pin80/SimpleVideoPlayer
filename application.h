#ifndef TAPPLICATION_H
#define TAPPLICATION_H
#include "video_reader.h"
#include <stdio.h>
#include <pthread.h>
#include "mainwindow.h"
#include "common.h"
#include "settings.h"

class TApplication : TNonCopyable
{
public:
    bool isError() const noexcept;
    int exec() noexcept;
    TApplication(int argc, char *argv[]);
    ~TApplication();
private:
    void process_events();
    bool m_isError = false;
    TSmartPtr<Twindow> m_window;
    TMainContext m_mctx;
    TEvent m_event = eNoEvent;
    TVariant m_eventParams[4];
    TSmartPtr<TVideo_reader> m_reader;
    TSmartPtr<TSound> m_soundp;
    int m_volume;
    bool m_mute;
    bool m_is_action;
    TSmartPtr<Tsettings> m_settings;
    bool update(uint8_t *&_data_out, size_t &_width, size_t &_height, size_t& _nsamples) noexcept;
};

#endif // TAPPLICATION_H
