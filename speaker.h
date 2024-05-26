#ifndef TALSA_H
#define TALSA_H
#define DEBUG_MODE
extern "C"
{
	#include <alsa/asoundlib.h>
}

class TSpeaker
{
public:
	TSpeaker(unsigned int _bitrate,
			 int _nchanels,
		  unsigned int _maxsamples);
	~TSpeaker();
	bool is_init() const;
	bool write_pcm(const unsigned char *_data, size_t _samples);
	void SetAlsaMasterVolume(unsigned _volume);
	unsigned getVolume() const;
	bool pauseProcess();
private:
	bool m_isinit = false;
	unsigned int m_nchanels;
	snd_pcm_t * m_pcm_handle = nullptr;
	snd_pcm_hw_params_t * m_params = nullptr;
	unsigned m_volume;
};

#endif // TALSA_H
