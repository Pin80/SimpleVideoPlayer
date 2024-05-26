bool TVideo_reader::read_frame(uint8_t* & _data_out,
                               size_t &_width,
                               size_t &_height,
                               size_t &_nsamples)
{
    if (!m_isinit)
        return false;
    int response = -1;
    m_found_video_frame = false;
    m_found_audio_frame = false;
    int av_read_frame_code = 0;
    do
    {
        if (m_do_decode)
        {
            av_read_frame_code = av_read_frame(m_av_format_ctx, m_av_packet);
            m_do_decode = false;
            if (av_read_frame_code < 0)
            {
                break;
            }

            if (m_av_packet->stream_index != m_video_stream_index)
            {
                if (m_av_packet->stream_index == m_audio_stream_index)
                {
                    if (m_av_codec_ctx_audio)
                    {
                        response = avcodec_send_packet(m_av_codec_ctx_audio, m_av_packet);
                        if (response < 0)
                        {
                            printf("Failed to decode packet \n");
                            break;
                        }
                    }
                }
            }
            else
            {
                response = avcodec_send_packet(m_av_codec_ctx_video, m_av_packet);
                if (response < 0)
                {
                    printf("Failed to decode packet\n");
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
                    response = avcodec_receive_frame(m_av_codec_ctx_audio, m_av_frame_audio);
                    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
                    {
                        printf("Last audio frame format %s size %d\n",
                               caformats[m_av_frame_audio->format + 1], m_av_frame_audio->linesize[0]);
                        m_do_decode = true;
                        av_packet_unref(m_av_packet);
                        m_found_video_frame = false;
                        m_found_audio_frame = false;
                        continue; // no process events for speed up
                    }
                    else if (response < 0)
                    {
                        printf("Failed to receive audio frame\n");
                        break;
                    }
#ifdef DEBUG_MODE
                    printf("read_frame: got audio frame format %s size %d \n",
                           caformats[m_av_frame_audio->format + 1],
                           m_av_frame_audio->linesize[0]);
#endif
                    m_found_audio_frame = true;
                }
            }
            else
            {
                m_do_decode = false;
                av_packet_unref(m_av_packet);
                m_found_video_frame = false;
                m_found_audio_frame = false;
                printf("read_frame: unknown stream type \n");
                return false;
            }
        }
        else //(m_av_packet->stream_index == m_video_stream_index)
        {
            response = avcodec_receive_frame(m_av_codec_ctx_video, m_av_frame_video);
            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
            {
                printf("read_frame: Last video frame format %d\ size %d \n",
                       m_av_frame_video->format, m_av_frame_video->linesize);
                m_do_decode = true;
                av_packet_unref(m_av_packet);
                m_found_video_frame = false;
                m_found_audio_frame = false;
                continue;
            }
            else if (response < 0)
            {
                printf("Failed to receive audio frame\n");
                break;
            }
#ifdef DEBUG_MODE
            printf("read_frame: got video frame format %d size %d \n",
                   m_av_frame_video->format,
                   m_av_frame_video->linesize);
#endif
            m_found_video_frame = true;
        }
    } while (m_do_decode);
    if (av_read_frame_code == AVERROR_EOF)
    {
        m_do_decode = true;
        av_packet_unref(m_av_packet);
        m_isInputEnd = true;
        return true;
    }
    else
    {
        if (m_do_decode)
        {
            av_packet_unref(m_av_packet);
            return true;
        }
        else
        {
            if (response < 0)
            {
                    av_packet_unref(m_av_packet);
                    return false;
            }
        }
        m_isInputEnd = false;
    }

    if (m_found_video_frame)
    {
        return convert_video(_data_out, _width, _height);
    }
    if (m_found_audio_frame)
    {
        return true;
    }
    return true;
}
