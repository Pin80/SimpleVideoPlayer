#include "sounda.h"
#include <cmath>
#include <new>
static const auto cdesired_format = SND_PCM_FORMAT_S16;
static const auto cmode = SND_PCM_STREAM_PLAYBACK;
static const auto caccess = SND_PCM_ACCESS_RW_INTERLEAVED;
static const unsigned int cbytes_per_sample = 2;
#define PCM_DEVICE "plughw:0,0"
#define PCM_DEVICE2 "default"
#define MAX_SOUND_VOLUME 150
#define BASE_UNDERRUN_TIMER 4300
// Size of period for async mode
const unsigned cperiod_size = 64;

struct async_private_data : public sound_private_data
{
   snd_pcm_channel_area_t *areas;
   snd_pcm_t * handle;
};

void async_callback(snd_async_handler_t * _ahandler)
{
   snd_pcm_t* handler = snd_async_handler_get_pcm(_ahandler);
   struct async_private_data* data = (async_private_data *)
                                     snd_async_handler_get_callback_private(_ahandler);
   if (data->phase >= data->nperiods)
       return;
   if (data->watchdog == 0)
   {
       snd_pcm_drain(handler);
       data->isdrain = true;
       return;
   }
   data->watchdog--;
   snd_pcm_sframes_t avail;
   data->isbusy = true;
   snd_pcm_sframes_t period_size = data->period_size;
   snd_pcm_sframes_t err;
   unsigned char* samples = data->samples + data->phase*data->period_size*data->areas[0].step;
   avail = snd_pcm_avail_update(handler);
   while (avail >= period_size)
   {
       err = snd_pcm_writei(handler, samples, period_size);
       if (err < 0)
       {
           elmsgr << "alsa: write error: " << snd_strerror(err) << mend;
           return;
       }
       if (err != period_size)
       {
           elmsgr << "write error: written: " << err << " but expected: " << period_size << mend;
           return;
       }
       avail = snd_pcm_avail_update(handler);
   }
   if (!data->isdrain)
       data->phase = (data->phase + 1)%data->nperiods;
   else
   {
       if (data->phase < data->nperiods)
           data->phase = data->phase + 1;
   }
   data->isbusy = false;
}

static void timer_proc(union sigval timer_data)
{
    struct async_private_data* data = static_cast<struct async_private_data*>
            (timer_data.sival_ptr);
    if ((data->phase >= data->nperiods) || (data->watchdog == 0))
    {
        return;
    }
    data->watchdog--;
    data->isbusy = true;
    snd_pcm_t* handler = data->handle;
    // non-interleaved sound data is not supported
    // snd_pcm_channel_area_t* areas = data->areas;
    unsigned period_size = data->period_size;
    unsigned char* samples = data->samples;
    samples = samples + data->period_size*data->phase*data->frame_size;
    auto state = snd_pcm_state(handler);
    if (state == SND_PCM_STATE_XRUN)
    {
        if (!data->isdrain)
        {
            elmsgr << "alsa: XRUN state detected (in no drain mode)" << mend;
            snd_pcm_prepare(handler);
        }
        state = snd_pcm_state(handler);
    }
    if ((state == SND_PCM_STATE_PREPARED) ||
        (state == SND_PCM_STATE_RUNNING))
    {
        int err = snd_pcm_writei(handler, samples, period_size);
        if (err == -EPIPE)
        {
            if (!data->isdrain)
            {
                elmsgr << "alsa: XRUN flag is detected in no drain" << mend;
                snd_pcm_prepare(handler);
            }
        }
        else if (err < 0)
        {
            elmsgr << "alsa: Can't write to PCM device: " << snd_strerror(err) << mend;
            return;
        }
        if (timer_settime(data->timerId, 0, &data->m_ts, NULL) != 0)
        {
            elmsgr << "alsa: timer_settime error" << mend;
            return;
        }
        int overrun = timer_getoverrun(data->timerId);
        if (overrun > 100)
            elmsgr << "timer_callback overrun:----------------------------------- " << overrun << mend;

    }
    else
    {
        elmsgr << "alsa: PCM device in bad state" << state << mend;
        return;
    }
    if (!data->isdrain)
        data->phase = (data->phase + 1)%data->nperiods;
    else
    {
        showmsg( "timer_proc in drain");
        if (data->phase < data->nperiods)
            data->phase = data->phase + 1;
    }
    data->isbusy = false;
}

