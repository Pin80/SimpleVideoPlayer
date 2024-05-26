#include "soundp.h"
#include <stdio.h>
#include <chrono>
#include <thread>
#include <string.h>
#define PULSE_OPERATION_TIMEOUT 5000 // msecs
#define TIME_EVENT_USEC 50000
#define BASE_UNDERRUN_TIMER 4300
static const auto cdesired_format = PA_SAMPLE_S16LE;
static const auto capp_name = "SimpleVideoPlayer";
static const auto cstm_name = "SimpleVideoPlayer_stream";

struct async_private_data : public sound_private_data
{
    Toper_context* operctx;
};

void on_drained(pa_stream *s, int success, void *udata)
{
    (void)success;
    assert(s);
    assert(udata);
    Toper_context *operctx = static_cast<Toper_context *>(udata);
    pa_threaded_mainloop_signal(operctx->loop, 0);
}

/* Show the current latency */
static void on_stream_update_timing(pa_stream *s,
                                          int success,
                                          void *udata)
{
    assert(s);
    assert(udata);
    Toper_context *operctx = static_cast<Toper_context *>(udata);
    pa_usec_t l, usec;
    int negative = 0;

    if (!success ||
        pa_stream_get_time(s, &usec) < 0 ||
        pa_stream_get_latency(s, &l, &negative) != 0)
    {
        showerrmsg("pulse: error: son_stream_update_timing callback error");
        pa_threaded_mainloop_signal(operctx->loop, 0);
        return;
    }
    float latency = (float) l * (negative?-1.0f:1.0f);
    if (latency > operctx->curr_latency)
        operctx->maxlatency = latency;
    operctx->curr_latency = latency;
    operctx->timeelapsed = (float)usec/1000;
    pa_threaded_mainloop_signal(operctx->loop, 0);
}

static void on_time_event(pa_mainloop_api *m,
                                pa_time_event *e,
                                const struct timeval *t,
                                void *udata)
{
    (void)t;
    assert(m);
    assert(udata);
    Toper_context *operctx = static_cast<Toper_context *>(udata);
    auto stream = operctx->stream;
    auto context = operctx->context;
    assert(stream);
    assert(context);
    if (pa_stream_get_state(stream) == PA_STREAM_READY)
    {
        pa_operation *o = pa_stream_update_timing_info(stream,
                                                       on_stream_update_timing,
                                                       udata);
        if (!o)
        {
            elmsgr << "pulse: error in time_event_callback: "
                  << pa_strerror(pa_context_errno(context)) << mend;
        }
        else
            pa_operation_unref(o);
    }
    pa_context_rttime_restart(context, e, pa_rtclock_now() + TIME_EVENT_USEC);
}

/*********** Stream callbacks **************/
static void stream_success(pa_stream *s, int succes, void *udata)
{
    (void)succes;
    assert(s);
    showmsg("Succeded");
    assert(udata);
    Toper_context *operctx = static_cast<Toper_context *>(udata);
    pa_threaded_mainloop_signal(operctx->loop, 0);
}

static void on_stream_event(pa_stream *s,
                                  const char *name,
                                  pa_proplist *pl,
                                  void *udata)
{
    assert(s);
    assert(name);
    assert(pl);
    assert(udata);
    char* t = pa_proplist_to_string_sep(pl, ", ");
    lmsgr << "pulse: Got event:" << name
         << " properties:" << t << mend;
    pa_xfree(t);
    Toper_context *operctx = static_cast<Toper_context *>(udata);
    pa_threaded_mainloop_signal(operctx->loop, 0);
}

static void on_stream_suspended(pa_stream *s, void *udata)
{
    assert(s);
    assert(udata);
    if (pa_stream_is_suspended(s))
        showmsg("pulse: Stream device suspended.");
    else
      showmsg("pulse: Stream device resumed.");
    Toper_context *operctx = static_cast<Toper_context *>(udata);
    pa_threaded_mainloop_signal(operctx->loop, 0);
}

static void on_stream_buffer_attr(pa_stream *s, void *udata)
{
    assert(s);
    assert(udata);
    showmsg("pulse: Stream buffer attributes is changed");
    Toper_context *operctx = static_cast<Toper_context *>(udata);
    pa_threaded_mainloop_signal(operctx->loop, 0);
}

