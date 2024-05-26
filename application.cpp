#include "application.h"
#include "json_parser/simdjson.h"

TApplication::TApplication(int argc, char *argv[]):
    m_volume(0), m_mute(false), m_is_action(false)
{
    const auto audio_max_sample_rate = 192000;
    const auto audio_min_sample_rate = 20000;
    const auto audio_min_channels = 1;
    const auto audio_max_channels = 8;
    m_isError = true;
    if (argc < 2)
    {
        showmsg("usage test_ffmpeg6 filename");
        return;
    }
    // Version library check
    if (LIBAVFORMAT_VERSION_MAJOR != 59 || LIBAVFORMAT_VERSION_MINOR != 27 ||
       LIBAVFORMAT_VERSION_MICRO != 100 )
    {
       showerrmsg("Version of ffmpeg is incorrect. Behaviour may be undefined !");
    }
    if (GLFW_VERSION_MAJOR != 3 || GLFW_VERSION_MINOR != 3 ||
        GLFW_VERSION_REVISION != 8 )
    {
       showerrmsg("Version of glfw is incorrect. Behaviour may be undefined !");
    }
    if (PA_MAJOR != 13 || PA_MINOR != 99 || PA_MICRO != 0 )
    {
       showerrmsg("Version of pulseaudio is incorrect. Behaviour may be undefined !");
    }
    if (SND_LIB_MAJOR != 1 || SND_LIB_MINOR != 2 || SND_LIB_SUBMINOR != 2 )
    {
       showerrmsg("Version of alsa is incorrect. Behaviour may be undefined !");
    }
    if (GL_MAJOR_VERSION != 0x821B || GL_MINOR_VERSION != 0x821C ||
        GL_SHADING_LANGUAGE_VERSION != 0x8B8C )
    {
       showerrmsg("Version of opengl is incorrect. Behaviour may be undefined !");
    }
    m_settings.reset(new(std::nothrow)Tsettings);
    if (!m_settings || m_settings->is_error())
    {
        showerrmsg("Couldn't open settings file");
        return;
    }
    // Initialization
    int screen_width;
    int screen_height;
    Twindow::getScreenResolution(screen_width, screen_height);
    if (screen_width < MIN_SCREEN_WIDTH || screen_height < MIN_SCREEN_HEIGHT)
    {
        showerrmsg("Screen resolution is too small");
        return;
    }
    char* cfn = argv[1];
    const char* appname = "Simple VideoPlayer | ";
    m_reader.reset(new(std::nothrow)TVideo_reader(cfn, m_settings) );
    if (!m_reader || m_reader->is_error())
    {
        showerrmsg("Couldn't open or parse video file");
        return;
    }

    m_mctx.samplerate = m_reader->get_sample_rate();
    m_mctx.naudiochannels = m_reader->get_number_chanels();
    m_mctx.width = m_reader->get_width();
    m_mctx.height = m_reader->get_height();
    m_mctx.naudioframes = m_reader->get_max_samples();
    lmsgr << "input parameter: samplerate:" << m_mctx.samplerate << " Hz" << mend;
    lmsgr << "input parameter: number of audio channels:" << m_mctx.naudiochannels << mend;
    lmsgr << "input parameter: number of audio frames:" << m_mctx.naudioframes << mend;
    if (m_mctx.samplerate < audio_min_sample_rate)
    {
        showerrmsg("Sample rate is too small");
        return;
    }
    if (m_mctx.samplerate > audio_max_sample_rate)
    {
        showerrmsg("Sample rate is too big");
        return;
    }
    if (m_mctx.naudiochannels < audio_min_channels)
    {
        showerrmsg("Number of audio channels is too small");
        return;
    }
    if (m_mctx.naudiochannels > audio_max_channels)
    {
        showerrmsg("Number of audio channels is too big");
        return;
    }
    unsigned output_nchanels = (m_mctx.naudiochannels > 1)?2:1;
    int fps = m_reader->get_framerate();
    fmsgr << appname << " fps " << fps;
    const char* title = fmsgr.getString();
    m_soundp.reset( new(std::nothrow)TSound(m_mctx.samplerate,
                                       output_nchanels,
                                       m_mctx.naudioframes,
                                       m_settings));
    if (!m_soundp || m_soundp->isError())
    {
        showerrmsg("Couldn't init audio device");
        //return -1;
    }
    else
    {
        showmsg("pulse: initialization successfully finished");
    }
    m_volume = m_soundp->getVolume();
    m_window.reset( new(std::nothrow)Twindow(title, m_reader.get(), m_soundp.get(), m_mctx));
    if (!m_window || !m_window->isInit())
    {
        showerrmsg("Couldn't init window");
        return;
    }
    showmsg("start of frame decoding loop");
    pthread_t thId = pthread_self();
    pthread_attr_t thAttr;
    int policy = 0;
    int max_prio_for_policy = 0;
    pthread_attr_init(&thAttr);
    pthread_attr_getschedpolicy(&thAttr, &policy);
    max_prio_for_policy = sched_get_priority_max(policy);
    pthread_setschedprio(thId, max_prio_for_policy);
    pthread_attr_destroy(&thAttr);
    m_isError = false;
}

