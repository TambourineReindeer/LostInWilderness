
#include "LostInWilderness.h"
#include "Buffer.h"
#include "Chunk.h"
#include "Config.h"
#include "Exceptions.h"
#include "Network.h"
#include "Shader.h"
#include "Texture.h"
#include "World.h"
#include "WorldGenerator.h"
#include "FileSystem.h"

#if defined(_MSC_VER)
#include <direct.h>
#include <Windows.h>
// Make an alias to the CRT safe function
#define mkdir _mkdir
#else // GNU
#include <dirent.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <queue>
#include <string>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <io.h>
#else
#endif // _WIN32

#include <sys/stat.h>
#include <mutex>

/** Include WrapperSTL, has thread-safe alternatives to C++ Standard STL 2011 **/
#define __NEED_CONCURRENT_QUEUE__
#include "WrapperSTL.h"

/** Include OpenGL API/Utilities **/
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/noise.hpp>

#include "Events.h"

typedef struct ChunkUpdateStruct {
	enum EType
	{
		CHUNK_GENERATE,
		CHUNK_UPDATE,
		CHUNK_UPDATE_LIGHTING,
		CHUNK_UPDATE_GRAPHIC,
		CHUNK_SAVE,
		CHUNK_LOAD,
		NOTHING
	};
	EType	type;
	Chunk	*chunk;
	int		x,
			y,
			z;
	int		seed;
} ChunkUpdateType;

static Camera		g_camera;
static World		*world;
static int			ww, wh;
static int			mx, my, mz;
static int			face;
static bool			select_using_depthbuffer = false;
struct stat		    info;

uint32_t chunk_update_count = 0;
uint32_t chunk_gen_count = 0;


// Shorthand: Size of one chunk in blocks
#define CX CHUNK_WIDTH
#define CY CHUNK_HEIGHT
#define CZ CHUNK_LENGTH


bool							g_finished;
//g_chunk_updater_mutex;
concurrent::queue<ChunkUpdateType*>	chunk_thread_queue;
//std::queue<ChunkUpdateType*>	chunk_thread_queue;


void LostInWilderness::chunkThread( void )
{
	// -- Set the concurrent context used for parallel buffers, as the current context
	glfwMakeContextCurrent( (GLFWwindow*)Renderer::getWindow( "g_updatecontext" ) );

	// -- While the queue is not empty
	while ( !g_finished )
	{
		if ( !chunk_thread_queue.empty() )
		{
			ChunkUpdateType *chunk_data = nullptr;

			chunk_data = chunk_thread_queue.front();
			chunk_thread_queue.pop();

			switch (chunk_data->type)
			{
			case ChunkUpdateType::CHUNK_GENERATE:
				chunk_data->chunk->generate( chunk_data->seed );
				break;

			case ChunkUpdateType::CHUNK_UPDATE:
				chunk_data->chunk->update( );
				break;

			case ChunkUpdateType::CHUNK_LOAD:
				chunk_data->chunk->load( );
				break;

			case ChunkUpdateType::CHUNK_SAVE:
				chunk_data->chunk->save( );
				break;

			case ChunkUpdateType::CHUNK_UPDATE_GRAPHIC:
			case ChunkUpdateType::CHUNK_UPDATE_LIGHTING:
			case ChunkUpdateType::NOTHING:
				break;
			}

			delete chunk_data;
		}

		//std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
	}

	// -- No context is current
	glfwMakeContextCurrent( nullptr );
    //g_chunk_updater_mutex.unlock();
	std::cout << "[THREAD:Chunks] Chunk thread finished." << std::endl;
}


