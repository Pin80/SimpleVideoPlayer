#include "mainwindow.h"


static void error_callback(int error, const char* description)
{
    (void)error;
    showerrmsg("GLFW error");
    showerrmsg(description);
}

class Twindow_helper
{
public:
	Twindow_helper(Twindow* _wnd) : m_wnd(_wnd)
	{ 	}
	void key_callback(GLFWwindow* _window,
                     int _key,
                     int _scancode,
                     int _action,
                     int _mods)
	{
        (void)_scancode;
        (void)_mods;
        assert(m_wnd);
        assert(m_wnd->m_reader);
        assert(m_wnd->m_speaker);
        switch(_key)
		{
			case GLFW_KEY_ESCAPE:
			{
                if (_action == GLFW_PRESS)
                {
                    m_wnd->m_event = eExit;
                }
				break;
			}
			case GLFW_KEY_P:
			{
                if (_action == GLFW_PRESS)
				{
					if (!m_wnd->m_reader->isPaused())
					{
                        m_wnd->m_event = ePause;
					}
					else
					{
                        m_wnd->m_event = eResume;
					}
				}
				break;
			}
            case GLFW_KEY_F:
            {
                if (_action == GLFW_PRESS)
                {
                    if (!m_wnd->isFull())
                    {
                        m_wnd->m_oldwnd_width = m_wnd->m_wnd_width;
                        m_wnd->m_oldwnd_height = m_wnd->m_wnd_height;
                        m_wnd->m_oldscale = m_wnd->m_scale;
                        m_wnd->m_monitor = glfwGetPrimaryMonitor();
                        int xpos = -1;
                        int ypos = -1;
                        int width = -1;
                        int height = -1;
                        glfwGetMonitorWorkarea(m_wnd->m_monitor,
                                               &xpos,
                                               &ypos,
                                               &width,
                                               &height);
                        m_wnd->m_wnd_width = width + xpos;
                        m_wnd->m_wnd_height = height + ypos;
                        int dist_x = m_wnd->m_wnd_width - m_wnd->m_mctx.opengl_width*m_wnd->m_scale;
                        int dist_y = m_wnd->m_wnd_height - m_wnd->m_mctx.opengl_height*m_wnd->m_scale;
                        if (dist_x > dist_y)
                        {
                            m_wnd->m_scale = (float)m_wnd->m_wnd_height/(float)m_wnd->m_mctx.opengl_height;
                        }
                        else
                        {
                            m_wnd->m_scale = (float)m_wnd->m_wnd_width/(float)m_wnd->m_mctx.opengl_width;
                        }
                        unsigned wnd_width = (m_wnd->m_mctx.opengl_width < m_wnd->m_wnd_width)?m_wnd->m_wnd_width:m_wnd->m_mctx.opengl_width;
                        unsigned wnd_height = (m_wnd->m_mctx.opengl_height < m_wnd->m_wnd_height)?m_wnd->m_wnd_height:m_wnd->m_mctx.opengl_height;
                        float width_pads = (float)((m_wnd->m_scale*m_wnd->m_mctx.opengl_width < wnd_width)?(wnd_width - m_wnd->m_scale*m_wnd->m_mctx.opengl_width)/2:0)/
                                           (float)wnd_width;
                        float height_pads = (float)((m_wnd->m_scale*m_wnd->m_mctx.opengl_height < wnd_height)?(wnd_height - m_wnd->m_scale*m_wnd->m_mctx.opengl_height)/2:0)/
                                (float)wnd_height;
                        glfwSetWindowMonitor(m_wnd->m_window,
                                             m_wnd->m_monitor,
                                             0,
                                             0,
                                             m_wnd->m_wnd_width,
                                             m_wnd->m_wnd_height,
                                             GLFW_DONT_CARE);
                        // Upset down and center x align image
                        glViewport(0, 0, m_wnd->m_wnd_width, m_wnd->m_wnd_height);
                        glRasterPos2f(-1 + 2*width_pads, 1- 2*height_pads);
                        glPixelZoom( 1*m_wnd->m_scale, -1*m_wnd->m_scale );
                        m_wnd->setFull(true);
                        m_wnd->m_event = eFullScreen;
                    }
                    else
                    {
                        m_wnd->m_wnd_width = m_wnd->m_oldwnd_width;
                        m_wnd->m_wnd_height = m_wnd->m_oldwnd_height;
                        m_wnd->m_scale = m_wnd->m_oldscale;
                        glfwSetWindowMonitor(m_wnd->m_window,
                                             NULL,
                                             0,
                                             0,
                                             m_wnd->m_wnd_width,
                                             m_wnd->m_wnd_height,
                                             GLFW_DONT_CARE);
                        m_wnd->setFull(false);
                        // Upset down and center x align image
                        glViewport(0, 0, m_wnd->m_wnd_width, m_wnd->m_wnd_height);
                        glRasterPos2f(-1 + 2*m_wnd->m_oldwidth_pad, 1 - 2*m_wnd->m_oldheight_pad);
                        glPixelZoom( 1*m_wnd->m_scale, -1*m_wnd->m_scale );
                        glLoadIdentity();
                        m_wnd->m_event = eUnFullScreen;
                    }
                }
                break;
            }
			case GLFW_KEY_R:
			{
                if (_action == GLFW_PRESS)
				{
                    m_wnd->m_event = eRestart;
				}
				break;
			}
			case GLFW_KEY_S:
			{
                if (_action == GLFW_PRESS)
                {
                    m_wnd->m_event = eAddVolume;
                }
				break;
			}
            case GLFW_KEY_O:
            {
                if (_action == GLFW_PRESS)
                {
                    if (_mods == GLFW_MOD_SHIFT)
                    {
                        int rect_tl_abs_ctx[2];
                        int rect_br_abs_ctx[2];
                        int offsetx = (m_wnd->m_wnd_width - m_wnd->m_scale*m_wnd->m_mctx.opengl_width)/2;
                        int offsety = (m_wnd->m_wnd_height - m_wnd->m_scale*m_wnd->m_mctx.opengl_height)/2;
                        rect_tl_abs_ctx[0] = (m_wnd->m_rect_tl_abs[0] - offsetx)/m_wnd->m_scale;
                        rect_tl_abs_ctx[1] = (m_wnd->m_rect_tl_abs[1] - offsety)/m_wnd->m_scale;
                        rect_br_abs_ctx[0] = (m_wnd->m_rect_br_abs[0] - offsetx)/m_wnd->m_scale;
                        rect_br_abs_ctx[1] = (m_wnd->m_rect_br_abs[1] - offsety)/m_wnd->m_scale;
                        m_wnd->m_eventParams[0].int_ = rect_tl_abs_ctx[0];
                        m_wnd->m_eventParams[1].int_ = rect_tl_abs_ctx[1];
                        m_wnd->m_eventParams[2].int_ = rect_tl_abs_ctx[2];
                        m_wnd->m_eventParams[3].int_ = rect_tl_abs_ctx[3];
                        m_wnd->m_event = eCroppedShot;
                    }
                    else
                    {
                        m_wnd->m_event = eScreenShot;
                    }
                }
                break;
            }
            case GLFW_KEY_M:
            {
                if (_action == GLFW_PRESS)
                {
                    m_wnd->m_event = eMute;
                }
                break;
            }
			case GLFW_KEY_X:
			{
                if (_action == GLFW_PRESS)
                {
                    m_wnd->m_event = eDecVolume;
                }
				break;
			}
			case GLFW_KEY_B:
			{
                if (_action == GLFW_PRESS)
                {
                    m_wnd->m_event = eBackStep;
                }
				break;
			}
            case GLFW_KEY_N:
            {
                if (_action == GLFW_PRESS)
                {
                    m_wnd->m_event = eNextStep;
                }
                break;
            }
			default:
			{
				break;
			}
		}
	}
    void cursor_callback(GLFWwindow* window, double _xpos, double _ypos)
    {
        double xpos_abs, ypos_abs;
        glfwGetCursorPos(window, &xpos_abs, &ypos_abs);
        m_wnd->m_rect_bottom_right[0] = -1 + 2*xpos_abs/m_wnd->m_wnd_width;
        m_wnd->m_rect_bottom_right[1] = 1 - 2*ypos_abs/m_wnd->m_wnd_height;
    }
    void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
    {
        double xpos_abs, ypos_abs;
        if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        {
           //getting cursor position
           glfwGetCursorPos(window, &xpos_abs, &ypos_abs);
           m_wnd->m_rect_tl_abs[0] = xpos_abs;
           m_wnd->m_rect_tl_abs[1] = ypos_abs;
           m_wnd->m_rect_top_left[0] = -1 + 2*xpos_abs/m_wnd->m_wnd_width;
           m_wnd->m_rect_top_left[1] = 1 - 2*ypos_abs/m_wnd->m_wnd_height;
           m_wnd->m_rect_bottom_right[0] = m_wnd->m_rect_top_left[0];
           m_wnd->m_rect_bottom_right[1] = m_wnd->m_rect_top_left[1];
           lmsgr << "Pressed Cursor Position at ("
                 << m_wnd->m_rect_top_left[0] << " : " << m_wnd->m_rect_top_left[1] << mend;
           m_wnd->m_isMButtomPressed = true;
        }
        else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
        {
            glfwGetCursorPos(window, &xpos_abs, &ypos_abs);
            m_wnd->m_rect_br_abs[0] = xpos_abs;
            m_wnd->m_rect_br_abs[1] = ypos_abs;
            m_wnd->m_rect_bottom_right[0] = -1 + 2*xpos_abs/m_wnd->m_wnd_width;
            m_wnd->m_rect_bottom_right[1] = 1 - 2*ypos_abs/m_wnd->m_wnd_height;
            m_wnd->m_isMButtomPressed = false;
            lmsgr << "Pressed Cursor Position at ("
                  << m_wnd->m_rect_bottom_right[0] << " : " << m_wnd->m_rect_bottom_right[1] << mend;
        }
    }
private:
	Twindow* m_wnd;
};

