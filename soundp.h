#ifndef SOUNDP_H
#define SOUNDP_H

#include <pthread.h>
#include <signal.h>
#include <time.h>
#include "common.h"
#include "sounda.h"
#include "settings.h"

extern "C"
{
    #include <pulse/simple.h>
    #include <pulse/error.h>
    #include <pulse/pulseaudio.h>
}

#define MAX_SOUND_VOLUME 150
#define DEFAULT_SOUND_VOLUME 90
#ifndef MAX_LATENCY
    #define MAX_LATENCY 300000 // in usecs
#endif

struct TSoundSettings
{
    unsigned long max_init_latancy;
    unsigned long min_soundtime_period;
    unsigned long max_soundtime_period;
    unsigned long good_buffers_time;
};

struct Toper_context
{
    pa_threaded_mainloop* loop;
    pa_stream * stream;
    char* name;
    pa_context * context;
    float curr_latency;
    float maxlatency;
    float timeelapsed;
    bool isdrain;
    TSoundSettings sgs;
};

class TSoundP : TNonCopyable
{
public:
    TSoundP(unsigned int _bitrate,
            unsigned int _nchanels,
          unsigned int _maxsamples,
            const TSmartPtr<Tsettings> &_sgs) noexcept;
    ~TSoundP();
    bool isError() const noexcept;
    bool write_pcm(const unsigned char *_data, size_t _samples);
    bool SetMasterVolume(unsigned _volume) noexcept;
    unsigned getVolume() const noexcept;
    bool pauseProcess() noexcept;
    bool do_drain() noexcept;
private:
    bool m_isError;
    unsigned int m_nchanels;
    pa_threaded_mainloop* m_mloop;
    pa_mainloop_api* m_mlapi = nullptr;
    pa_context * m_ctx = nullptr;
    unsigned m_volume;
    TSmartPtr<uint8_t> m_megabuffer;
    TSmartPtr<struct sigevent> m_psev;
    struct itimerspec m_ts;
    Toper_context m_operctx;
    pa_sample_spec m_spec;
    pa_stream *m_stm = nullptr;
    TSmartPtr<char> m_device_id_sink;
    struct pa_cvolume m_cvolume;
    std::size_t m_buffersize = 0;
    std::size_t m_samplesize = 0;
    // Size of frame (each frame contains one sample of each channel)
    std::size_t m_framesize; // in samples
    unsigned int m_buffer_time;
    unsigned int m_nbuffers = 0;
    unsigned int m_bufferIDX = 0;
    bool m_isdrain = false;
    TSmartPtr<struct async_private_data> m_data;
    pthread_t m_thread;
    timer_t m_timerId = 0;
    bool wait_operation(pa_operation * _op) noexcept;
    bool wait_connection() noexcept;
    bool wait_stream_connection() noexcept;
    bool set_async_loop_timer() noexcept;
};



#endif // SOUNDP_H