TSoundL::TSoundL(unsigned int _bitrate,
             int _nchanels,
             unsigned int _maxsamples,
             const TSmartPtr<Tsettings> &_sgs) noexcept: m_volume(0)
{
    if (!_sgs)
        return;
    m_buffer_size = _maxsamples;
    m_isinit = false;
    if (_nchanels < 1)
        return;
    if (!SetMasterVolume(90))
        return;

    snd_pcm_hw_params_alloca(&m_hwparams);
    snd_pcm_sw_params_alloca(&m_swparams);

    int err;
    err = snd_output_stdio_attach(&m_output, stdout, 0);
    if (err < 0)
    {
        elmsgr << "Output failed: " << snd_strerror(err) << mend;
        return;
    }

    if ((err = snd_pcm_open(&m_pcm_handle, PCM_DEVICE, cmode, 0)) < 0)
    {
        elmsgr << "alsa: Playback open error: " << snd_strerror(err) << mend;
        return;
    }
    if (!set_hwparams(caccess, cdesired_format, _nchanels, _bitrate, m_buffer_size))
    {
        elmsgr << "Setting of hwparams failed" << mend;
        return;
    }
    if (!set_swparams())
    {
        elmsgr << "alsa: Setting of swparams failed" << mend;
        return;
    }
#ifdef DEBUG_MODE
    lmsgr << "alsa: audio device with PCM name: "
         << snd_pcm_name(m_pcm_handle)
         << " is used" << mend;
#endif
    snd_pcm_state_t pcm_state = snd_pcm_state(m_pcm_handle);
    const char * str_pcm_state = snd_pcm_state_name(pcm_state);

#ifdef DEBUG_MODE
    lmsgr << "alsa: PCM state: " << str_pcm_state << mend;
#endif
    snd_pcm_hw_params_get_channels(m_hwparams, &m_nchanels);
#ifdef DEBUG_MODE
    lmsgr << "alsa: set nchannels: " << m_nchanels << mend;
#endif

    unsigned int tmp = m_nchanels;
    if (_nchanels != m_nchanels)
    {
        showerrmsg("Can't set number of chanels");
        return;
    }
#ifdef DEBUG_MODE
    if (m_nchanels == 1)
    {
        showmsg("alsa: set mono sound");
    }
    else if (m_nchanels == 2)
    {
        showmsg("alsa: set stereo sound");
    }
    else
    {
        lmsgr << "alsa: set" << m_nchanels <<  "sound chanels" << mend;
    }
#endif
    snd_pcm_hw_params_get_rate(m_hwparams, &tmp, 0);
    if (_bitrate != tmp)
    {
        showerrmsg("can't set bitrate ");
        return;
    }
#ifdef DEBUG_MODE
    lmsgr << "alsa: set bitrate: " << tmp << mend;
#endif
    snd_pcm_uframes_t nframes; //1024
    // Allocate buffer to hold single period
    snd_pcm_hw_params_get_period_size(m_hwparams, &nframes, 0);
#ifdef DEBUG_MODE
    lmsgr << "alsa: set nframes per buffer: " << nframes << mend;
#endif
    // Extract period time from a configuration space in usecs
    snd_pcm_hw_params_get_period_time(m_hwparams, &tmp, NULL);
#ifdef DEBUG_MODE
    lmsgr << "alsa: set period time for buffer(in useconds): " << tmp << mend; //128000
#endif
    snd_pcm_prepare(m_pcm_handle);
    lmsgr << "alsa: buffer time is: " << m_buffer_time << " usecs" << mend;
    m_nbuffers = GOOD_BUFFERS_TIME/m_buffer_time;
    if (m_nbuffers < 2)
        m_nbuffers = 2;
    lmsgr << "alsa: is reserved: " << m_nbuffers << " buffers" << mend;
    m_framesize = _nchanels * snd_pcm_format_physical_width(cdesired_format) / 8;
    unsigned real_mbsize = m_nbuffers * m_buffer_size * m_framesize ;
    lmsgr << "real size of megabuffer:"  << real_mbsize << mend;
    m_megabuffer = TSmartPtr<uint8_t>(real_mbsize);
    if (!m_megabuffer)
    {
        showerrmsg("alsa: Not enough memory");
        return;
    }
    memset(&*m_megabuffer, 0, real_mbsize);
    // non interleaved mode is not supported
    /*
    m_areas = (snd_pcm_channel_area_t *)calloc(_nchanels, sizeof(snd_pcm_channel_area_t));
    if (m_areas == NULL)
    {
        showerrmsg("Not enough memory");
        return;
    }
    unsigned int chn;
    for (chn = 0; chn < _nchanels; chn++)
    {
        m_areas[chn].addr = (unsigned short *)m_megabuffer;
        m_areas[chn].first = chn * snd_pcm_format_physical_width(cdesired_format);
        m_areas[chn].step = m_framesize;
    }
    */
#ifdef ALSAASYNCMODE
    if (!async_loop_alsa())
    {
        printf("alsa async_loop is failed \n");
        return;
    }
#else
    if (!async_loop_timer())
    {
        showerrmsg("alsa: async_loop is failed");
        return;
    }
    sleep(1);
#endif
    if (!SetMasterVolume(90))
        return;
    m_isinit = true;
}

