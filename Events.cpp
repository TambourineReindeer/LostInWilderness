#include "Events.h"
#include "Camera.h"
#include "Chunk.h"
#include "Config.h"
#include "Renderer.h"

#include <GLFW/glfw3.h>

// Shorthand: Size of one chunk in blocks
#define CX CHUNK_WIDTH
#define CY CHUNK_HEIGHT
#define CZ CHUNK_LENGTH

static Camera		g_camera;
static int			ww, wh;
static int			mx, my, mz;
static int			face;
static uint8_t		buildtype = 1;
static unsigned int keys;
static bool			select_using_depthbuffer = false;

Events::Events()
{
    //ctor
}

Events::~Events()
{
    //dtor
}

// Calculate the forward, right and lookat vectors from the angle vector
static void update_vectors()
{
	g_camera.forward.x = std::sin(g_camera.angle.x);
	g_camera.forward.y = 0;
	g_camera.forward.z = std::cos(g_camera.angle.x);

	g_camera.right.x = -std::cos(g_camera.angle.x);
	g_camera.right.y = 0;
	g_camera.right.z = std::sin(g_camera.angle.x);

	g_camera.lookat.x = std::sin(g_camera.angle.x) * std::cos(g_camera.angle.y);
	g_camera.lookat.y = std::sin(g_camera.angle.y);
	g_camera.lookat.z = std::cos(g_camera.angle.x) * std::cos(g_camera.angle.y);

	g_camera.up = glm::cross(g_camera.right, g_camera.lookat);
}

void resize( GLFWwindow* wnd, int w, int h)
{
	ww = w;
	wh = h;
	glViewport(0, 0, w, h);
}

void key_cb( GLFWwindow* wnd, int key, int scancode, int action, int mods )
{
	if ( action == GLFW_RELEASE )
	switch(key) {
		case GLFW_KEY_A:
			keys &= ~1;
			break;
		case GLFW_KEY_D:
			keys &= ~2;
			break;
		case GLFW_KEY_W:
			keys &= ~4;
			break;
		case GLFW_KEY_S:
			keys &= ~8;
			break;
		case GLFW_KEY_SPACE:
			keys &= ~16;
			break;
		case GLFW_KEY_LEFT_SHIFT:
			keys &= ~32;
			break;
	}
	if ( action == GLFW_PRESS )
	switch(key)
	{
		case GLFW_KEY_A:
			keys |= 1;
			break;
		case GLFW_KEY_D:
			keys |= 2;
			break;
		case GLFW_KEY_W:
			keys |= 4;
			break;
		case GLFW_KEY_S:
			keys |= 8;
			break;
		case GLFW_KEY_SPACE:
			keys |= 16;
			break;
		case GLFW_KEY_LEFT_SHIFT:
			keys |= 32;
			break;
		case GLFW_KEY_HOME:
			g_camera.position = glm::vec3(0, CY / 2, 0);
			g_camera.angle = glm::vec3(0, -0.5, 0);
			update_vectors();
			break;
		case GLFW_KEY_END:
			g_camera.position = glm::vec3(0, CX * Config::getGlobal()->getInteger( "renderer.view_distance" ), 0);
			g_camera.angle = glm::vec3(0, -3.14 * 0.49, 0);
			update_vectors();
			break;
		case GLFW_KEY_F1:
			select_using_depthbuffer = !select_using_depthbuffer;
			if(select_using_depthbuffer)
				printf("Using depth buffer selection method\n");
			else
				printf("Using ray casting selection method\n");
			break;
		case GLFW_KEY_F4:
			{
				/*GLint pm;
				glGetIntegerv( GL_POLYGON_MODE, &pm );
				if ( pm == GL_FILL )
					glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
				else
					glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );*/
			}
			break;
		case GLFW_KEY_ESCAPE:
			glfwSetWindowShouldClose( (GLFWwindow*)Renderer::getWindow( "g_window" ), GL_TRUE );
			break;
	}
}

static void idle()
{
	static double pt = 0.0;
	static const float movespeed = 10;

	double t = glfwGetTime();
	float dt = (t - pt);
	pt = t;

	if(keys & 1)
		g_camera.position -= g_camera.right * movespeed * dt;
	if(keys & 2)
		g_camera.position += g_camera.right * movespeed * dt;
	if(keys & 4)
		g_camera.position += g_camera.forward * movespeed * dt;
	if(keys & 8)
		g_camera.position -= g_camera.forward * movespeed * dt;
	if(keys & 16)
		g_camera.position.y += movespeed * dt;
	if(keys & 32)
		g_camera.position.y -= movespeed * dt;
}

void motion(GLFWwindow *wnd, double x, double y)
{
	static bool warp = false;
	static const float mousespeed = 0.001f;

	if ( glfwGetWindowAttrib( (GLFWwindow*)Renderer::getWindow( "g_window" ), GLFW_FOCUSED) == GL_FALSE )
		return;

	int wx,wy;
	glfwGetWindowPos( (GLFWwindow*)Renderer::getWindow( "g_window" ), &wx, &wy );

	if(!warp)
	{
		g_camera.angle.x -= (x - (((float)ww / 2.0f)) ) * mousespeed;
		g_camera.angle.y -= (y - (((float)wh / 2.0f)) ) * mousespeed;

		if(g_camera.angle.x < -3.14)
			g_camera.angle.x += 3.14 * 2;
		if(g_camera.angle.x > 3.14)
			g_camera.angle.x -= 3.14 * 2;
		if(g_camera.angle.y < -3.14 / 2)
			g_camera.angle.y = -3.14 / 2;
		if(g_camera.angle.y > 3.14 / 2)
			g_camera.angle.y = 3.14 / 2;

        update_vectors();

		// Force the mouse pointer back to the center of the screen.
		// This causes another call to motion(), which we need to ignore.
		warp = true;
		glfwSetCursorPos( (GLFWwindow*)Renderer::getWindow( "g_window" ), (float)ww / 2.0, (float)wh / 2.0);
	} else {
		warp = false;
	}
}

static void mouse(int button, int state, int x, int y)
{
	if(state != GLFW_PRESS)
		return;

	// Scrollwheel
	if(button == 3 || button == 4)
	{
		if(button == 3)
			buildtype--;
		else
			buildtype++;

		//buildtype &= 0xf;
		//fprintf(stderr, "Building blocks of type %u (%s)\n", buildtype, blocknames[buildtype]);
		return;
	}

	fprintf(stderr, "Clicked on %d, %d, %d, face %d, button %d\n", mx, my, mz, face, button);

	/*if(button == 0)
	{
		if(face == 0)
			mx++;
		if(face == 3)
			mx--;
		if(face == 1)
			my++;
		if(face == 4)
			my--;
		if(face == 2)
			mz++;
		if(face == 5)
			mz--;
		world->set( mx, my, mz, buildtype );
	}
	else
	{
		world->set( mx, my, mz, nullptr );
	}*/
}
