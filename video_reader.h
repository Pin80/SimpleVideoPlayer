#ifndef TVIDEO_READER_H
#define TVIDEO_READER_H
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include "common.h"
#include "settings.h"

extern "C"
{
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	#include <libswresample/swresample.h>
	#include <libavutil/opt.h>
	#include <inttypes.h>
	#include <libavutil/imgutils.h>
	#include <libavcodec/bsf.h>
    #include "libavfilter/avfilter.h"
    #include "libavfilter/buffersrc.h"
    #include "libavfilter/buffersink.h"
}

#include <GLFW/glfw3.h>

// One thread video and audio reader
class TVideo_reader : TNonCopyable
{
public:
    TVideo_reader(const char *_filename,
                  const TSmartPtr<Tsettings>& _sgs)  noexcept;
    virtual ~TVideo_reader();
    bool read_frame(uint8_t *&_data_out, size_t &_width, size_t &_height, size_t& _nsamples) noexcept;
    double rescale(int64_t a, int64_t b, int64_t c) noexcept;
    bool is_error() const noexcept;
    bool restart_video() noexcept;
    bool backstep_video() noexcept;
    bool pause_video() noexcept;
    bool resume_video() noexcept;
    bool is_video_frame() const noexcept;
    bool is_audio_frame() const noexcept;
    unsigned int get_sample_rate() const noexcept;
    unsigned int get_number_chanels() const noexcept;
    unsigned int get_width() const noexcept;
    unsigned int get_height() const noexcept;
    size_t get_max_samples() const noexcept;
    int get_framerate() noexcept;
    bool isPaused() const noexcept;
    bool isInputEnd() const noexcept;
    bool isPacketEnd() const noexcept;
    bool isSeeked() const noexcept;
    virtual bool do_delay(double _del) noexcept;
    virtual bool reset_timer(bool _firstframe = false) noexcept;
    bool convert_video(uint8_t* &data_out, size_t &_width, size_t &_height) noexcept;
    bool convert_audio(uint8_t* &_data_out) noexcept;
    bool update(uint8_t *&_data_out, size_t &_width, size_t &_height, size_t& _nsamples) noexcept;
    virtual bool skip_forward(int64_t _dts, int _ntimes = 0) noexcept;
    void save_rgb_frame() noexcept;
    const uint8_t* getLastVideoData() const noexcept;
    bool getCroppedImage(const uint8_t* _buffer, int _left, int _top, int _right, int _bottom);
protected:
    virtual double get_time() noexcept;
private:
	enum eSyncMaster {eUndef = -1, eVideoMaster, eAudioMaster};
	AVFormatContext* m_av_format_ctx = nullptr;
	int m_video_stream_index = -1;
	int m_audio_stream_index = -1;
	const AVCodec * m_av_codec_video = nullptr;
	const AVCodec * m_av_codec_audio = nullptr;
	AVCodecContext * m_av_codec_ctx_video = nullptr;
	AVCodecContext * m_av_codec_ctx_audio = nullptr;
	AVFrame* m_av_frame_video = nullptr;
	AVFrame* m_av_frame_audio = nullptr;
	AVPacket* m_av_packet = nullptr;
    TSmartPtr<uint8_t> m_videodata;
	std::size_t m_prevvideosize = 0;
    TSmartPtr<uint8_t> m_audiodata;
	std::size_t m_prevaudiosize = 0;
	AVRational m_timebase_video;
	AVRational m_timebase_audio;
	int64_t m_current_pts_video = 0;
	int64_t m_current_pts_audio = 0;
    int64_t m_previous_pts_video = 0;
    int64_t m_previous_pts_audio = 0;
	int m_frame_width;
	int m_frame_height;
    size_t m_framesamples;
	unsigned int m_samplerate;
	unsigned m_nbchanels;
	unsigned m_lastKeyFrame = 0;
	AVSampleFormat m_sampleformat;
    bool m_isError = false;
	bool m_firstframe;
	bool m_ispause_enabled = false;
	bool m_isInputEnd = false;
	bool m_found_video_frame = false;
	bool m_found_audio_frame = false;
    //bool m_isSeeked = false;
	SwrContext* m_swrContext = nullptr;
	double m_min_audio_interframe_time = 0;
	double m_old_current_time = 0;
	double m_old_delay = 0;
	double m_pausetime = 0;
	std::chrono::time_point<std::chrono::system_clock> m_StartTime;
	eSyncMaster m_syncmaster = eUndef;
	uint64_t m_video_nframes_decoded = 0;
	uint64_t m_audio_nframes_decoded = 0;
	AVRational m_framerate;
	const AVBitStreamFilter* m_filter = nullptr;
	AVBSFContext * m_filterContext = nullptr;
	bool m_isBSF = false;
	bool m_do_decode = false;
    uint64_t m_totaldatasize = 0;
    void do_dump(unsigned char* _buf, size_t _size) noexcept;
};

#endif // TVIDEO_READER_H