TSoundL::~TSoundL()
{
#ifdef ALSAASYNCMODE
    snd_async_del_handler(m_ahandler);
#else
    timer_delete(m_timerId);
#endif
    snd_pcm_close(m_pcm_handle);
    snd_pcm_hw_free(m_pcm_handle);
    snd_config_update_free_global();
    // non interleaved mode is not supported
    //free(m_areas);
}

bool TSoundL::set_hwparams(snd_pcm_access_t _access,
                           snd_pcm_format_t _format,
                           int _channels,
                           unsigned int _samplerate,
                           unsigned int _buffersize) noexcept
{
   snd_pcm_uframes_t size;
   int err = 0;

   // choose all parameters
   err = snd_pcm_hw_params_any(m_pcm_handle, m_hwparams);
   if (err < 0)
   {
       elmsgr << "Broken configuration for playback: no configurations available: "
             << snd_strerror(err) << mend;
       return false;
   }
   const int resample = 1;
   // set hardware resampling
   err = snd_pcm_hw_params_set_rate_resample(m_pcm_handle, m_hwparams, resample);
   if (err < 0)
   {
       elmsgr << "Resampling setup failed for playback: " << snd_strerror(err) << mend;
       return false;
   }
   // set the interleaved read/write format
   err = snd_pcm_hw_params_set_access(m_pcm_handle, m_hwparams, _access);
   if (err < 0)
   {
       elmsgr << "Access type not available for playback: " << snd_strerror(err) << mend;
       return false;
   }
   // set the sample format
   err = snd_pcm_hw_params_set_format(m_pcm_handle, m_hwparams, _format);
   if (err < 0)
   {
       elmsgr << "Sample format not available for playback: " << snd_strerror(err) << mend;
       return false;
   }
   // set the count of channels
   err = snd_pcm_hw_params_set_channels(m_pcm_handle, m_hwparams, _channels);
   if (err < 0)
   {
       elmsgr << "Channels count: " << _channels << " is not available for playbacks: "
             << " details:" << snd_strerror(err) << mend;
       return false;
   }
   // set the stream rate
   auto samplerate = _samplerate;
   err = snd_pcm_hw_params_set_rate_near(m_pcm_handle, m_hwparams, &samplerate, 0);
   if (err < 0)
   {
       elmsgr << "alsa: samplerate" << _samplerate << "Hz is not available for playback"
       << " details:" << snd_strerror(err) << mend;
       return false;
   }
   if (samplerate != _samplerate)
   {
       elmsgr << "Samplerate doesn't match (is requested: " << _samplerate
             << " (Hz), got: " << samplerate << mend;
       return false;
   }

   err = snd_pcm_hw_params_set_buffer_size(m_pcm_handle, m_hwparams, _buffersize);
   if (err < 0)
   {
       elmsgr << "Unable to set buffer siize" << _buffersize << " for playback"
             << " details: " << snd_strerror(err) << mend;
       return false;
   }
   err = snd_pcm_hw_params_get_buffer_size(m_hwparams, &size);
   if (err < 0)
   {
       elmsgr << "Unable to get buffer size for playback: " <<  snd_strerror(err) << mend;
       return false;
   }
   // set the buffer time
   err = snd_pcm_hw_params_get_buffer_time(m_hwparams, &m_buffer_time, nullptr);
   if (err < 0)
   {
       elmsgr << "Unable to get buffer time for playback: " << snd_strerror(err) << mend;
       return false;
   }

   m_buffer_size = _buffersize;
   m_period_size = (m_buffer_size > cperiod_size)?cperiod_size:m_buffer_size;
   lmsgr << "alsa: period size is:" << m_period_size << mend;
   /* set the period time */
   auto period_time = m_period_size*1000000/_samplerate;
   lmsgr << "alsa: eval period time is: " << period_time << mend;
   err = snd_pcm_hw_params_set_period_size(m_pcm_handle, m_hwparams, m_period_size, 0);
   if (err < 0)
   {
       elmsgr << "Unable to set period time %u for playback: "
             << m_period_size << snd_strerror(err) << mend;
       return false;
   }
   err = snd_pcm_hw_params_get_period_size(m_hwparams, &size, 0);
   if (err < 0)
   {
       elmsgr << "Unable to get period size for playback: " << snd_strerror(err) << mend;
       return false;
   }
   /* write the parameters to device */
   err = snd_pcm_hw_params(m_pcm_handle, m_hwparams);
   if (err < 0)
   {
       elmsgr << "Unable to set hw params for playback: " << snd_strerror(err) << mend;
       return false;
   }
   return true;
}