static void cursor_callback(GLFWwindow* _window, double _xpos, double _ypos)
{
    try
    {
        void* ptr = glfwGetWindowUserPointer(_window);
        if (!ptr)
            return;
        Twindow * wnd = reinterpret_cast<Twindow*>(ptr);
        Twindow_helper wnd_helper(wnd);
        wnd_helper.cursor_callback(_window, _xpos, _ypos);
    }
    catch(...)
    {
        showerrmsg("Exception in key_callback");
    }
}

void mouse_button_callback(GLFWwindow* _window, int _button, int _action, int _mods)
{
    try
    {
        void* ptr = glfwGetWindowUserPointer(_window);
        if (!ptr)
            return;
        Twindow * wnd = reinterpret_cast<Twindow*>(ptr);
        Twindow_helper wnd_helper(wnd);
        wnd_helper.mouse_button_callback(_window, _button, _action, _mods);
    }
    catch(...)
    {
        showerrmsg("Exception in key_callback");
    }
}

static void key_callback(GLFWwindow* _window,
						 int key,
						 int scancode,
						 int action,
						 int mods)
{
	try
	{
		void* ptr = glfwGetWindowUserPointer(_window);
		if (!ptr)
			return;
		Twindow * wnd = reinterpret_cast<Twindow*>(ptr);
		Twindow_helper wnd_helper(wnd);
		wnd_helper.key_callback(_window, key, scancode, action, mods);
	}
	catch(...)
	{
        showerrmsg("Exception in key_callback");
	}
}

