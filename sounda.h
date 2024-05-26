#ifndef SOUNDA_H
#define SOUNDA_H

#include <pthread.h>
#include <signal.h>
#include <time.h>
#include "common.h"
#include "settings.h"

extern "C"
{
    #include <alsa/asoundlib.h>
}

struct sound_private_data
{
   unsigned char *samples;
   unsigned phase;
   unsigned nperiods;
   unsigned period_size;
   unsigned frame_size;
   bool isbusy;
   timer_t timerId;
   struct itimerspec m_ts;
   bool isdrain;
   unsigned watchdog;
};


class TSoundL : TNonCopyable
{
public:
    TSoundL(unsigned int _bitrate,
             int _nchanels,
          unsigned int _maxsamples,
            const TSmartPtr<Tsettings>& _sgs) noexcept;
    ~TSoundL();
    bool write_pcm(const unsigned char *_data, size_t _samples) noexcept;
    bool SetMasterVolume(unsigned _volume) noexcept;
    unsigned getVolume() const noexcept;
    bool do_drain() noexcept;
    bool isError() const noexcept;
private:
    bool m_isinit = false;
    unsigned int m_nchanels;
    snd_output_t* m_output = nullptr;
    snd_pcm_t * m_pcm_handle = nullptr;
    snd_pcm_hw_params_t *m_hwparams;
    snd_pcm_sw_params_t *m_swparams;
    snd_pcm_channel_area_t* m_soundArea = nullptr;
    snd_pcm_sframes_t m_period_size;
    snd_pcm_sframes_t m_buffer_size; // in samples
    // Size of frame (each frame contains one sample of each channel)
    snd_pcm_sframes_t m_framesize; // in samples
    unsigned int m_buffer_time;
    unsigned int m_nbuffers = 0;
    unsigned int m_bufferIDX = 0;
    // non interleaved mode is not supported
    //snd_pcm_channel_area_t * m_areas = nullptr;
    snd_async_handler_t * m_ahandler = nullptr;
    TSmartPtr<struct async_private_data> m_data;
    unsigned m_volume;
    TSmartPtr<uint8_t> m_megabuffer;
    pthread_t m_thread;
    timer_t m_timerId = 0;
    TSmartPtr<struct sigevent> m_psev;
    struct itimerspec m_ts;
    bool set_swparams() noexcept;
    bool set_hwparams(snd_pcm_access_t _access,
                      snd_pcm_format_t _format,
                      int _channels,
                      unsigned int _samplerate,
                      unsigned int _buffersize) noexcept;
    bool async_loop_alsa() noexcept;
    bool async_loop_timer() noexcept;
    bool pauseProcess() noexcept;
};


#endif // SOUNDA_H