static void on_stream_underflow(pa_stream *s, void *udata)
{
    assert(s);
    assert(udata);
    showerrmsg("pulse: Stream is underrun");
    Toper_context *operctx = static_cast<Toper_context *>(udata);
    pa_threaded_mainloop_signal(operctx->loop, 0);
}

static void on_stream_overflow(pa_stream *s, void *udata)
{
    assert(s);
    assert(udata);
    showerrmsg("Stream is overrun");
    Toper_context *operctx = static_cast<Toper_context *>(udata);
    pa_threaded_mainloop_signal(operctx->loop, 0);
}


static void on_state_changed(pa_context *c, void *udata)
{
    assert(c);
    assert(udata);
    Toper_context *operctx = static_cast<Toper_context *>(udata);
    switch (pa_context_get_state(c))
    {
        case PA_CONTEXT_UNCONNECTED:
        {
            showmsg("pulse: connection is closed");
            break;
        }
        case PA_CONTEXT_CONNECTING:
        {
            showmsg("pulse: connection is established");
            break;
        }
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
        {
            showmsg("pulse: another state is established");
            break;
        }
        case PA_CONTEXT_READY:
        {
            showmsg("pulse: server is ready");
            break;
        }
        case PA_CONTEXT_FAILED:
        {
            showmsg("pulse: state is error");
            break;
        }
        case PA_CONTEXT_TERMINATED:
        {
            showmsg("pulse: context is terminated");
            break;
        }
        default:
        {
            showmsg("pulse: another state");
            break;
        }
    }
    pa_threaded_mainloop_signal(operctx->loop, 0);
}

static void on_dev_sink(pa_context *c, const pa_sink_info *info, int eol, void *udata)
{
    assert(c);
    assert(udata);
    const char* filter = "output";
    Toper_context *operctx = static_cast<Toper_context *>(udata);
    if (eol != 0)
    {
        showerrmsg("pulse: on_dev_sink callback error or end of list");
        pa_threaded_mainloop_signal(operctx->loop, 0);
        return;
    }
    if (!info)
    {
        showerrmsg("pulse: on_dev_sink callback error2");
        pa_threaded_mainloop_signal(operctx->loop, 0);
        return;
    }
    const char *device_name = info->name;
    if (device_name)
    {
        if (operctx->name == nullptr)
        {
            auto res = strstr(device_name, filter);
            if (res != nullptr)
                operctx->name = strdup(device_name);
        }
        lmsgr << "pulse: found device sink name: " << device_name << mend;
    }
    else
    {
        showerrmsg("pulse: general callback error");
    }
    pa_threaded_mainloop_signal(operctx->loop, 0);
}

static void on_dev_source(pa_context *c, const pa_source_info *info, int eol, void *udata)
{
    assert(c);
    assert(udata);
    const char* filter = "input";
    Toper_context *operctx = static_cast<Toper_context *>(udata);
    if (eol != 0)
    {
        showerrmsg("pulse: on_dev_source callback error or end of list");
        pa_threaded_mainloop_signal(operctx->loop, 0);
        return;
    }
    if (!info)
    {
        showerrmsg("pulse: on_dev_source callback error2");
        pa_threaded_mainloop_signal(operctx->loop, 0);
        return;
    }
    const char *device_name = info->name;
    if (device_name)
    {
        if (operctx->name == nullptr)
        {
            auto res = strstr(device_name, filter);
            if (res != nullptr)
                operctx->name = strdup(device_name);
        }
        lmsgr << "pulse: found device source name: " << device_name << mend;
    }
    pa_threaded_mainloop_signal(operctx->loop, 0);
}

static void on_io_completed(pa_stream *s, size_t nbytes, void *udata)
{
    assert(s);
    assert(udata);
    Toper_context *operctx = static_cast<Toper_context *>(udata);
    lmsgr << "pulse: io operation is completed: " << nbytes << " bytes" << mend;
    pa_threaded_mainloop_signal(operctx->loop, 0);
    return;
}

static void on_written( void *udata)
{
    assert(udata);
    showmsg("pulse: on_written: is written");
    Toper_context *operctx = static_cast<Toper_context *>(udata);
    if (operctx->isdrain)
    {
        operctx->curr_latency = 0;
        operctx->maxlatency = 0;
        operctx->isdrain = false;
    }
    return;
}


