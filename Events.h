#ifndef EVENTS_H
#define EVENTS_H

#include <GLFW/glfw3.h>

class Events
{
    public:
        Events();
        virtual ~Events();
        void resize( GLFWwindow* wnd, int w, int h );
        void key_cb( GLFWwindow* wnd, int key, int scancode, int action, int mods );
        void motion(GLFWwindow* wnd, double x, double y);
        static void update_vectors();
        static void idle();
        static void mouse(int button, int state, int x, int y);

    protected:
    private:
};

#endif // EVENTS_H