Twindow::Twindow(const char* _name,
                 const TVideo_reader *_reader,
                 const TSound *_speaker,
                 const TMainContext& _mctx) noexcept
    : m_reader(_reader),
      m_speaker(_speaker),
      m_flag(false)
{
    if (!m_reader)
		return;
    if (!m_speaker)
		return;
    if (_mctx.width < 1)
		return;
    if (_mctx.height < 1)
		return;
    m_scale = 1;
    m_mctx.height = _mctx.height;
    m_mctx.width = _mctx.width;
    if (m_mctx.height % 4 == 0)
    {
        m_mctx.opengl_height = m_mctx.height;
    }
    else
    {
        m_mctx.opengl_height = 4*(m_mctx.height/4) + 4;
    }
    if (m_mctx.width % 4 == 0)
    {
        m_mctx.opengl_width = m_mctx.width;
    }
    else
    {
        m_mctx.opengl_width = 4*(m_mctx.width/4) + 4;
    }
    m_is_full = false;
    m_monitor = nullptr;
    if (!glfwInit())
    {
        showerrmsg("Couldn't init gui engine");
        return;
    }
	glfwSetErrorCallback(error_callback);

    m_window = nullptr;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    GLFWmonitor* monitor  = glfwGetPrimaryMonitor();
    const GLFWvidmode* vidmode = glfwGetVideoMode(monitor);
    m_window = glfwCreateWindow(vidmode->width, vidmode->height, _name, NULL, NULL);
    glfwGetWindowSize(m_window, &m_wnd_maxwidth, &m_wnd_maxheight);
    if (m_wnd_maxwidth < (2*MIN_SCREEN_WIDTH/3) || m_wnd_maxheight < (2*MIN_SCREEN_HEIGHT/3))
    {
        elmsgr << "Size of window too small" << mend;
        return;
    }
    const auto minimum_pad = 10;
    if (((m_mctx.opengl_width + minimum_pad)< m_wnd_maxwidth)
            && ((m_mctx.opengl_height + minimum_pad)< m_wnd_maxheight))
    {
        glfwHideWindow(m_window);
        m_wnd_width = (m_mctx.opengl_width < MIN_SCREEN_WIDTH)?MIN_SCREEN_WIDTH:m_mctx.opengl_width;
        m_wnd_height = ((m_mctx.opengl_height + 16)<m_wnd_maxheight)?m_mctx.opengl_height + 64:
                       m_mctx.opengl_height;
        glfwSetWindowSize(m_window, m_wnd_width, m_wnd_height);
        m_wnd_width = m_wnd_width;
        m_wnd_height = m_wnd_height;
        float prepads_width = m_wnd_width - m_scale*m_mctx.opengl_width;
        m_oldwidth_pad = (float)((m_mctx.opengl_width*m_scale < m_wnd_width)?( prepads_width )/2:0)/
                           (float)m_wnd_width;
        float prepads_height = m_wnd_height - m_scale*m_mctx.opengl_height;
        m_oldheight_pad = (float)((m_mctx.opengl_height*m_scale < m_wnd_height)?(float)(prepads_height)/2:0)/
                           (float)m_wnd_height;
        glfwShowWindow(m_window);
        glfwPollEvents();
    }
    else
    {
        m_wnd_width = m_wnd_maxwidth;
        m_wnd_height = m_wnd_maxheight;
        if ((m_wnd_width - m_mctx.opengl_width) < (m_wnd_height - m_mctx.opengl_height))
        {
            m_scale = (float)(m_wnd_width - minimum_pad)/(float)(m_mctx.opengl_width);
        }
        else
        {
            m_scale = (float)(m_wnd_height - minimum_pad)/(float)(m_mctx.opengl_height);
        }
        float prepads_width = m_wnd_width - m_scale*m_mctx.opengl_width;
        m_oldwidth_pad = (float)((m_mctx.opengl_width*m_scale < m_wnd_width)?( prepads_width )/2:0)/
                           (float)m_wnd_width;
        float prepads_height = m_wnd_height - m_scale*m_mctx.opengl_height;
        m_oldheight_pad = (float)((m_mctx.opengl_height*m_scale < m_wnd_height)?(float)(prepads_height)/2:0)/
                           (float)m_wnd_height;
    }
	if (!m_window)
	{
        showerrmsg("Couldn't create window");
		return;
	}

    glfwSetWindowUserPointer(m_window, this);
	glfwSetKeyCallback(m_window, key_callback);
    glfwSetCursorPosCallback( m_window, cursor_callback);
    glfwSetMouseButtonCallback(m_window, mouse_button_callback);
	// Make the window's context current
	glfwMakeContextCurrent(m_window); // associate window to glut
    glfwSwapInterval(1);

    gladLoadGL();
    glViewport( 0, 0, m_wnd_width, m_wnd_height );
	// Render here
	// Upset down and center x align image
    glRasterPos2f(-1 + 2*m_oldwidth_pad, 1 - 2*m_oldheight_pad);
    glPixelZoom( 1*m_scale, -1*m_scale );
    glLoadIdentity();
    glClearColor(0.1f, 0.2f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glLineWidth(3);
    m_rect_top_left[0] = -0.5;
    m_rect_top_left[1] = 0.5;
    m_rect_bottom_right[0] = 0.5;
    m_rect_bottom_right[1] = -0.5;
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glColor3f(0.0,1.0,0.0);
    glBegin(GL_POLYGON);
        glVertex2f(m_rect_top_left[0],m_rect_top_left[1]);
        glVertex2f(m_rect_bottom_right[0],m_rect_top_left[1]);
        glVertex2f(m_rect_bottom_right[0],m_rect_bottom_right[1]);
        glVertex2f(m_rect_top_left[0],m_rect_bottom_right[1]);
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    std:size_t valsize = m_mctx.opengl_height*m_mctx.opengl_width*3;
    m_validImage = TSmartPtr<uint8_t>(valsize);
    memset(const_cast<uint8_t*>(m_validImage.get()), 0, valsize);
    if (!m_validImage)
        return;
    glfwSwapBuffers(m_window);
	m_isinit = true;
}

Twindow::~Twindow()
{
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

int Twindow::show_image(const uint8_t * _data, size_t _width, size_t _height) noexcept
{
    if (!m_isinit)
        return -1;
    if (!_data)
        return -1;
    for(int i = 0; i < m_mctx.opengl_height; i++)
        for(int j = 0; j < m_mctx.opengl_width; j++)
        {
            int offset = i*m_mctx.width*3 + j*3;
            int offset_val = i*m_mctx.opengl_width*3 + j*3;
            *m_validImage[offset_val] = _data[offset];
            *m_validImage[offset_val + 1] = _data[offset + 1];
            *m_validImage[offset_val + 2] = _data[offset + 2];
        }
    if (m_isMButtomPressed)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawPixels(m_mctx.opengl_width,
                     m_mctx.opengl_height,
                     GL_RGB,
                     GL_UNSIGNED_BYTE,
                     m_validImage.get());
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glColor3f(0.0,1.0,0.0);
        glBegin(GL_POLYGON);
            glVertex2f(m_rect_top_left[0],m_rect_top_left[1]);
            glVertex2f(m_rect_bottom_right[0],m_rect_top_left[1]);
            glVertex2f(m_rect_bottom_right[0],m_rect_bottom_right[1]);
            glVertex2f(m_rect_top_left[0],m_rect_bottom_right[1]);
        glEnd();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    else
    {
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawPixels(m_mctx.opengl_width,
                     m_mctx.opengl_height,
                     GL_RGB,
                     GL_UNSIGNED_BYTE,
                     m_validImage.get());
    }

	// Swap front and back buffers
    glfwSwapBuffers(m_window);
    return 0;
}

int Twindow::show_last_image() noexcept
{
    if (m_isMButtomPressed)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawPixels(m_mctx.opengl_width,
                     m_mctx.opengl_height,
                     GL_RGB,
                     GL_UNSIGNED_BYTE,
                     m_validImage.get());
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glColor3f(0.0,1.0,0.0);
        glBegin(GL_POLYGON);
            glVertex2f(m_rect_top_left[0],m_rect_top_left[1]);
            glVertex2f(m_rect_bottom_right[0],m_rect_top_left[1]);
            glVertex2f(m_rect_bottom_right[0],m_rect_bottom_right[1]);
            glVertex2f(m_rect_top_left[0],m_rect_bottom_right[1]);
        glEnd();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    else
    {
        if (!m_reader->isPaused())
        {
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawPixels(m_reader->get_width(),
                         m_reader->get_height(),
                         GL_RGB,
                         GL_UNSIGNED_BYTE,
                         m_validImage.get());
        }
    }
    // Swap front and back buffers
    glfwSwapBuffers(m_window);
    return 0;
}

void Twindow::process_events() noexcept
{
	glfwPollEvents();
}

bool Twindow::shouldClose() noexcept
{
	return glfwWindowShouldClose(m_window);
}

bool Twindow::isInit() const noexcept
{
    return m_isinit;
}

bool Twindow::isFlag() const noexcept
{
    return m_flag;
}

bool Twindow::isFull() const noexcept
{
    return m_is_full;
}

void Twindow::setFull(bool _isfull) noexcept
{
    m_is_full = _isfull;
}

void Twindow::getScreenResolution(int &_width, int &_height)
{
    if (!glfwInit())
    {
        _width = -1;
        _height = -1;
        showerrmsg("Couldn't init gui engine");
        return;
    }
    GLFWmonitor* monitor  = glfwGetPrimaryMonitor();
    const GLFWvidmode * mode = glfwGetVideoMode(monitor);
    _width = mode->width;
    _height = mode->height;
}

TEvent Twindow::extractEvent(TVariant& _par1,
                             TVariant& _par2,
                             TVariant& _par3,
                             TVariant& _par4)
{
    TEvent e = m_event;
    _par1 = m_eventParams[0];
    _par2 = m_eventParams[1];
    _par3 = m_eventParams[2];
    _par4 = m_eventParams[3];
    m_event = eNoEvent;
    return e;
}

void Twindow::closeWindow()
{
    glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}

float Twindow::getScale() const
{
    return m_scale;
}