bool TSoundL::set_swparams() noexcept
{
   int err;
   // get the current swparams
   err = snd_pcm_sw_params_current(m_pcm_handle, m_swparams);
   if (err < 0)
   {
       elmsgr << "Unable to determine current swparams for playback: "
              << snd_strerror(err) << mend;
       return false;
   }
   // start the transfer when the buffer is almost full:
   // (buffer_size / avail_min) * avail_min
   err = snd_pcm_sw_params_set_start_threshold(m_pcm_handle, m_swparams, m_buffer_size);
   if (err < 0)
   {
       elmsgr << "Unable to set start threshold mode for playback: "
             << snd_strerror(err) << mend;
       return false;
   }
   // allow the transfer when at least period_size samples can be processed
   // or disable this mechanism when period event is enabled (aka interrupt like style processing)
   err = snd_pcm_sw_params_set_avail_min(m_pcm_handle, m_swparams, m_period_size);
   if (err < 0)
   {
       elmsgr << "Unable to set avail min for playback: " << snd_strerror(err) << mend;
       return false;
   }
   //I know nothing about snd_pcm_sw_params_set_period_event
   // enable period events when requested
   /*
   if (period_event) {
       err = snd_pcm_sw_params_set_period_event(handle, swparams, 1);
       if (err < 0) {
           showerrmsg("Unable to set period event: ", snd_strerror(err));
           return err;
       }
   }
   */
   // write the parameters to the playback device
   err = snd_pcm_sw_params(m_pcm_handle, m_swparams);
   if (err < 0)
   {
       elmsgr << "Unable to set sw params for playback: " << snd_strerror(err) << mend;
       return false;
   }
   return true;
}

bool TSoundL::async_loop_alsa() noexcept
{
   //struct async_private_data data;
   m_data = TSmartPtr<struct async_private_data>(new(std::nothrow) async_private_data );
   int err, count;
   showmsg("async loop");
   m_data->samples = unconst(m_megabuffer.get());
   // non interleaved mode is not supported
   //m_data->areas = m_areas;
   m_data->phase = 0;
   m_data->isdrain = false;
   m_data->nperiods = m_nbuffers*m_buffer_size/m_period_size;
   m_data->period_size = m_period_size;
   m_data->watchdog = m_data->nperiods*2;
   void* ptr_pd = const_cast<async_private_data*>(m_data.get());
   err = snd_async_add_pcm_handler(&m_ahandler, m_pcm_handle,
                                   async_callback, ptr_pd);
   if (err < 0)
   {
       showerrmsg("Unable to register async handler ");
       return false;
   }
   auto ptr_mega = unconst(m_megabuffer.get());
   for (count = 0; count < 2; count++)
   {
       err = snd_pcm_writei(m_pcm_handle, ptr_mega, m_period_size);
       if (err < 0)
       {
           elmsgr << "alsa: Initial write error: " << snd_strerror(err) << mend;
           return false;
       }
       if (err != m_period_size)
       {
           elmsgr << "Initial write error: was written: " << err << " was expected: "
                 << m_period_size << mend;
           return false;
       }
   }
   return true;
}

