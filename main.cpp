#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstdio>

void error_callback(int error, const char *description) {
  fprintf(stderr, "Error: %s\n", description);
}

int main(int argc, char const *argv[]) {
  glfwSetErrorCallback(error_callback);

  GLFWwindow *window;

  if (!glfwInit()) {
    return -1;
  }

  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  //* Create a windowed-mode window and its OpenGL context */
  window = glfwCreateWindow(640, 480, "Title", NULL, NULL);
  if (!window) {
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);

  // Call OpenGL
  int glVersion[2] = {-1, 1};
  glGetIntegerv(GL_MAJOR_VERSION, &glVersion[0]);
  glGetIntegerv(GL_MAJOR_VERSION, &glVersion[1]);

  printf("Using OpenGL: %d.%d\n", glVersion[0], glVersion[1]);

  glClearColor(1.0, 0.0, 0.0, 1.0);

  while (!glfwWindowShouldClose(window)) {
    //  double buffering scheme; the "front" buffer is used for displaying an
    //  image, while the "back" buffer is used for drawing
    glClear(GL_COLOR_BUFFER_BIT);

    glfwSwapBuffers(window);

    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