TApplication::~TApplication()
{ }

void TApplication::process_events()
{
    assert(m_reader);
    assert(m_soundp);
    assert(m_window);
    m_event = m_window->extractEvent(m_eventParams[0], m_eventParams[1],
                                     m_eventParams[2], m_eventParams[3]);
    switch(m_event)
    {
    case eNoEvent:
        {
            break;
        }
    case eExit:
        {
            m_window->closeWindow();
            m_event = eNoEvent;
            break;
        }
    case ePause:
        {
            m_reader->pause_video();
            m_event = eNoEvent;
            break;
        }
    case eResume:
        {
            m_reader->resume_video();
            m_event = eNoEvent;
            break;
        }
    case eFullScreen:
        {
            m_event = eNoEvent;
            break;
        }
    case eUnFullScreen:
        {
            m_event = eNoEvent;
            break;
        }
    case eRestart:
        {
            m_soundp->do_drain();
            m_reader->restart_video();
            m_event = eNoEvent;
            break;
        }
    case eAddVolume:
        {
            if (m_volume < MAX_SOUND_VOLUME)
            {
                m_volume += 10;
                m_volume = (m_volume > MAX_SOUND_VOLUME)?MAX_SOUND_VOLUME:m_volume;
                m_soundp->SetMasterVolume(m_volume);
            }
            m_event = eNoEvent;
            break;
        }
    case eScreenShot:
        {
            m_reader->save_rgb_frame();
            m_event = eNoEvent;
            break;
        }
    case eCroppedShot:
        {
            float scale = m_window->getScale();
            std::size_t cropped_width  = (m_eventParams[2].int_ - m_eventParams[0].int_)/scale;
            std::size_t cropped_height  = (m_eventParams[3].int_ - m_eventParams[1].int_)/scale;
            std::size_t size = cropped_width*cropped_height*3;
            TSmartPtr<uint8_t> buffer = TSmartPtr<uint8_t>(size);
            m_reader->getCroppedImage(buffer.get(),
                                      m_eventParams[0].int_,
                                      m_eventParams[1].int_,
                                      m_eventParams[2].int_,
                                      m_eventParams[3].int_);
            m_event = eNoEvent;
            break;
        }
    case eMute:
        {
            if (m_mute)
            {
                m_soundp->SetMasterVolume(m_volume);
                m_mute = false;
            }
            else
            {
                m_soundp->SetMasterVolume(10);
                m_mute = true;
            }
            m_event = eNoEvent;
            break;
        }
    case eDecVolume:
        {
            if (m_volume > 0)
            {
                m_volume -= 10;
                m_volume = (m_volume < 0)?0:m_volume;
                m_soundp->SetMasterVolume(m_volume);
            }
            m_event = eNoEvent;
            break;
        }
    case eBackStep:
        {
            if (m_reader->isPaused())
            {
                m_is_action = m_reader->backstep_video();
            }
            m_event = eNoEvent;
            break;
        }
    case eNextStep:
        {
            if (m_reader->isPaused())
            {
                m_is_action = m_reader->skip_forward(0,1);
            }
            m_event = eNoEvent;
            break;
        }
    }
}