int init_resources()
{
	// -- Network
	/*Network::HttpRequest http;
	http.open( Network::HttpRequest::HTTP_GET, "www.lostinwilderness.com/updates.php" );
	http.send( std::string("test") );*/

	// -- Default the uniforms/attribs
	Shader *shader_world = ResourceManager::iResourceManager->getShader("default");
	shader_world->setUniform1i( "texture", 0 );
	shader_world->setUniform4f( "g_SunLightSource.position", (float*)glm::value_ptr( glm::vec4( 0, -1, 0, 0 ) ) );
	shader_world->setUniform4f( "g_SunLightSource.diffuse", (float*)glm::value_ptr( glm::vec4( 1, 1, 1, 1 ) ) );
	shader_world->setUniform4f( "g_SunLightSource.ambient", (float*)glm::value_ptr( glm::vec4( 0.4, 0.4, 0.4, 1 ) ) );
	shader_world->setUniform1f( "g_SunLightSource.specular", 1.0f );

	// -- Create the world
	world = new World( "world" );

	// -- Pre-generate the terrain here


    // TODO: place the camera on the terrain
    //pseudocode: getHeight(curPos) --> set y value to this
    // ResourceManager::iResourceManager->getWorldGen("default").getTerrainHeight(0,0,0)
	g_camera.position = glm::vec3(0, CY / 2, 0);
	g_camera.angle = glm::vec3(0, -0.5, 0);
	Events::update_vectors();

	return 1;
}

// -- Not really GLSL fract(), but the absolute distance to the nearest integer value
static float fract(float value)
{
	float f = value - floorf(value);
	if(f > 0.5)
		return 1 - f;
	else
		return f;
}


static double prevFrameTime = 0.0;
static unsigned int framesPerSecond = 0;
static unsigned int frameCount = 0;