static void on_volume_set(pa_context *c, int success, void *udata)
{
    assert(c);
    assert(udata);
    Toper_context *operctx = static_cast<Toper_context *>(udata);
    if (success)
        showmsg("pulse: Volume is set");
    else
        showerrmsg("pulse: Volume is not set");
    pa_threaded_mainloop_signal(operctx->loop, 0);
}

static void timer_proc(union sigval timer_data)
{
    struct async_private_data* data = static_cast<struct async_private_data*>
            (timer_data.sival_ptr);
    auto operctx = data->operctx;
    if ((data->phase >= data->nperiods) || (data->watchdog == 0))
    {
        return;
    }
    data->watchdog--;
    data->isbusy = true;
    unsigned period_size = data->period_size;
    unsigned char* samples = data->samples;
    samples = samples + period_size*data->phase*data->frame_size;
    if ((operctx->maxlatency > operctx->sgs.max_init_latancy) && (!data->isdrain))
    {
        elmsgr << "pulse: current biggest latency (" << operctx->maxlatency
              << " usecs ) exceeds maximum latency" << mend;
        operctx->maxlatency = 0;
        return;
    }
    pa_threaded_mainloop_lock(operctx->loop);
    std::size_t navail = pa_stream_writable_size(operctx->stream);
    if (navail < data->period_size*data->frame_size)
    {
        pa_threaded_mainloop_unlock(operctx->loop);
        lmsgr << "pulse: error in write_pcm. Only " << navail << " bytes is available" << mend;
        lmsgr << "pulse: biggest latency is " <<  operctx->maxlatency << " usecs" << mend;
        //m_isinit = false;
        return;
    }
    auto res = pa_stream_write(operctx->stream, samples, period_size*data->frame_size, NULL, 0, PA_SEEK_RELATIVE);
    if (res < 0)
    {
        pa_threaded_mainloop_unlock(operctx->loop);
        showerrmsg("pulse: error in pa_stream_write");
        return;
    }
    else
    {
        lmsgr << "pulse: have been written " << period_size << " frames, current latency is: "
             << operctx->maxlatency <<  " usecs, time elapsed: "
             << operctx->timeelapsed << " msecs" << " phase:" << data->phase << mend;
    }
    pa_threaded_mainloop_unlock(operctx->loop);

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

TSoundP::TSoundP(unsigned int _bitrate,
                 unsigned _nchanels,
                 unsigned int _maxsamples,
                 const TSmartPtr<Tsettings>& _sgs) noexcept:
    m_nchanels(_nchanels), m_buffersize(_maxsamples)
{
    m_isError = true;
    m_operctx.loop = nullptr;
    m_operctx.name = nullptr;
    m_operctx.context = nullptr;
    m_operctx.stream = nullptr;
    m_operctx.maxlatency = 0;
    m_operctx.timeelapsed = 0;
    m_operctx.isdrain = false;
    void *udata = &m_operctx;
    if (!_sgs)
        return;
    if ((_bitrate == (unsigned)-1) || (_nchanels == (unsigned)-1) || (_maxsamples == (unsigned)-1))
        return;
    // Create a mainloop API and connection to the default server
    m_mloop = pa_threaded_mainloop_new();
    pa_threaded_mainloop_start(m_mloop);

    if (!_sgs->getValue("sound_max_latancy", m_operctx.sgs.max_init_latancy))
    {
        showerrmsg("Couldn't find \"sound_max_latancy\" in settings file");
        m_operctx.sgs.max_init_latancy = DEFAULT_INIT_MAX_LATENCY;
    }
    if (!_sgs->getValue("sound_min_soundtime_period", m_operctx.sgs.min_soundtime_period))
    {
        showerrmsg("Couldn't find \"sound_min_soundtime_period\" in settings file");
        m_operctx.sgs.min_soundtime_period = MIN_SOUNDTIME_PERIOD;
    }
    if (!_sgs->getValue("sound_max_soundtime_period", m_operctx.sgs.max_soundtime_period))
    {
        showerrmsg("Couldn't find \"sound_max_soundtime_period\" in settings file");
        m_operctx.sgs.max_soundtime_period = MAX_SOUNDTIME_PERIOD;
    }
    if (!_sgs->getValue("sound_good_buffers_time", m_operctx.sgs.good_buffers_time))
    {
        showerrmsg("Couldn't find \"sound_good_buffers_time\" in settings file");
        m_operctx.sgs.good_buffers_time = GOOD_BUFFERS_TIME;
    }
    m_mlapi = pa_threaded_mainloop_get_api(m_mloop);
    m_ctx = pa_context_new_with_proplist(m_mlapi, capp_name, NULL);
    m_operctx.loop = m_mloop;
    pa_context_set_state_callback(m_ctx, on_state_changed, udata);

    // This function connects to the pulse server
    pa_context_flags_t flags = PA_CONTEXT_NOFLAGS;
    pa_context_connect(m_ctx, NULL, flags, NULL);
    if (!wait_connection())
    {
        showerrmsg("pulse: pa_context_connect error");
        return;
    }
    pa_threaded_mainloop_lock(m_mloop);
    pa_operation *op = nullptr;
    op = pa_context_get_sink_info_list(m_ctx, on_dev_sink, udata);
    if (!wait_operation(op))
    {
        pa_operation_unref(op);
        pa_threaded_mainloop_unlock(m_mloop);
        showerrmsg("pulse: pa_context_get_sink_info_list error");
        return;
    }
    if (m_operctx.name)
    {
        m_device_id_sink.reset(strdup(m_operctx.name));
        lmsgr << "pulse: choosen device sink name: " << m_operctx.name << mend;
        free(m_operctx.name);
        m_operctx.name = nullptr;
    }
    else
    {
        pa_operation_unref(op);
        pa_threaded_mainloop_unlock(m_mloop);
        showerrmsg("pulse: not found  sink name");
        return;
    }
    pa_operation_unref(op);
    op = pa_context_get_source_info_list(m_ctx, on_dev_source, udata);
    if (!wait_operation(op))
    {
        pa_operation_unref(op);
        pa_threaded_mainloop_unlock(m_mloop);
        showerrmsg("pulse: pa_context_get_source_info_list operation error ");
        return;
    }
    if (m_operctx.name)
    {
        lmsgr << "pulse: choosen device source name: " << m_operctx.name << mend;
        free(m_operctx.name);
        m_operctx.name = nullptr;
    }
    pa_operation_unref(op);
    m_spec.channels = m_nchanels;
    m_spec.format = PA_SAMPLE_S16LE;
    m_spec.rate = _bitrate;
    m_samplesize = pa_sample_size(&m_spec);
    m_framesize = m_samplesize*m_spec.channels;

    const auto cmillion = 1000'000;
    m_buffer_time = m_buffersize*cmillion/m_spec.rate;
    m_nbuffers = m_operctx.sgs.good_buffers_time/m_buffer_time;
    if (m_nbuffers < 2)
        m_nbuffers = 2;
    lmsgr << "pulse: size of buffer is " << m_buffersize
          <<  " (frames), eval time of buffer:" << m_buffer_time << " usecs" << mend;
    m_stm = pa_stream_new(m_ctx, cstm_name, &m_spec, NULL);
    if (!m_stm)
    {
        showerrmsg("pulse: error of call of pa_stream_new");
    }
    m_operctx.stream = m_stm;
    m_operctx.context = m_ctx;
    pa_stream_set_suspended_callback(m_stm, on_stream_suspended, udata);
    //pa_stream_set_moved_callback(m_stm, stream_moved_callback, NULL);
    pa_stream_set_underflow_callback(m_stm, on_stream_underflow, udata);
    pa_stream_set_overflow_callback(m_stm, on_stream_overflow, udata);
    //pa_stream_set_started_callback(m_stm, stream_started_callback, NULL);
    pa_stream_set_event_callback(m_stm, on_stream_event, udata);
    pa_stream_set_buffer_attr_callback(m_stm, on_stream_buffer_attr, NULL);

    pa_buffer_attr attr;
    memset(&attr, 0xff, sizeof(attr));
    float max_latency;
    max_latency = ((float)m_operctx.sgs.max_init_latancy/(float)1000000);
    float onebufferlatency = (float)_maxsamples/(float)_bitrate;
    if (onebufferlatency > max_latency)
    {
        pa_threaded_mainloop_unlock(m_mloop);
        showerrmsg("pulse: latency of buffer esceeds inint maximum latency");
        return;
    }
    unsigned  reserved_buffers = max_latency/onebufferlatency;
    lmsgr << "pulse: have been reserved buffers: " << reserved_buffers << mend;
    attr.tlength = reserved_buffers*m_buffersize*m_framesize;
    attr.maxlength = -1;
    attr.prebuf = attr.tlength/2;
    //attr.minreq = -1;
    //attr.fragsize = -1;
    pa_stream_set_write_callback(m_stm, on_io_completed, udata);
    pa_stream_flags_t sflags = static_cast<pa_stream_flags_t>(PA_STREAM_ADJUST_LATENCY |
                                                              PA_STREAM_INTERPOLATE_TIMING);
    pa_stream_connect_playback(m_stm, m_device_id_sink.get(), &attr, sflags, NULL, NULL);
    if (!wait_stream_connection())
    {
        pa_threaded_mainloop_unlock(m_mloop);
        showerrmsg("pulse: error: stream connection error");
        return;
    }
    pa_time_event *time_event;
    time_event = pa_context_rttime_new(m_ctx, pa_rtclock_now() + TIME_EVENT_USEC,
                                       on_time_event,
                                       udata);
    if (!time_event)
    {
       showerrmsg("pulse: error of call of pa_context_rttime_new");
    }
    size_t n = pa_stream_writable_size(m_stm);
    lmsgr << "pulse: buffer size:" << m_buffersize << " availble bytes to write:" << n << mend;
    m_megabuffer = TSmartPtr<uint8_t>(m_nbuffers*m_buffersize*m_framesize);
    pa_threaded_mainloop_unlock(m_mloop);
    SetMasterVolume(DEFAULT_SOUND_VOLUME);
    if (!set_async_loop_timer())
    {
        showerrmsg("pulse: error of timer initialization");
        return;
    }
    m_isError = false;
}

TSoundP::~TSoundP()
{
    if (m_stm)
    {
        pa_stream_disconnect(m_stm);
        pa_stream_unref(m_stm);
    }
    if (m_ctx)
    {
        pa_context_disconnect(m_ctx);
    }
    if (m_mloop)
    {
        pa_threaded_mainloop_stop(m_mloop);
        pa_threaded_mainloop_free(m_mloop);
    }
}

bool TSoundP::isError() const noexcept
{
    return m_isError;
}

bool TSoundP::SetMasterVolume(unsigned int _volume) noexcept
{
    if (_volume > MAX_SOUND_VOLUME)
        return false;
    pa_threaded_mainloop_lock(m_mloop);
    pa_volume_t volume = _volume*PA_VOLUME_UI_MAX/100;
    pa_cvolume_set(&m_cvolume, m_spec.channels, volume);
    pa_operation* op = pa_context_set_sink_volume_by_name(m_ctx,
                                       m_device_id_sink.get(),
                                       &m_cvolume,
                                       on_volume_set,
                                       &m_operctx
                                       );
    m_volume = _volume;
    wait_operation(op);
    pa_threaded_mainloop_unlock(m_mloop);
    return true;
}

unsigned int TSoundP::getVolume() const noexcept
{
    return m_volume;
}

bool TSoundP::do_drain() noexcept
{
    if (m_isError)
        return false;
    if (!m_stm)
        return false;
    assert(m_mloop);
    m_isdrain = true;
    pa_threaded_mainloop_lock(m_mloop);
    void *udata = &m_operctx;
    pa_operation *op = pa_stream_drain(m_stm, on_drained, udata);
    if (!wait_operation(op))
    {
        showerrmsg("pulse:  drain error");
        pa_threaded_mainloop_unlock(m_mloop);
        return false;
    }
    pa_threaded_mainloop_unlock(m_mloop);
    return true;
}

bool TSoundP::wait_operation(pa_operation *_op) noexcept
{
    using namespace std::chrono;
    auto start = steady_clock::now();
    decltype(duration_cast<milliseconds>(start - start).count()) dur;
    do
    {
        int operres = pa_operation_get_state(_op);
        if (operres == PA_OPERATION_DONE)
            return true;
        if (operres == PA_OPERATION_CANCELLED)
            return false;
        auto end = steady_clock::now();
        dur = duration_cast<std::chrono::milliseconds>(end - start).count();
        pa_threaded_mainloop_wait(m_mloop); // unknown timeout
    }while(dur < PULSE_OPERATION_TIMEOUT);
    return false;
}

bool TSoundP::wait_connection() noexcept
{
    using namespace std::chrono;
    auto start = steady_clock::now();
    decltype(duration_cast<milliseconds>(start - start).count()) dur;
    do
    {
        auto state = pa_context_get_state(m_ctx);
        if (state == PA_CONTEXT_READY)
            return true;
        if (state == PA_CONTEXT_FAILED)
            return false;
        auto end = steady_clock::now();
        dur = duration_cast<std::chrono::milliseconds>(end - start).count();
        pa_threaded_mainloop_wait(m_mloop); // unknown timeout
    }while(dur < PULSE_OPERATION_TIMEOUT);
    return false;
}

bool TSoundP::wait_stream_connection() noexcept
{
    using namespace std::chrono;
    auto start = steady_clock::now();
    decltype(duration_cast<milliseconds>(start - start).count()) dur;
    do
    {
        auto state = pa_stream_get_state(m_stm);
        if (state == PA_STREAM_READY)
            return true;
        if (state == PA_STREAM_FAILED)
            return false;
        auto end = steady_clock::now();
        dur = duration_cast<std::chrono::milliseconds>(end - start).count();
        pa_threaded_mainloop_wait(m_mloop); // unknown timeout
    }while(dur < PULSE_OPERATION_TIMEOUT);
    return false;
}

bool TSoundP::set_async_loop_timer() noexcept
{
    m_data = TSmartPtr<struct async_private_data>(new(std::nothrow) async_private_data);
    m_data->samples = const_cast<uint8_t*>(m_megabuffer.get());
    // non interleaved mode is goig to be done in futute
    //m_data->areas = m_areas;
    m_data->operctx = &m_operctx;
    m_data->phase = 0;
    m_data->nperiods = m_nbuffers;
    // m_data->period_size == m_buffersize;
    m_data->period_size = m_nbuffers*m_buffersize/m_data->nperiods;
    m_data->frame_size = m_framesize;
    m_data->isdrain = false;
    m_data->watchdog = m_data->nperiods*2;
    m_psev = TSmartPtr<struct sigevent>(new(std::nothrow) sigevent);
    memset(const_cast<struct sigevent*>(m_psev.get()), 0, sizeof(sigevent));
    m_psev->sigev_notify = SIGEV_THREAD;
    m_psev->sigev_notify_function = &timer_proc;
    m_psev->sigev_value.sival_ptr = const_cast<struct async_private_data*>(m_data.get());

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
    if (m_buffer_time < m_data->operctx->sgs.min_soundtime_period)
    {
        showerrmsg("pulse: timer period is too small");
        return false;
    }
    if (m_buffer_time > m_data->operctx->sgs.max_soundtime_period)
    {
        showerrmsg("pulse: timer period is too large");
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


bool TSoundP::write_pcm(const unsigned char *_data, size_t _samples)
{
    if (m_isError)
        return false;
    assert(_data);
    std::size_t nframes = _samples;
    if (nframes > m_buffersize)
    {
        elmsgr << "pulse: error: size of audio packet (" << nframes
              << " frames ) exceeds buffer size" << mend;
        m_isError = true;
        return false;
    }
    if (m_data)
    {
        if (m_data->isdrain)
        {
            m_bufferIDX = 0;
        }
        m_data->watchdog = m_nbuffers*2;
        auto buffer = const_cast<uint8_t*>(m_megabuffer.get()) + m_framesize*m_buffersize*m_bufferIDX;
        if (_samples < m_buffersize)
        {
            memset(buffer, 0, m_framesize*m_buffersize);
        }
        memcpy(buffer, _data, _samples*m_framesize);
        m_bufferIDX = (m_bufferIDX + 1)%m_nbuffers;
        if (m_data->isdrain)
        {
            m_data->isdrain = false;
            m_data->phase = 0;
        }
    }
    return true;
}

