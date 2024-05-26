#include "speaker.h"
#include <cmath>
const auto cdesired_format = SND_PCM_FORMAT_S16;
const auto cmode = SND_PCM_STREAM_PLAYBACK;
const unsigned int cbytes_per_sample = 2;

#define PCM_DEVICE "default"

//static char *device = "plughw:0,0";         /* playback device */
//static snd_pcm_format_t format = SND_PCM_FORMAT_S16;    /* sample format */

TSpeaker::TSpeaker(unsigned int _bitrate,
			 int _nchanels,
			 unsigned int _maxsamples): m_volume(0)
{
	m_isinit = false;
	if (_nchanels < 1)
		return;
	int pcm_result;
	// Open the PCM device in playback mode
	pcm_result = snd_pcm_open(&m_pcm_handle, PCM_DEVICE, cmode, 0);
	if (pcm_result  < 0)
	{
		printf("ERROR: Can't open \"%s\" PCM device. %s\n",
			   PCM_DEVICE, snd_strerror(pcm_result));
		return;
	}
	// Allocate parameters object and fill it with default values
	snd_pcm_hw_params_alloca(&m_params);
	snd_pcm_hw_params_any(m_pcm_handle, m_params);

    pcm_result = snd_pcm_hw_params_set_access(m_pcm_handle,
                                              m_params,
                                              SND_PCM_ACCESS_RW_INTERLEAVED);


	if (pcm_result < 0)
	{
		printf("ERROR: Can't set interleaved mode. %s\n", snd_strerror(pcm_result));
		return;
	}
	pcm_result = snd_pcm_hw_params_set_format(m_pcm_handle, m_params, cdesired_format);
	if (pcm_result < 0)
	{
		printf("ERROR: Can't set format. %s\n", snd_strerror(pcm_result));
		return;
	}
	pcm_result = snd_pcm_hw_params_set_channels(m_pcm_handle, m_params, _nchanels);
	if (pcm_result < 0)
	{
		printf("ERROR: Can't set channels number. %s\n", snd_strerror(pcm_result));
		return;
	}
	pcm_result = snd_pcm_hw_params_set_rate_near(m_pcm_handle, m_params, &_bitrate, 0);
	if (pcm_result < 0)
	{
		printf("ERROR: Can't set rate. %s\n", snd_strerror(pcm_result));
		return;
	}
    pcm_result = snd_pcm_hw_params_set_period_size(m_pcm_handle, m_params, 64, 0);
    if (pcm_result < 0)
    {
        printf("ERROR: Can't set period size. %s\n", snd_strerror(pcm_result));
        return;
    }

    pcm_result = snd_pcm_hw_params_set_buffer_size(m_pcm_handle, m_params, _maxsamples*2);
    if (pcm_result < 0)
    {
        printf("ERROR: Can't set buffer size. %s\n", snd_strerror(pcm_result));
        return;
    }

	// Write parameters
	pcm_result = snd_pcm_hw_params(m_pcm_handle, m_params);
	if (pcm_result < 0)
	{
		printf("ERROR: Can't set harware parameters. %s\n", snd_strerror(pcm_result));
		return;
	}

#ifdef DEBUG_MODE
	printf("alsa: audio device with PCM name: '%s' is used\n", snd_pcm_name(m_pcm_handle));
#endif
	snd_pcm_state_t pcm_state = snd_pcm_state(m_pcm_handle);
	const char * str_pcm_state = snd_pcm_state_name(pcm_state);
#ifdef DEBUG_MODE
	printf("alsa: PCM state: %s\n", str_pcm_state);
#endif
	snd_pcm_hw_params_get_channels(m_params, &m_nchanels);
#ifdef DEBUG_MODE
	printf("alsa: set nchannels: %i \n", m_nchanels);
#endif
	unsigned int tmp = m_nchanels;
	if (_nchanels != m_nchanels)
	{
		printf("ERROR: Can't set number of chanels \n");
		return;
	}
#ifdef DEBUG_MODE
	if (m_nchanels == 1)
	{
		printf("alsa: set mono sound\n");
	}
	else if (m_nchanels == 2)
	{
		printf("alsa: set stereo sound\n");
	}
	else
	{
		printf("alsa: set %d sound chanels\n", m_nchanels);
	}
#endif
	snd_pcm_hw_params_get_rate(m_params, &tmp, 0);
	if (_bitrate != tmp)
	{
		printf("ERROR: Can't set bitrate \n");
		return;
	}
#ifdef DEBUG_MODE
	printf("alsa: set bitrate: %d\n", tmp);
#endif
	snd_pcm_uframes_t nframes; //1024
	// Allocate buffer to hold single period
	snd_pcm_hw_params_get_period_size(m_params, &nframes, 0);
#ifdef DEBUG_MODE
    printf("alsa: set nframes per period: %d\n", nframes);
#endif

	// Extract period time from a configuration space in usecs
	snd_pcm_hw_params_get_period_time(m_params, &tmp, NULL);
#ifdef DEBUG_MODE
	printf("alsa: set period time for buffer(in useconds): %d\n", tmp); //128000
#endif
	snd_pcm_prepare(m_pcm_handle);
    m_isinit = true;
}

bool TSpeaker::write_pcm(const unsigned char *_data, size_t _samples)
{
	int pcm_result;
	//snd_pcm_uframes_t curr_nframes = _size/(m_nchanels * cbytes_per_sample);
			//bytes_read/(channels * cbytes_per_sample);
	pcm_result = snd_pcm_writei(m_pcm_handle, _data, _samples);
	if (pcm_result == -EPIPE)
	{
		printf("XRUN. written %d\n",_samples);
        snd_pcm_prepare(m_pcm_handle);
        pcm_result = snd_pcm_writei(m_pcm_handle, _data, _samples);

	}
	else if (pcm_result < 0)
	{
		printf("ERROR: Can't write to PCM device. %s\n", snd_strerror(pcm_result));
		return false;
	}
	return true;
}

bool TSpeaker::is_init() const
{
	return m_isinit;
}


void TSpeaker::SetAlsaMasterVolume(unsigned _volume)
{
	if (_volume > 100)
		return;
	long min, max;
	snd_mixer_t * mixer_handle;
	snd_mixer_selem_id_t *sid;
	const char *selem_name = "Master";
	// second argument isn't used and doesn't have any purpose
	snd_mixer_open(&mixer_handle, 0);
	snd_mixer_attach(mixer_handle, PCM_DEVICE);
	snd_mixer_selem_register(mixer_handle, NULL, NULL);
	snd_mixer_load(mixer_handle);

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, selem_name);
	snd_mixer_elem_t* elem = snd_mixer_find_selem(mixer_handle, sid);

	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	snd_mixer_selem_set_playback_volume_all(elem, _volume * max / 100);

	snd_mixer_close(mixer_handle);
	m_volume = _volume;
}

unsigned TSpeaker::getVolume() const
{
	return m_volume;
}

bool TSpeaker::pauseProcess()
{
	snd_pcm_drop(m_pcm_handle);
	snd_pcm_prepare(m_pcm_handle);
}

TSpeaker::~TSpeaker()
{
	snd_pcm_drain(m_pcm_handle);
	snd_pcm_close(m_pcm_handle);
}