static void display()
{
	LostInWilderness *client = LostInWilderness::iGaiaCraft;
	float fov = Config::getGlobal()->getFloat( "renderer.field_of_view" ) / 180.0f * 3.14f;
	float aspect = (float)ww / (float)wh;
	float znear = 0.25f;
	float zfar = 16.0f * 16;

	glm::mat4 view = glm::lookAt(g_camera.position, g_camera.position + g_camera.lookat, g_camera.up);
	glm::mat4 projection = glm::perspective( fov, aspect, znear, zfar );
	glm::mat4 mvp = projection * view;

	Renderer::default_shader->setUniformMatrix( "mvp", glm::value_ptr(mvp) );
	Renderer::default_shader->setUniform1i( "b_lighting", true );

	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glDisable( GL_BLEND );
	glEnable( GL_DEPTH_TEST );
	glEnable( GL_POLYGON_OFFSET_FILL );
	glEnable( GL_CULL_FACE );
	glCullFace( GL_BACK );

	// -- Then draw chunks

	world->update();

	world->render(mvp);

	// -- At which voxel are we looking?

	if ( select_using_depthbuffer )
	{
		// -- Find out coordinates of the center pixel

		float depth;
		glReadPixels(ww / 2, wh / 2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

		glm::vec4 viewport = glm::vec4(0, 0, ww, wh);
		glm::vec3 wincoord = glm::vec3(ww / 2, wh / 2, depth);
		glm::vec3 objcoord = glm::unProject(wincoord, view, projection, viewport);

		// -- Find out which block it belongs to

		mx = objcoord.x;
		my = objcoord.y;
		mz = objcoord.z;
		if(objcoord.x < 0)
			mx--;
		if(objcoord.y < 0)
			my--;
		if(objcoord.z < 0)
			mz--;

		// -- Find out which face of the block we are looking at

		if(fract(objcoord.x) < fract(objcoord.y))
			if(fract(objcoord.x) < fract(objcoord.z))
				face = 0; // X
			else
				face = 2; // Z
		else
			if(fract(objcoord.y) < fract(objcoord.z))
				face = 1; // Y
			else
				face = 2; // Z

		if(face == 0 && g_camera.lookat.x > 0)
			face += 3;
		if(face == 1 && g_camera.lookat.y > 0)
			face += 3;
		if(face == 2 && g_camera.lookat.z > 0)
			face += 3;
	}
	else
	{
		// -- Very naive ray casting algorithm to find out which block we are looking at

		glm::vec3 testpos = g_camera.position;
		glm::vec3 prevpos = g_camera.position;

		for ( auto i = 0; i < 16 * 4; i++ )
		{
			// -- Advance from our currect position to the direction we are looking at, in increments of 1 voxel.
			// NOTE: 1 voxel is equal to 1.0f / 16.0f

			prevpos = testpos;
			testpos += g_camera.lookat * (1.0f / 16.0f);

			mx = floorf(testpos.x);
			my = floorf(testpos.y);
			mz = floorf(testpos.z);

			// -- If this block is not air, then break the loop
			if ( world->getBlock(mx, my, mz) )
				break;
		}

		// -- Find out which face of the block we are looking at

		int px = floorf(prevpos.x);
		int py = floorf(prevpos.y);
		int pz = floorf(prevpos.z);

		if(px > mx)
			face = 0;
		else if(px < mx)
			face = 3;
		else if(py > my)
			face = 1;
		else if(py < my)
			face = 4;
		else if(pz > mz)
			face = 2;
		else if(pz < mz)
			face = 5;

		// -- If we are looking at air, move the cursor out of sight
		if ( world->getBlock(mx, my, mz) == nullptr )
		{
			mx = my = mz = 99999;
		}
	}

	// -- Overlay
	glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_CULL_FACE);
	glDisable( GL_DEPTH_TEST );

	view		= glm::ortho( 0.0f, (float)ww, (float)wh, 0.0f );
	projection	= glm::mat4(1);
	mvp			= view * projection;

	Renderer::default_shader->setUniformMatrix( "mvp", glm::value_ptr(mvp) );
	Renderer::default_shader->setUniform1i( "b_lighting", false );

	std::wostringstream overlay_s;
	overlay_s	<< L"FPS: " << framesPerSecond
				<< L" Updates: " << chunk_update_count
				<< L" Gens: " << chunk_gen_count
				<< L"\nX,Y,Z: " << client->camera->position.x << "," << client->camera->position.y << "," << client->camera->position.z
				<< L"\nCX,CY,CZ: " << (client->camera->position.x/CHUNK_WIDTH) << "," << (client->camera->position.y/CHUNK_HEIGHT) << "," << (client->camera->position.z/CHUNK_LENGTH)
				<< L"\nOpenGL 3.2 Core | GLSL 1.50";

	Renderer::font_texture->bind();
	Renderer::font_texture->drawString( 2, 2, 0xFFFFFFFF, 2.0f, overlay_s.str().c_str() );
	Renderer::blocks_texture->bind();


	// -- Swap the framebuffer back and front, then poll for events in this thread
	glfwSwapBuffers( (GLFWwindow*)Renderer::getWindow( "g_window" ) );
	glfwPollEvents();

	// -- Find the FPS
	double curTime = glfwGetTime();
	if ( curTime >= prevFrameTime+1.0 )
	{
		prevFrameTime = curTime;
		framesPerSecond = frameCount + 1;
		frameCount = 0;
		chunk_update_count = 0;
		chunk_gen_count = 0;
	}
	else frameCount++;
}


/*********************************************************************************************
** Everything below this comment block actually belongs in this source file...
*********************************************************************************************/

LostInWilderness *LostInWilderness::iGaiaCraft;


LostInWilderness::LostInWilderness()
{
	LostInWilderness::iGaiaCraft = this;

	this->camera						= &g_camera;
	this->config						= Config::getGlobal();

	// -- Load the configuration file
	this->config->load( "client.properties" );

	// -- Prepare the network API
	Network::network_initialise();

	Renderer::iRenderer					= new Renderer( LostInWilderness::iGaiaCraft->config->getString( "renderer.api" ) );
	ResourceManager::iResourceManager	= new ResourceManager();
}

LostInWilderness::~LostInWilderness()
{
	Network::network_cleanup();

	// CDB caught this, GDB did not, cannot delete a static variable
	//delete this->config;
	delete ResourceManager::iResourceManager;
	delete Renderer::iRenderer;
}