bool TApplication::update(uint8_t *&_data_out, size_t &_width, size_t &_height, size_t &_nsamples) noexcept
{
    if (m_is_action)
    {
        m_is_action = false;
        return m_reader->update(_data_out, _width, _height, _nsamples);
    }
    return false;
}

int TApplication::exec() noexcept
{
    if (m_isError)
        return -1;
    process_events();
    m_reader->reset_timer(true);
    m_reader->do_delay(0.5);
    m_reader->do_delay(0.5);
    m_reader->do_delay(0.5);
    showmsg("--start--");

/*
    while(1)
    {
        window.process_events();
        if (window.shouldClose())
            break;
        window.show_image(nullptr, 0, 0);
    }

    return 0;
*/
    bool succRead = true;
    uint8_t * mdata = nullptr;
    size_t frame_width = 0;
    size_t frame_height = 0;
    size_t frame_nsamples = 0;
    while (succRead)
    {
        if (m_reader->isPaused())
        {
            m_window->process_events();
            process_events();
            if (m_window->shouldClose())
                break;
            if (update(mdata, frame_width, frame_height, frame_nsamples))
            {
                if ((m_reader->is_video_frame()))
                {
                    if (frame_width > m_mctx.width || frame_height > m_mctx.height)
                    {
                        showerrmsg("Frame parameters don't match input parameters");
                        break;
                    }
                    if (m_window->show_image(mdata, frame_width, frame_height) == -1)
                    {
                        break;
                    }
                }
                else
                {
                    showerrmsg("Unknown error");
                    break;
                }
            }
            else
            {
                if (m_window->show_last_image() == -1)
                {
                    break;
                }
                if (m_reader->is_error())
                    break;
            }
            continue;
        }
        else
        {
            if (!m_reader->is_error())
                succRead = m_reader->read_frame(mdata, frame_width, frame_height, frame_nsamples);
            else
                break;
        }
        if (m_window->shouldClose())
        {
            lmsgr << "found quit event" << mend;
            break;
        }
        if ((succRead) && (m_reader->is_video_frame()))
        {
            if (frame_width > m_mctx.width || frame_height > m_mctx.height)
            {
                showerrmsg("Current frame size doesn't match declared frame size in container");
                break;
            }
            if (m_window->show_image(mdata, frame_width, frame_height) == -1)
            {
                break;
            }
        }
        else if ((succRead) && (m_reader->is_audio_frame()))
        {
            if (frame_nsamples != m_mctx.naudioframes)
            {
                showerrmsg("Parameters of current audio frame don't match declared audio parameters");
                break;
            }
            if (!m_soundp->isError())
            {
                if (!m_window->isFlag())
                {
                    if (!m_soundp->write_pcm(mdata, frame_nsamples))
                    {
                        showerrmsg("Bad call of soundp.write_pcm");
                        break;
                    }
                }
            }
        }
        else
        {
            if (!succRead)
            {
                elmsgr << "next packet is not found or is invalid" << mend;
                break;
            }
            m_window->process_events();
            continue;
        }
        m_window->process_events();
        process_events();
    }
    if (m_reader->is_error())
    {
        elmsgr << "Critical error is occured durring the playback" << mend;
    }
    m_soundp->do_drain();
    showmsg("--end--");
    return EXIT_SUCCESS;
}
