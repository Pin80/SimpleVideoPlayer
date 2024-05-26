#include "video_reader.h"

const unsigned cnaformats = 15;
const char* caformats[] = {
		  "AV_SAMPLE_FMT_NONE", // -1
		  "AV_SAMPLE_FMT_U8",  // 0
		  "AV_SAMPLE_FMT_S16", // 1
		  "AV_SAMPLE_FMT_S32", // 2
		  "AV_SAMPLE_FMT_FLT", // 3
		  "AV_SAMPLE_FMT_DBL", // 4
		  "AV_SAMPLE_FMT_U8P", // 5
		  "AV_SAMPLE_FMT_S16P", // 6
		  "AV_SAMPLE_FMT_S32P",  // 7
		  "AV_SAMPLE_FMT_FLTP", // 8
		  "AV_SAMPLE_FMT_DBLP", // 9
		  "AV_SAMPLE_FMT_S64", // 10
		  "AV_SAMPLE_FMT_S64P", // 11
		  "AV_SAMPLE_FMT_NB", // 12
		  "UNKNOWN"}; // 13


void TVideo_reader::save_rgb_frame() noexcept
{
    FILE* f = nullptr;
    fmsgr << SCREENSHOT_FILE_NAME << m_av_codec_ctx_video->frame_number << ".ppm" << mend;
    f = fopen(fmsgr.getString(), "wb");
    fmsgr << "P6" << nl << m_frame_width << " " << m_frame_height << nl << 255 << mend;
    const char* str = fmsgr.getString();
    std::size_t lng = strlen(str);
    fwrite(str, lng, 1, f );
    for (int i = 0; i < m_frame_height; i++)
	{
        void* ptr = const_cast<uint8_t*>(m_videodata.get()) + 3*i * m_frame_width;
        fwrite(ptr, 1, m_frame_width*3, f);
	}
	fclose(f);
}

const uint8_t* TVideo_reader::getLastVideoData() const noexcept
{
    return m_videodata.get();
}

bool TVideo_reader::getCroppedImage(const uint8_t* _buffer, int _left, int _top, int _right, int _bottom)
{
    FILE* f = nullptr;
    fmsgr << SCREENSHOT_FILE_NAME << "_cropped"
          << m_av_codec_ctx_video->frame_number << ".ppm" << mend;
    f = fopen(fmsgr.getString(), "wb");
    int tmp;
    if (_left > _right)
    {
        tmp = _right;
        _right = _left;
        _left = tmp;
    }
    if (_top > _bottom)
    {
        tmp = _bottom;
        _bottom = _top;
        _top = tmp;
    }
    _left = (_left < 0)?0:_left;
    _right = (_right < 0)?0:_right;
    _top = (_top < 0)?0:_top;
    _bottom = (_bottom < 0)?0:_bottom;
    _left = (_left > m_frame_width)?m_frame_width:_left;
    _right = (_right > m_frame_width)?m_frame_width:_right;
    _top = (_top > m_frame_height)?m_frame_height:_top;
    _bottom = (_bottom > m_frame_height)?m_frame_height:_bottom;
    fmsgr << "P6" << nl << (_right - _left) << " " << (_bottom - _top) << nl << 255 << mend;
    const char* str = fmsgr.getString();
    std::size_t lng = strlen(str);
    fwrite(str, lng, 1, f );
    for (int i = _top; i < _bottom; i++)
    {
        void* ptr = const_cast<uint8_t*>(m_videodata.get()) + 3*(i*m_frame_width + _left);
        fwrite(ptr, 1, (_right - _left)*3, f);
    }
    fclose(f);
    return true;
}