void LostInWilderness::addChunkToGenerateQueue( int x, int y, int z, int seed, Chunk *chunk )
{
	ChunkUpdateType *chunk_data = new ChunkUpdateType;
	chunk_data->chunk	= chunk;
	chunk_data->type	= ChunkUpdateType::CHUNK_GENERATE;
	chunk_data->x		= x;
	chunk_data->y		= y;
	chunk_data->z		= z;
	chunk_data->seed	= seed;

	chunk_thread_queue.push( chunk_data );
}

void LostInWilderness::addChunkToUpdateQueue( Chunk *chunk )
{
	ChunkUpdateType *chunk_data = new ChunkUpdateType;
	chunk_data->chunk	= chunk;
	chunk_data->type	= ChunkUpdateType::CHUNK_UPDATE;
	chunk_data->x		= 0;
	chunk_data->y		= 0;
	chunk_data->z		= 0;
	chunk_data->seed	= 0;

	chunk_thread_queue.push( chunk_data );
}

void LostInWilderness::addChunkToSaveQueue( Chunk *chunk )
{
	ChunkUpdateType *chunk_data = new ChunkUpdateType;
	chunk_data->chunk	= chunk;
	chunk_data->type	= ChunkUpdateType::CHUNK_SAVE;
	chunk_data->x		= 0;
	chunk_data->y		= 0;
	chunk_data->z		= 0;
	chunk_data->seed	= 0;

	chunk_thread_queue.push( chunk_data );
}

int LostInWilderness::run()
{
	// -- File system manipulation goes here
	FileSystem::Directory::create("../world");

#if defined(_MSC_VER)

	WIN32_FIND_DATA fd;
	HANDLE hfile = FindFirstFile("..\\world\\*", &fd);

	if (hfile != INVALID_HANDLE_VALUE)
	{
		while (FindNextFile(hfile, &fd))
		{
			std::string filename = fd.cFileName;

			// Don't delete special files/folders
			if (!filename.compare(".") || !filename.compare("..")) continue;

			DeleteFile(filename.c_str());
		}

		FindClose(hfile);
	}

#elif defined(__GNUC__)
	if (!(info.st_mode & S_IFDIR))
	{

		// -- NOTE: This is temporary
		DIR *d = opendir("../world");
		struct dirent *dp;

		while ((dp = readdir(d)) != nullptr)
		{
			std::string file = dp->d_name;

			if (!file.compare(".") || !file.compare("..")) continue;

			std::remove(file.c_str());
		}

		closedir(d);
	}
#endif

	// -- Initialise any threads here
	g_finished = false;
	this->threads.insert( std::make_pair( std::string("chunks"), std::thread(LostInWilderness::chunkThread) ) );

	if (init_resources())
	{
		GLFWwindow *window = (GLFWwindow*)Renderer::getWindow( "g_window" );
		glfwGetWindowSize( window, &ww, &wh );
		glfwSetCursorPos( window, ww / 2, wh / 2);
		glfwSetWindowSizeCallback( window, Events::resize );
		glfwSetKeyCallback( window, Events::key_cb );
		glfwSetCursorPosCallback( window, Events::motion );

		while ( !glfwWindowShouldClose( window ) )
		{
			Events::idle();
			display();
		}

		std::cout << "[INFO] Exited the loop" << std::endl;
	}

	// -- Join all threads into the main again
	g_finished = true;
	this->threads.find( "chunks" )->second.join();

	std::cout << "[INFO] Threads have joined" << std::endl;

	// -- Free the resources
	if ( world != nullptr )
	{
		delete world;
	}
	std::cout << "[INFO] Resources freed" << std::endl;

	return 0;
}

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#if defined(NDEBUG) && defined(_MSC_VER)
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, LPTSTR lpszCmdLine, int nCmdShow)
#else
int main(int argc, char* argv[])
#endif
{
#if defined(_DEBUG) && defined(_MSC_VER)
	_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF);
#endif

	int r = 0;

	try
	{
		LostInWilderness game;
		r = game.run();
	}
	catch ( std::exception& e )
	{
		std::cerr << "=================================================\n";
		std::cerr << "Fatal Exception:\n" << e.what() << std::endl;
	}

	system("pause");

	return r;
}
