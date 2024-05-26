#ifndef TWINDOW_H
#define TWINDOW_H
#include "video_reader.h"
#include "sounda.h"
#include "soundp.h"
#include "soundp2.h"
#include "common.h"

#ifdef SOUND_WAY
    #if SOUND_WAY == 1
        typedef TSoundL TSound;
    #else
        typedef TSoundP TSound;
    #endif
#else
    typedef TSoundP TSound;
#endif

struct TMainContext
{
    int width;
    int height;
    int opengl_width;
    int opengl_height;
    std::size_t naudioframes;
    std::size_t naudiochannels;
    std::size_t samplerate;
};

class Twindow_helper;

class Twindow : TNonCopyable
{
public:
    Twindow(const char* _name,
            const TVideo_reader* _reader,
            const TSound* _speaker,
            const TMainContext &_mctx) noexcept;
	virtual ~Twindow();
    int show_image(const uint8_t *_data, size_t _width, size_t _height) noexcept;
    int show_last_image() noexcept;
    void process_events() noexcept;
    bool shouldClose() noexcept;
    bool isInit() const noexcept;
    bool isFlag() const noexcept;
    bool isFull() const noexcept;
    void setFull(bool _isfull) noexcept;
    static void getScreenResolution(int& _width, int& _height);
    bool isPaused();
    TEvent extractEvent(TVariant &_par1, TVariant &_par2, TVariant &_par3, TVariant &_par4);
    void closeWindow();
    float getScale() const;
private:
    const TVideo_reader* m_reader;
    const TSound* m_speaker;
	GLFWwindow* m_window;
	bool m_isinit = false;
    bool m_flag;
    TMainContext m_mctx;
    bool m_ismctxset;
    bool m_is_full;
    int m_wnd_width;
    int m_wnd_height;
    int m_wnd_maxwidth;
    int m_wnd_maxheight;
    int m_oldwnd_width;
    int m_oldwnd_height;
    float m_oldscale;
    float m_oldwidth_pad;
    float m_oldheight_pad;
    float m_scale;
    GLFWmonitor * m_monitor;
	friend class Twindow_helper;
    GLfloat m_rect_top_left[2];
    GLfloat m_rect_bottom_right[2];
    int m_rect_tl_abs[2];
    int m_rect_br_abs[2];
    bool m_isMButtomPressed = false;
    TSmartPtr<uint8_t> m_validImage;
    TEvent m_event = eNoEvent;
    TVariant m_eventParams[4];
};

#endif // TWINDOW_H