TVideo_reader::TVideo_reader(const char* _filename,
                             const TSmartPtr<Tsettings> &_sgs) noexcept
{
    m_isError = true;
    if (!_sgs)
        return;
	m_prevaudiosize = 0;
	m_prevvideosize = 0;
	m_av_format_ctx = avformat_alloc_context();
	if (!m_av_format_ctx)
	{
        showerrmsg("Couldn't be created AVFormatContext");
		return;
	}
	if (avformat_open_input(&m_av_format_ctx, _filename, NULL, NULL) != 0)
	{
        showerrmsg("Couldn't be opened video file");
		return;
	}
    lmsgr << "Format " << m_av_format_ctx->iformat->name
          << ", duration " << (long unsigned)m_av_format_ctx->duration << " us" << mend;
	if (avformat_find_stream_info(m_av_format_ctx, NULL) < 0)
	{
        showerrmsg("Cannot find stream information");
		return;
	}
    if (!_sgs->containValue("video_formats", m_av_format_ctx->iformat->name))
    {
        showerrmsg("Format of video file is unsupported");
        return;
    }
	// select the video stream
	m_video_stream_index = av_find_best_stream(m_av_format_ctx,
											   AVMEDIA_TYPE_VIDEO,
											   -1,
											   -1,
											   &m_av_codec_video,
											   0);
	if ((m_video_stream_index < 0) || (!m_av_codec_video))
	{
        showerrmsg("Cannot find a video stream in the input file or unknown video codec");
		m_video_stream_index = -1;
	}
	AVCodecParameters * av_codec_params_video = nullptr;
	if (m_video_stream_index != -1)
	{
		av_codec_params_video = m_av_format_ctx->streams[m_video_stream_index]->codecpar;
		if (!av_codec_params_video)
		{
            showerrmsg("Couldn't find codec parameters");
			m_video_stream_index = -1;
		}
		else
		{
			m_timebase_video = m_av_format_ctx->streams[m_video_stream_index]->time_base;
			if (m_timebase_video.den == 0)
			{
                showerrmsg("Zero timebase for video stream");
				m_video_stream_index = -1;
			}
		}
	}

	m_audio_stream_index = av_find_best_stream(m_av_format_ctx,
											   AVMEDIA_TYPE_AUDIO,
											   -1,
											   -1,
											   &m_av_codec_audio,
											   0);
	if ((m_audio_stream_index < 0) || (!m_av_codec_audio))
	{
		m_audio_stream_index = -1;
        showerrmsg("Cannot find a audio stream in the input file or unknow audio codec");
	}
	AVCodecParameters * av_codec_params_audio = nullptr;
	if (m_audio_stream_index != -1)
	{
		m_timebase_audio = m_av_format_ctx->streams[m_audio_stream_index]->time_base;
		av_codec_params_audio = m_av_format_ctx->streams[m_audio_stream_index]->codecpar;
		if (!av_codec_params_audio)
		{
			m_audio_stream_index = -1;
            showerrmsg("Couldn't find codec parameters");
		}
	}
	if (m_video_stream_index != -1)
	{
		m_av_codec_ctx_video = avcodec_alloc_context3(m_av_codec_video);
		if (!m_av_codec_ctx_video)
		{
            showerrmsg("Couldn't create AVCodecContext");
			m_video_stream_index = -1;
		}
	}
	if (m_audio_stream_index != -1)
	{
		int sample_fmt = av_codec_params_audio->format;
		sample_fmt = (sample_fmt < 0)?0:sample_fmt;
		sample_fmt = (sample_fmt > cnaformats - 3)?cnaformats -3:sample_fmt;
        lmsgr << "Sample format:" << caformats[sample_fmt + 1] << mend;
		if ((sample_fmt == 0) || (sample_fmt == cnaformats - 3))
			m_audio_stream_index = -1;
        if (!_sgs->containValue("audio_sample_fmt", caformats[sample_fmt + 1]))
        {
            showerrmsg("Audio sample format is unsupported");
            return;
        }
	}
	if (m_audio_stream_index != -1)
	{
		m_av_codec_ctx_audio = avcodec_alloc_context3(m_av_codec_audio);
		if (!m_av_codec_ctx_audio)
		{
			m_audio_stream_index = -1;
            showerrmsg("Couldn't create AVCodecContext");
		}
	}
	if (m_video_stream_index != -1)
	{
		if (avcodec_parameters_to_context(m_av_codec_ctx_video, av_codec_params_video) < 0)
		{
            showerrmsg("Couldn't initialize AVCodecContext");
			m_video_stream_index = -1;
		}
	}
	if (m_video_stream_index != -1)
	{
        lmsgr << "Video Codec:" << m_av_codec_video->long_name
             << " ID:" << (long int)m_av_codec_video->id
             << " bit_rate:" << av_codec_params_video->bit_rate
             << " Pix format:" << (int)m_av_codec_ctx_video->pix_fmt
             << mend;
        if (!_sgs->containValue("video_codecs", m_av_codec_video->long_name))
        {
            showerrmsg("Video codec is unsupported");
            m_video_stream_index = -1;
            return;
        }
	}
	if (m_audio_stream_index != -1)
	{
		if (avcodec_parameters_to_context(m_av_codec_ctx_audio, av_codec_params_audio) < 0)
		{
			m_audio_stream_index = -1;
            showerrmsg("Couldn't initialize AVCodecContext");
		}
	}
	if (m_audio_stream_index != -1)
	{
		m_samplerate = m_av_codec_ctx_audio->sample_rate;
		m_nbchanels = m_av_codec_ctx_audio->channels;
		m_sampleformat = m_av_codec_ctx_audio->sample_fmt;
		m_framesamples = m_av_codec_ctx_audio->frame_size;
	}
	if (m_video_stream_index != -1)
	{
		m_frame_width = m_av_codec_ctx_video->width;
		m_frame_height = m_av_codec_ctx_video->height;
		m_framerate = m_av_format_ctx->streams[m_video_stream_index]->r_frame_rate;
		if ((m_frame_width < 0) || (m_frame_height < 0) )
		{
            showerrmsg("Illegal width, height, framesample");
			m_video_stream_index = -1;
		}
        if ( ((int)m_av_codec_ctx_video->pix_fmt <= AV_PIX_FMT_NONE) ||
             ((int)m_av_codec_ctx_video->pix_fmt >= AV_PIX_FMT_NB))
        {
            showerrmsg("Unreconized pixel format in video stream");
            m_video_stream_index = -1;
        }

	}
	else
	{
		m_frame_width = 320;
		m_frame_height = 240;
	}
	if (m_audio_stream_index != -1)
	{
		if ((m_nbchanels < 0) || (m_framesamples < 0))
		{
            showerrmsg("Illegal nchanels");
			m_audio_stream_index = -1;
		}
	}
	if (m_audio_stream_index != -1)
	{
		m_swrContext = swr_alloc();
		av_opt_set_int(m_swrContext, "in_channel_layout", (int64_t) m_av_codec_ctx_audio->channel_layout, 0);
        av_opt_set_int(m_swrContext, "out_channel_layout", AV_CH_LAYOUT_STEREO,  0);
		av_opt_set_int(m_swrContext, "in_sample_rate", m_av_codec_ctx_audio->sample_rate, 0);
		av_opt_set_int(m_swrContext, "out_sample_rate", m_av_codec_ctx_audio->sample_rate, 0);

		av_opt_set_sample_fmt(m_swrContext, "in_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
		av_opt_set_sample_fmt(m_swrContext, "out_sample_fmt", AV_SAMPLE_FMT_S16,  0);
		if (swr_init(m_swrContext) < 0)
		{
			m_audio_stream_index = -1;
            showerrmsg("Couldn't init swr context");
		}
        m_filter = nullptr;//av_bsf_get_by_name("mpeg4_unpack_bframes");
		if (m_filter == nullptr)
            elmsgr << "bsf not found" << mend;
		else
		{
            showmsg("bsf found");
			int ret = av_bsf_alloc(m_filter, &m_filterContext);
			if (ret < 0)
			{
                showmsg("bsf context not found");
			}
			else
			{
                showmsg("bsf context found");
			}
			if (ret >= 0)
			{
				ret = avcodec_parameters_copy(m_filterContext->par_in, av_codec_params_video);
				if (ret < 0)
				{
                    showmsg("no copy params for bsf context");
				}
				else
				{
                    showmsg("copy params for bsf context");
					m_filterContext->time_base_in = m_timebase_video;
				}
			}
			if (ret >= 0)
			{
				ret = av_bsf_init(m_filterContext);
				if (ret < 0)
				{
                    showerrmsg("bsf not init");
				}
				else
				{
                    showmsg("bsf init");
					m_isBSF = true;
				}
			}
		}
	}
	if (m_video_stream_index != -1)
	{
		if (avcodec_open2(m_av_codec_ctx_video, m_av_codec_video, NULL) < 0)
		{
            showerrmsg("Couldn't open codec");
			m_video_stream_index = -1;
		}
	}
	if (m_audio_stream_index != -1)
	{
        lmsgr << "Audio Codec:" << m_av_codec_audio->long_name
             << " ID:" << (long unsigned int)m_av_codec_audio->id
             << " bit_rate:" << (long unsigned int)av_codec_params_audio->bit_rate << mend;
		if (avcodec_open2(m_av_codec_ctx_audio, m_av_codec_audio, NULL) < 0)
		{
			m_audio_stream_index = -1;
            showerrmsg("Couldn't open audio codec");
		}
	}
	if (m_video_stream_index != -1)
	{
		m_av_frame_video = av_frame_alloc();
		if (!m_av_frame_video)
		{
            showerrmsg("Couldn't allocate video AVFrame");
			m_video_stream_index = -1;
		}
	}
	if (m_audio_stream_index != -1)
	{
		m_av_frame_audio = av_frame_alloc();
		if (!m_av_frame_audio)
		{
			m_audio_stream_index = -1;
            showerrmsg("Couldn't allocate audio AVFrame");
		}
	}

	m_av_packet = av_packet_alloc();
	if (!m_av_packet)
	{
        showerrmsg("Couldn't allocate AVPacket");
		return;
	}
	size_t datasize = 0;
	if (m_video_stream_index != -1)
	{
		datasize = m_frame_width * m_frame_height * 3;
        m_videodata = TSmartPtr<uint8_t>(datasize);
		if (!m_videodata)
		{
            showerrmsg("Unable to allocate video data buffer");
			m_video_stream_index = -1;
		}
	}
	m_prevvideosize = datasize;
	if (m_audio_stream_index != -1)
	{
        datasize = m_framesamples * m_nbchanels * 2;
        m_audiodata = TSmartPtr<uint8_t>(datasize);
		if (!m_audiodata)
		{
			m_audio_stream_index = -1;
            showerrmsg("Unable to allocate audio data buffer");
		}
		m_prevaudiosize = datasize;
	}
    if ((m_video_stream_index != -1) || (m_audio_stream_index != -1))
        m_isError = false;
	else
		return;
	m_firstframe = true;
	m_do_decode = true;
	m_current_pts_video = 0;
}

bool TVideo_reader::read_frame(uint8_t* & _data_out,
							   size_t &_width,
							   size_t &_height,
                               size_t &_nsamples) noexcept
{
    if (m_isError)
		return false;
	m_found_video_frame = false;
	m_found_audio_frame = false;
    int packet_error_code = -1;
    int frame_error_code = -1;
	do
	{
		if (m_do_decode)
		{
            packet_error_code = av_read_frame(m_av_format_ctx, m_av_packet);
			m_do_decode = false;
            if (packet_error_code < 0)
			{
                m_isError = true;
				break;
			}
            m_isBSF = false;
			if (m_isBSF)
			{
                int ret = av_bsf_send_packet(m_filterContext, m_av_packet);
				if (ret < 0)
				{
					m_isBSF = false;
				}

				ret = av_bsf_receive_packet(m_filterContext, m_av_packet);
				if (ret == 0)
				{
                    showmsg("bsf good \n");
				}
				if (ret == AVERROR(EAGAIN))
				{
					// nothing to do
				}
				if (ret == AVERROR_EOF)
				{
					// nothing to do
				}
				if (ret < 0)
				{
					m_isBSF = false;
                    showmsg("bsf fail \n");
				}

			}
			if (m_av_packet->stream_index != m_video_stream_index)
			{
				if (m_av_packet->stream_index == m_audio_stream_index)
				{
					if (m_av_codec_ctx_audio)
					{
                        frame_error_code = avcodec_send_packet(m_av_codec_ctx_audio, m_av_packet);
                        if (frame_error_code < 0)
						{
                            showerrmsg("Failed to decode packet");
							break;
						}
					}
				}
			}
			else
			{
                frame_error_code = avcodec_send_packet(m_av_codec_ctx_video, m_av_packet);
                if (frame_error_code < 0)
				{
                    showerrmsg("Failed to decode packet");
					break;
				}
			}
		}
        if (m_av_packet->stream_index != m_video_stream_index)
        {
            if (m_av_packet->stream_index == m_audio_stream_index)
            {
                if (m_av_codec_ctx_audio)
                {
                    frame_error_code = avcodec_receive_frame(m_av_codec_ctx_audio, m_av_frame_audio);
                    if (frame_error_code == AVERROR(EAGAIN) || frame_error_code == AVERROR_EOF)
                    {
                        lmsgr << "Last audio frame format: " << caformats[m_av_frame_audio->format + 1]
                             << " size: " << (long int)m_av_frame_audio->linesize[0] << mend;
                        m_do_decode = true;
                        av_packet_unref(m_av_packet);
                        m_found_video_frame = false;
                        m_found_audio_frame = false;
                        continue; // no process events for speed up
                    }
                    else if (frame_error_code < 0)
                    {
                        showerrmsg("Failed to receive audio frame");
                        break;
                    }
#ifdef DEBUG_MODE
                    lmsgr << "read_frame: got audio frame format: " << caformats[m_av_frame_audio->format + 1]
                         << " size: " << (long int)m_av_frame_audio->linesize[0] << mend;
#endif
                    m_found_audio_frame = true;
                }
            }
            else // unknown steam type
            {
                m_do_decode = false;
                av_packet_unref(m_av_packet);
                m_found_video_frame = false;
                m_found_audio_frame = false;
                showerrmsg("read_frame: unknown stream type");
                return false;
            }
        }
        else //(m_av_packet->stream_index == m_video_stream_index)
        {
            frame_error_code = avcodec_receive_frame(m_av_codec_ctx_video, m_av_frame_video);
            if (frame_error_code== AVERROR(EAGAIN) || frame_error_code == AVERROR_EOF)
            {
                lmsgr << "read_frame: Last video frame."  << mend;
                m_do_decode = true;
                av_packet_unref(m_av_packet);
                m_found_video_frame = false;
                m_found_audio_frame = false;
                continue;
            }
            else if (frame_error_code < 0)
            {
                showerrmsg("Failed to receive audio frame");
                break;
            }
#ifdef DEBUG_MODE
            lmsgr << "read_frame: got video frame format: " << m_av_frame_video->format
                 << " size: " << m_av_frame_video->linesize[0] << mend;
#endif
            m_found_video_frame = true;
        }
    } while (m_do_decode);
    if (packet_error_code == AVERROR_EOF)
	{
		m_do_decode = true;
		av_packet_unref(m_av_packet);
		m_isInputEnd = true;
        msgr << "Found last packet" << mend;
		return true;
	}
	else
	{
        if (packet_error_code < 0)
        {
            av_packet_unref(m_av_packet);
            m_isError = true;
            return false;
        }
		if (m_do_decode)
		{
			av_packet_unref(m_av_packet);
			return true;
		}
		else
		{
            if (frame_error_code < 0)
			{
					av_packet_unref(m_av_packet);
					return false;
			}
		}
		m_isInputEnd = false;
	}

	if (m_found_video_frame)
	{
		m_video_nframes_decoded++;
		switch (m_syncmaster)
		{
			case eUndef:
					{
                        m_syncmaster = eVideoMaster;
						break;
					}
			case eAudioMaster:
					{
						if (!m_ispause_enabled)
						{
                            m_syncmaster = eVideoMaster;
						}
						break;
					}
			case eVideoMaster:
					{
                        //m_isSeeked = false;
						break;
					}
		}
        //m_syncmaster = eAudioMaster;
        if (m_syncmaster == eVideoMaster)
            showmsg("m_syncmaster eVideoMaster");
        else if (m_syncmaster == eAudioMaster)
            showmsg("m_syncmaster eAudioMaster");
		return convert_video(_data_out, _width, _height);
	}
	if (m_found_audio_frame)
	{
		m_audio_nframes_decoded++;
		switch (m_syncmaster)
		{
			case eUndef:
					{
						if (m_video_stream_index == -1)
						{
							m_syncmaster = eAudioMaster;
						}
						else
						{
							m_current_pts_audio = m_av_frame_audio->pts;
							auto v_pt_in_sec = rescale(m_current_pts_video,
													   m_timebase_video.num,
													   m_timebase_video.den);
							auto a_pt_in_sec = rescale(m_current_pts_audio,
													   m_timebase_audio.num,
													   m_timebase_audio.den);
							if (a_pt_in_sec - v_pt_in_sec > 3)
							{
								m_syncmaster = eAudioMaster;
							}
							else
							{
                                m_syncmaster = eVideoMaster;
                                //m_syncmaster = eAudioMaster;
							}
						}
						break;
					}
			case eVideoMaster:
					{
						if (!m_ispause_enabled)
						{
							m_current_pts_audio = m_av_frame_audio->pts;
							auto v_pt_in_sec = rescale(m_current_pts_video,
													   m_timebase_video.num,
													   m_timebase_video.den);
							auto a_pt_in_sec = rescale(m_current_pts_audio,
													   m_timebase_audio.num,
													   m_timebase_audio.den);
							if (a_pt_in_sec - v_pt_in_sec > 3)
							{
								m_syncmaster = eAudioMaster;
							}

						}
						break;
					}
			case eAudioMaster:
					{
						break;
					}
		}
        //m_syncmaster = eAudioMaster;
        if (m_syncmaster == eVideoMaster)
            showmsg("m_syncmaster eVideoMaster");
        else if (m_syncmaster == eAudioMaster)
        showmsg("m_syncmaster eAudioMaster");
        _nsamples = m_av_frame_audio->nb_samples;
        return convert_audio(_data_out);
        m_found_audio_frame = false;
        return true;
	}
	return true;
}

double TVideo_reader::rescale(int64_t a, int64_t b, int64_t c) noexcept
{
	return (double)(a *b)/(double)c;
}

bool TVideo_reader::is_error() const noexcept
{
    return m_isError;
}

bool TVideo_reader::restart_video() noexcept
{
    showmsg("reader: restart video -------------------------------------");
    if (m_isError)
        return false;
    auto valid_idx = -1;
    if (m_video_stream_index != -1 )
        valid_idx = m_video_stream_index;
    else if (m_audio_stream_index != -1 )
        valid_idx = m_audio_stream_index;
    else return false;
    m_firstframe = true;
    m_pausetime = 0;
    m_ispause_enabled = false;
    if (avio_seek(m_av_format_ctx->pb, 0, SEEK_SET) < 0)
    {
        showerrmsg("Unable to restart video (avio)");
        return false;
    }
    /*
    if (avformat_seek_file(m_av_format_ctx,
                       valid_idx,
                       0,
                       0,
                       m_av_format_ctx->streams[valid_idx]->duration,
                       0) < 0)
    {
        showerrmsg("Unable to restart video (seek)");
        return false;
    }
    */
    if (av_seek_frame(m_av_format_ctx,
                       valid_idx,
                       0,
                       AVSEEK_FLAG_BACKWARD) < 0)
    {
        showerrmsg("Unable to restart video (seek)");
        return false;
    }
    m_video_nframes_decoded = 0;
    m_audio_nframes_decoded = 0;
    m_current_pts_video = 0;
    m_current_pts_audio = 0;
    return true;
}

bool TVideo_reader::backstep_video() noexcept
{
    if (m_isError)
		return false;
	if (!m_ispause_enabled)
		return false;
    if (m_video_stream_index == -1)
    {
        return false;
    }
    lmsgr << "backstep_video: current: " <<  m_current_pts_video
         << " previous: " <<  m_previous_pts_video << mend;
	auto valid_idx = -1;
	if (m_syncmaster == eVideoMaster)
	{
        valid_idx = m_video_stream_index;
	}
	else if (m_syncmaster == eAudioMaster)
	{
        showerrmsg("video_reader error: m_syncmaster == eAudioMaster");
		return false;
		valid_idx = m_audio_stream_index;
	}
	else
	{
        showerrmsg("video_reader error: m_syncmaster == Unknown");
		return false;
	}
    lmsgr << "backstep_video: Framerate: " << m_framerate.num << m_framerate.den << mend;

	if (m_current_pts_video > 0)
	{
		if (av_seek_frame(m_av_format_ctx,
						  valid_idx,
                          m_current_pts_video -1,
                          AVSEEK_FLAG_BACKWARD ) < 0)
		{
            showerrmsg("video_reader error: Unable to backward video (seek)");
			return false;
		}
	}
    else
    {
        showerrmsg("video_reader: achieved start of video");
        return false;
    }

    //printf("Backward step %i %i \n", m_current_pts_video, seekPTS_target);
    //m_isSeeked = true;
	if (m_syncmaster == eVideoMaster)
		m_pausetime = rescale(m_current_pts_video, m_timebase_video.num, m_timebase_video.den);
	else if (m_syncmaster == eAudioMaster)
		m_pausetime = rescale(m_current_pts_audio, m_timebase_audio.num, m_timebase_audio.den);
    if (!skip_forward(m_previous_pts_video))
    {
        m_previous_pts_video = m_current_pts_video;
        return false;
    }
    lmsgr << "backstep_video: current pts: " << m_current_pts_video
         << " previous pts: " << m_previous_pts_video << mend;
    if (m_current_pts_video > 0)
    {
        if (av_seek_frame(m_av_format_ctx,
                          valid_idx,
                          m_current_pts_video -1,
                          AVSEEK_FLAG_BACKWARD ) < 0)
        {
            showerrmsg("video_reader error: Unable to backward video (seek)");
            return false;
        }
    }
    else
    {
        showmsg("video_reader: achieved start of video");
        return false;
    }
    if (!skip_forward(m_current_pts_video))
    {
        return false;
    }
    lmsgr << "backstep_video: current pts: " << m_current_pts_video
         << " previous pts: " << m_previous_pts_video << mend;
    lmsgr << "current pausetime: " <<  m_pausetime << mend;
    m_current_pts_video = m_previous_pts_video;
}

bool TVideo_reader::convert_video(uint8_t*&data_out, size_t &_width, size_t &_height) noexcept
{
    // const int pts_invalid = 0; // pts check is deleted
	size_t datasize = m_av_frame_video->width * m_av_frame_video->height * 3;
    //m_current_pts_video = m_av_frame_video->pts;
    //if (m_current_pts_video == pts_invalid)
    m_previous_pts_video = m_current_pts_video;
    m_current_pts_video = m_av_frame_video->pkt_dts;

	if (datasize != m_prevvideosize)
	{
        m_videodata = TSmartPtr<uint8_t>(datasize);
		if (!m_videodata)
		{
            showerrmsg("Unable to allocate video data buffer");
			return false;
		}
		m_prevvideosize = datasize;
	}
	SwsContext* sws_scaler_ctx = sws_getContext(m_av_frame_video->width,
												m_av_frame_video->height,
                                                m_av_codec_ctx_video->pix_fmt,
												m_av_frame_video->width,
												m_av_frame_video->height,
												AV_PIX_FMT_RGB24,
                                                SWS_FAST_BILINEAR,
												NULL, NULL, NULL);
	if (!sws_scaler_ctx)
	{
        showerrmsg("Couldn't initialize scaler");
		return false;
	}
    uint8_t* dest[4] = {unconst(m_videodata.get()), NULL, NULL, NULL};
	int dest_linesize[4] = {m_av_frame_video->width * 3, 0,0,0};
	sws_scale(sws_scaler_ctx,
			  m_av_frame_video->data,
			  m_av_frame_video->linesize,
			  0,
			  m_av_frame_video->height,
			  dest,
			  dest_linesize);
	sws_freeContext(sws_scaler_ctx);
	double pt_in_seconds = 0;
	double current_time = 0;
	double vdelay = 0;
	if (m_syncmaster == eVideoMaster)
	{
		if (m_firstframe)
		{
			m_firstframe = false;
			if (!reset_timer())
			{
				return false; // impossible
			}
		}
		else
		{
			pt_in_seconds = rescale(m_current_pts_video, m_timebase_video.num, m_timebase_video.den);
			current_time = get_time();
			if (pt_in_seconds > current_time)
			{
				vdelay = pt_in_seconds - current_time;
				(void)do_delay(vdelay);
			}
		}
	}
#ifdef DEBUG_MODE
    long unsigned a = (long)m_av_frame_video->pts;
    lmsgr << "convert video Frame:" << (int)av_get_picture_type_char(m_av_frame_video->pict_type)
         << " Frame#(" << m_av_codec_ctx_video->frame_number << ") pts: " << a
         << " dts: " << m_av_frame_video->pkt_dts
         << " key_frame: " << m_av_frame_video->key_frame
         << " delay: " << vdelay
         << " -------------- data size: " << datasize << mend;
	if (m_av_frame_video->key_frame)
		m_lastKeyFrame = m_av_codec_ctx_video->frame_number;
#endif
	_width = m_av_frame_video->width;
	_height = m_av_frame_video->height;
	data_out = dest[0];
	return true;
}

bool TVideo_reader::convert_audio(uint8_t*& _data_out) noexcept
{
	const int pts_invalid = 0;
	int result = -1;
	size_t datasize = m_av_frame_audio->nb_samples * 2 * 2;
    m_previous_pts_audio = m_current_pts_audio;
	m_current_pts_audio = m_av_frame_audio->pts;

	if (m_current_pts_audio == pts_invalid)
		m_current_pts_audio = m_av_frame_audio->pkt_dts;

	if (datasize != m_prevaudiosize)
	{
        m_audiodata = TSmartPtr<uint8_t>(datasize);
		if (!m_audiodata)
		{
            showerrmsg("Unable to allocate audio data buffer");
			return false;
		}
		m_prevaudiosize = datasize;
	}
    m_totaldatasize += datasize;
    _data_out = unconst(m_audiodata.get());
	//memcpy(data, m_av_frame_audio->data, datasize);
    uint8_t* usefull_audiodata = unconst(m_audiodata.get());
    uint8_t* out_bufs[8] = {usefull_audiodata, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    const uint8_t* inp_bufs[8] = {m_av_frame_audio->data[0],
								  m_av_frame_audio->data[1],
								  m_av_frame_audio->data[2],
								  m_av_frame_audio->data[3],
								  m_av_frame_audio->data[4],
                                  m_av_frame_audio->data[5],
                                  m_av_frame_audio->data[6],
                                  m_av_frame_audio->data[7]};
	result = swr_convert(m_swrContext,
						out_bufs,
						m_av_frame_audio->nb_samples,
						inp_bufs,
						m_av_frame_audio->nb_samples);
	if (result < 0)
	{
        char a[AV_ERROR_MAX_STRING_SIZE]{0};
        elmsgr << "Could not convert input samples: "
               << av_make_error_string(a, AV_ERROR_MAX_STRING_SIZE, result) << mend;
		return false;
	}

	double pt_in_seconds = 0;
	double current_time = 0;
	float adelay = 0;
    //if (m_syncmaster == eAudioMaster)
	{
		if (m_firstframe)
		{
			m_firstframe = false;
			if (!reset_timer())
			{
                elmsgr << "reset timer error" << mend;
				return false;
			}
		}
		else
		{
			pt_in_seconds = rescale(m_current_pts_audio, m_timebase_audio.num, m_timebase_audio.den);
			current_time = get_time();
			if (pt_in_seconds > current_time + m_min_audio_interframe_time)
			{
				adelay = (pt_in_seconds - current_time - m_min_audio_interframe_time)*0.1;
				if ((adelay < 0) || (adelay > 1.))
				{
                    elmsgr << "error delay is: " << adelay << mend;
					return false;
				}
				if (!do_delay((adelay)))
				{
					return false;
				}
				m_old_delay = adelay;
			}
			else
			{
				m_min_audio_interframe_time = current_time - m_old_current_time;
			}
			m_old_current_time = get_time();
		}
	}
    int byterate = (float)m_totaldatasize/(float)(current_time + 1e-4);
#ifdef DEBUG_MODE
    lmsgr << "convert_audio: audio Frame # " << m_av_codec_ctx_audio->frame_number
         << " pts: " << m_av_frame_audio->pts
         << " dts: " << m_av_frame_audio->pkt_dts
         << " delay: " << adelay
         << " data size: " << datasize
         << " current time: " << current_time
         << " stream time stamp: " << pt_in_seconds
         << " byterate: " << byterate
         << mend;
#endif
    return true;
}

bool TVideo_reader::update(uint8_t *&_data_out,
                           size_t &_width,
                           size_t &_height,
                           size_t &_nsamples) noexcept
{
    if (m_isError)
        return false;
    (void)_nsamples;
    if (m_found_video_frame)
        return convert_video(_data_out, _width, _height );
    else if (m_found_audio_frame)
        return convert_audio(_data_out);
    return false;
}

void TVideo_reader::do_dump(unsigned char *_buf, size_t _size) noexcept
{
	FILE* f = NULL;
	f = fopen("dump.bin", "w");
	auto result = fwrite((void *)_buf, _size , 1, f);
    if (result != 1)
        elmsgr << "TVideo_reader::do_dump error" << mend;
	fclose(f);
}


unsigned int TVideo_reader::get_sample_rate() const noexcept
{
    if (m_audio_stream_index == -1)
        return -1;
    else
        return m_samplerate;
}

unsigned int TVideo_reader::get_number_chanels() const noexcept
{
	if (m_audio_stream_index != -1)
		return m_nbchanels;
	else
		return -1;
}

unsigned int TVideo_reader::get_width() const noexcept
{
	return m_frame_width;
}

unsigned int TVideo_reader::get_height() const noexcept
{
	return m_frame_height;
}

size_t TVideo_reader::get_max_samples() const noexcept
{
    if (m_audio_stream_index == -1)
        return -1;
    else
        return m_framesamples;
}

int TVideo_reader::get_framerate() noexcept
{
	m_framerate.den = (m_framerate.den == 0)?1:m_framerate.den;
	return m_framerate.num/m_framerate.den;
}

bool TVideo_reader::isPaused() const noexcept
{
	return m_ispause_enabled;
}

bool TVideo_reader::isInputEnd() const noexcept
{
	return m_isInputEnd;
}

bool TVideo_reader::isPacketEnd() const noexcept
{
	return m_do_decode;
}

/*
bool TVideo_reader::isSeeked() const
{
	return m_isSeeked;
}
*/

bool TVideo_reader::reset_timer(bool _firstframe) noexcept
{
    if (m_isError)
		return false;
	//glfwSetTime(0.0); // alternative implementation
	m_StartTime = std::chrono::system_clock::now(); //noexcept;
	m_firstframe = _firstframe;
	return true;
}

double TVideo_reader::get_time() noexcept
{
	typedef std::chrono::microseconds usecs;
    if (m_isError)
		return false;
	//return glfwGetTime(); // alternative implementation
	auto endTime = std::chrono::system_clock::now();
	auto duration = endTime - m_StartTime;
	auto fcurr_time = std::chrono::duration_cast<usecs>(duration).count();
    return (double)fcurr_time/1000000.0 + m_pausetime;
}

bool TVideo_reader::skip_forward(int64_t _dts, int _ntimes) noexcept
{
    if (m_isError)
        return false;
    if (m_video_stream_index == -1)
    {
        return false;
    }
    bool fin = false;
    int64_t current_dts = 0;
    av_packet_unref(m_av_packet);
    m_do_decode = true;
    int packet_error_code = -1;
    int frame_error_code = -1;
    bool found_video_packet = false;
    do
    {
        found_video_packet = false;
        if (m_do_decode)
        {
            packet_error_code = av_read_frame(m_av_format_ctx, m_av_packet);
            m_do_decode = false;
            if (packet_error_code < 0)
            {
                m_isError = true;
                break;
            }
            if (m_av_packet->stream_index == m_audio_stream_index)
            {
                // Do nothing
            }
            else if (m_av_packet->stream_index == m_video_stream_index)
            {
                m_previous_pts_video = m_current_pts_video;
                m_current_pts_video = m_av_packet->dts;
                current_dts = m_current_pts_video;
                if ((current_dts >= _dts) && (_ntimes == 0))
                {
                    fin = true;
                }
                lmsgr << "skip_forward: send packet to decoder with dts: " << current_dts << mend;
                frame_error_code = avcodec_send_packet(m_av_codec_ctx_video, m_av_packet);
                if (frame_error_code < 0)
                {
                    showerrmsg("skip_forward error: Failed to decode packet");
                    break;
                }
                found_video_packet = true;
            }
        }
        if (found_video_packet)
        {
            lmsgr << "skip_forward: video current dts is: " << current_dts
                 << " expected is: " << _dts << mend;
        }
        m_found_video_frame = false;
        m_found_audio_frame = false;
        if (!found_video_packet)
        {
            m_do_decode = true;
            av_packet_unref(m_av_packet);
            m_found_video_frame = false;
            m_found_audio_frame = false;
        }
        else
        {
            frame_error_code = avcodec_receive_frame(m_av_codec_ctx_video, m_av_frame_video);
            if (frame_error_code == AVERROR(EAGAIN) || frame_error_code == AVERROR_EOF)
            {
                lmsgr << "skip_forward: Last video frame." << mend;
                m_do_decode = true;
                av_packet_unref(m_av_packet);
                m_found_video_frame = false;
                m_found_audio_frame = false;
                continue;
            }
            else if (frame_error_code < 0)
            {
                showerrmsg("skip_forward error: Failed to receive video frame");
                m_isError = true;
                break;
            }
            m_found_video_frame = true;
#ifdef DEBUG_MODE
            lmsgr << "skip_forward: got video frame format " << m_av_frame_video->format
                  << " size " << m_av_frame_video->linesize[0] << mend;
#endif
        }
        if ((_ntimes > 0) && (found_video_packet))
        {
            _ntimes--;
            fin = (_ntimes == 0);
        }
    } while (!fin);
    if ((current_dts != _dts) && (_ntimes == 0))
    {
        m_pausetime = rescale(m_current_pts_video, m_timebase_video.num, m_timebase_video.den);
        lmsgr << "skip_forward:  Seeking forward didnt find dts, current is: "
              << m_current_pts_video << " previous is:" << m_previous_pts_video << mend;
        return true;
    }
    else
    {
        m_pausetime = rescale(m_current_pts_video, m_timebase_video.num, m_timebase_video.den);
        lmsgr << "skip_forward:  Seeking forward find dts, current is:"
              << m_current_pts_video << " previous is:" << m_previous_pts_video << mend;
        return true;
    }
    return false;
}

bool TVideo_reader::do_delay(double _del) noexcept
{
    if (m_isError)
		return false;
	uint64_t del = _del*1000000;
    std::this_thread::sleep_for(std::chrono::microseconds(del));
	return true;
}

bool TVideo_reader::pause_video() noexcept
{
    if (m_isError)
		return false;
	if (m_ispause_enabled)
		return false;
	m_ispause_enabled = true;
	//m_pausetime = get_time() + 3*m_old_delay;
	if (m_syncmaster == eVideoMaster)
		m_pausetime = rescale(m_current_pts_video, m_timebase_video.num, m_timebase_video.den);
	else if (m_syncmaster == eAudioMaster)
		m_pausetime = rescale(m_current_pts_audio, m_timebase_audio.num, m_timebase_audio.den);
    lmsgr << "Pause time is: " << m_pausetime << " seconds" << mend;
	return true;
}

bool TVideo_reader::resume_video() noexcept
{
    if (m_isError)
		return false;
    showmsg("Resume\n");
	if (!m_ispause_enabled)
		return false;
	m_ispause_enabled = false;
	reset_timer(true);
	return true;
}

bool TVideo_reader::is_video_frame() const noexcept
{
	return m_found_video_frame;
}

bool TVideo_reader::is_audio_frame() const noexcept
{
	return m_found_audio_frame;
}

TVideo_reader::~TVideo_reader()
{
	if (m_av_packet)
	{
		av_packet_free(&m_av_packet);
	}
	if (m_av_frame_audio)
	{
		av_frame_free(&m_av_frame_audio);
	}
	if (m_av_frame_video)
	{
		av_frame_free(&m_av_frame_video);
	}
	if (m_av_codec_audio)
	{
		avcodec_close(m_av_codec_ctx_video);
	}
	if (m_av_codec_ctx_video)
	{
		avcodec_close(m_av_codec_ctx_video);
	}
	if (m_filterContext)
	{
		av_bsf_free(&m_filterContext);
	}
	if (m_swrContext)
	{
		swr_free(&m_swrContext);
	}
	if (m_av_codec_ctx_audio)
	{
		avcodec_free_context(&m_av_codec_ctx_audio);
	}
	if (m_av_codec_ctx_video)
	{
		avcodec_free_context(&m_av_codec_ctx_video);
	}
	if (m_av_format_ctx)
	{
		avformat_close_input(&m_av_format_ctx);
		avformat_free_context(m_av_format_ctx);
	}
}