bool TSoundL::async_loop_timer() noexcept
{
    m_data = TSmartPtr<struct async_private_data>(new(std::nothrow) async_private_data);
    m_data->handle = m_pcm_handle;
    m_data->samples = unconst(m_megabuffer.get());
    // non interleaved mode is not supported
    //m_data->areas = m_areas;
    m_data->phase = 0;
    m_data->nperiods = m_nbuffers;
    // m_data->period_size == m_buffer_size
    m_data->period_size = m_nbuffers*m_buffer_size/m_data->nperiods;
    m_data->frame_size = m_framesize;
    m_data->isdrain = false;
    m_data->watchdog = m_nbuffers*2;
    m_psev = TSmartPtr<struct sigevent>(new(std::nothrow) sigevent);
    memset(const_cast<struct sigevent*>(m_psev.get()), 0, sizeof(sigevent));
    m_psev->sigev_notify = SIGEV_THREAD;
    m_psev->sigev_notify_function = &timer_proc;
    m_psev->sigev_value.sival_ptr = const_cast<struct async_private_data *>(m_data.get());

    if (timer_create(CLOCK_MONOTONIC, const_cast<struct sigevent*>(m_psev.get()), &m_timerId) != 0)
    {
        elmsgr << "alsa: timer_create error" << mend;
        return false;
    }
    m_data->timerId = m_timerId;
    // only for debug
    //printf("alsa: Timer ID: %u\n", (size_t) m_timerId);
    //printf("alsa: buffer time in usecs is %u \n", m_buffer_time);
    const auto onemillion = 1000'000;
    m_buffer_time = m_nbuffers*m_buffer_time /m_data->nperiods -
                    m_nbuffers*BASE_UNDERRUN_TIMER/m_data->nperiods;
    if (m_buffer_time < MIN_SOUNDTIME_PERIOD)
    {
        showerrmsg("alsa: timer period is too small");
        return false;
    }
    if (m_buffer_time > MAX_SOUNDTIME_PERIOD)
    {
        showerrmsg("alsa: timer period is too large");
        return false;
    }
    m_ts.it_interval.tv_sec = m_buffer_time/onemillion;
    m_ts.it_interval.tv_nsec = m_buffer_time * 1000;
    m_ts.it_value.tv_sec = m_buffer_time/onemillion;
    m_ts.it_value.tv_nsec = m_buffer_time * 1000;
    m_data->m_ts = m_ts;

    if (timer_settime(m_timerId, 0, &m_ts, NULL) != 0)
    {
        showerrmsg("alsa: timer_settime error");
        return false;
    }

    return true;
}

bool TSoundL::write_pcm(const unsigned char *_data, size_t _samples) noexcept
{
    if (!m_isinit)
        return false;
    if (_samples > m_buffer_size)
    {
        elmsgr << "alsa: size of audio packet ( " << _samples
              << " samples ) exceeds buffer size" << mend;
        return false;
    }
    std::size_t nbytes = m_nchanels *_samples * m_framesize;
#ifdef ALSAASYNCMODE
    int pcm_result;
    m_data->watchdog = m_data->nperiods*2;
    auto state = snd_pcm_state(m_pcm_handle);
    if (state == SND_PCM_STATE_PREPARED)
    {
        if (m_data->isdrain)
        {
            // Do nothing
        }
        else
        {
            pcm_result = snd_pcm_start(m_pcm_handle);
            if (pcm_result < 0)
            {
                elmsgr << "alsa: Async Start error: " << snd_strerror(pcm_result) << mend;
                return false;
            }
            m_data->phase = 0;
            showmsg("alsa: async handler is started ");
        }
    }
    else if (state == SND_PCM_STATE_XRUN)
    {
        pcm_result = snd_pcm_prepare(m_pcm_handle);
        if (pcm_result < 0)
        {
            elmsgr << "alsa: Async Start error: " << snd_strerror(pcm_result) << mend;
            return false;
        }
        pcm_result = snd_pcm_start(m_pcm_handle);
        if (pcm_result < 0)
        {
            elmsgr << "alsa: Async Start error: "<< snd_strerror(pcm_result) << mend;
            return false;
        }
        m_data->phase = 0;
        showerrmsg("alsa: async handler is restored ");
    }
    else if (state == SND_PCM_STATE_DRAINING)
    {
        showerrmsg("alsa: found draining state ---------------------------");
    }
    if (m_data->isdrain)
    {
        showerrmsg("alsa: found draining flag ---------------------------");
        m_data->isdrain = false;
        m_data->phase = 0;
        pcm_result = snd_pcm_drop(m_pcm_handle);
        if (pcm_result < 0)
        {
            elmsgr << "alsa: Async Drop error: " << snd_strerror(pcm_result) << mend;
            return false;
        }
        snd_async_del_handler(m_ahandler);
        pcm_result = snd_pcm_prepare(m_pcm_handle);
        if (pcm_result < 0)
        {
            elmsgr << "alsa: Async Prepare error: " << snd_strerror(pcm_result) << mend;
            return false;
        }
        if (!async_loop_alsa())
        {
            showerrmsg("alsa async_loop is failed");
            return false; // exit(-1);
        }
        m_bufferIDX = 0;
    }
    auto buffer = m_megabuffer + m_framesize*m_buffer_size*m_bufferIDX;
    m_bufferIDX = (m_bufferIDX + 1)%m_nbuffers;
    if (_samples < m_buffer_size)
    {
        memset(buffer, 0, m_framesize*m_buffer_size);
    }
    memcpy(buffer, _data, _samples*m_data->areas[0].step);
    return true;
#else
    if (m_data)
    {
        if (m_data->isdrain)
        {
            m_bufferIDX = 0;
        }
        m_data->watchdog = m_nbuffers*2;
        unsigned char* buffer = unconst(m_megabuffer.get()) + m_framesize*m_buffer_size*m_bufferIDX;
        if (_samples < m_buffer_size)
        {
            memset(buffer, 0, m_framesize*m_buffer_size);
        }
        memcpy(buffer, _data, _samples*m_framesize);
        m_bufferIDX = (m_bufferIDX + 1)%m_nbuffers;
        if (m_data->isdrain)
        {
            snd_pcm_prepare(m_pcm_handle);
            m_data->isdrain = false;
            m_data->phase = 0;
        }
    }
    return true;
#endif
}


bool TSoundL::SetMasterVolume(unsigned _volume) noexcept
{
    if (_volume > MAX_SOUND_VOLUME)
        return false;
    long min, max;
    snd_mixer_t * mixer_handle = nullptr;
    snd_mixer_selem_id_t *sid = nullptr;
    int err;
    const char *selem_name_master = "Master";
    // second argument isn't used and doesn't have any purpose
    err = snd_mixer_open(&mixer_handle, 0); // Opens an empty mixer.
    if (err < 0)
    {
        showerrmsg("alsa: mixer is not opened ");
        return false;
    }
    err = snd_mixer_attach(mixer_handle, PCM_DEVICE2);
    if (err < 0)
    {
        showerrmsg("alsa: mixer is not attached ");
        return false;
    }
    err = snd_mixer_selem_register(mixer_handle, NULL, NULL);
    if (err < 0)
    {
        showerrmsg("alsa: mixer element class is not regidtered");
        return false;
    }
    err = snd_mixer_load(mixer_handle);
    if (err < 0)
    {
        showerrmsg("alsa: mixer element is not loaded");
        return false;
    }
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name_master);
    snd_mixer_elem_t* elem1 = snd_mixer_find_selem(mixer_handle, sid);
    if (elem1 == nullptr)
    {
        showerrmsg("alsa: Master mixer element is not found");
    }
    err = snd_mixer_selem_get_playback_volume_range(elem1, &min, &max);
    if (err < 0)
    {
        showerrmsg("alsa: volume range is not changed");
    }
    double fvolume = (double)_volume * (max - min) / 100.;
    long volume = min + fvolume;
    err = snd_mixer_selem_set_playback_volume_all(elem1, volume);
    if (err < 0)
    {
        showerrmsg("alsa: volume is not changed");
    }
    snd_mixer_close(mixer_handle);
    m_volume = _volume;
    return true;
}

unsigned TSoundL::getVolume() const noexcept
{
    return m_volume;
}

bool TSoundL::do_drain() noexcept
{
    if (!m_pcm_handle)
    {
        elmsgr << "pcm handle is not found" << mend;
        return false;
    }
#ifndef ALSAASYNCMODE
    m_data->isdrain = true;
    while ((m_data->phase < m_data->nperiods) && (m_data->watchdog != 0))
    {
        sleep(1);
    }
    return snd_pcm_drain(m_pcm_handle) == 0;
#else
    if (!m_data->isdrain)
    {
        m_data->isdrain = true;
        while ((m_data->phase < m_data->nperiods) && (m_data->watchdog != 0))
        {
            sleep(1);
        }
    }
    return snd_pcm_drain(m_pcm_handle) == 0;
#endif
    memset(unconst(m_megabuffer.get()), 0, m_nbuffers*m_buffer_size*m_framesize);
}

bool TSoundL::isError() const noexcept
{
    return !m_isinit;
}

bool TSoundL::pauseProcess() noexcept
{
    snd_pcm_drop(m_pcm_handle);
    snd_pcm_prepare(m_pcm_handle);
    return true;
}

