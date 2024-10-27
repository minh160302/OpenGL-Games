#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstdint>
#include <cstdio>

void error_callback(int error, const char *description) {
  fprintf(stderr, "Error: %s\n", description);
}

//* Structs */
struct Buffer {
  size_t width, height;
  uint32_t *data;
};

struct Sprite {
  size_t width, height;
  uint8_t *data;
};

struct Alien {
  size_t x, y;
  uint8_t type;
};

struct Player {
  size_t x, y;
  size_t life;
};

struct Game {
  size_t width, height;
  size_t num_aliens;
  Alien *aliens;
  Player player;
};

struct SpriteAnimation {
  bool loop;
  size_t num_frames;
  size_t frame_duration;
  size_t time;
  Sprite **frames;
};

//** Helper Functions */
void buffer_sprite_draw(Buffer *buffer, const Sprite &sprite, size_t x,
                        size_t y, uint32_t color) {
  for (size_t xi = 0; xi < sprite.width; ++xi) {
    for (size_t yi = 0; yi < sprite.height; ++yi) {
      size_t sy = sprite.height - 1 + y - yi;
      size_t sx = x + xi;
      if (sprite.data[yi * sprite.width + xi] && sy < buffer->height &&
          sx < buffer->width) {
        buffer->data[sy * buffer->width + sx] = color;
      }
    }
  }
}

bool sprite_overlap_check(const Sprite &sp_a, size_t x_a, size_t y_a,
                          const Sprite &sp_b, size_t x_b, size_t y_b) {
  // NOTE: For simplicity we just check for overlap of the sprite
  // rectangles. Instead, if the rectangles overlap, we should
  // further check if any pixel of sprite A overlap with any of
  // sprite B.
  if (x_a < x_b + sp_b.width && x_a + sp_a.width > x_b &&
      y_a < y_b + sp_b.height && y_a + sp_a.height > y_b) {
    return true;
  }

  return false;
}

void validate_shader(GLuint shader, const char *file = 0) {
  static const unsigned int BUFFER_SIZE = 512;
  char buffer[BUFFER_SIZE];
  GLsizei length = 0;

  glGetShaderInfoLog(shader, BUFFER_SIZE, &length, buffer);

  if (length > 0) {
    printf("Shader %d(%s) compile error: %s\n", shader, (file ? file : ""),
           buffer);
  }
}

bool validate_program(GLuint program) {
  static const GLsizei BUFFER_SIZE = 512;
  GLchar buffer[BUFFER_SIZE];
  GLsizei length = 0;

  glGetProgramInfoLog(program, BUFFER_SIZE, &length, buffer);

  if (length > 0) {
    printf("Program %d link error: %s\n", program, buffer);
    return false;
  }

  return true;
}

uint32_t rgb_to_uint32(uint8_t r, uint8_t g, uint8_t b) {
  return (r << 24) | (g << 16) | (b << 8) | 255;
}

void buffer_clear(Buffer *buffer, uint32_t color) {
  for (size_t i = 0; i < buffer->width * buffer->height; ++i) {
    buffer->data[i] = color;
  }
}

//** Main */
int main(int argc, char const *argv[]) {
  const size_t buffer_width = 224;
  const size_t buffer_height = 256;

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
  window = glfwCreateWindow(buffer_width, buffer_height, "Title", NULL, NULL);
  if (!window) {
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);

  GLenum err = glewInit();
  if (err != GLEW_OK) {
    fprintf(stderr, "Error initializing GLEW.\n");
    glfwTerminate();
    return -1;
  }

  // Call OpenGL
  int glVersion[2] = {-1, 1};
  glGetIntegerv(GL_MAJOR_VERSION, &glVersion[0]);
  glGetIntegerv(GL_MAJOR_VERSION, &glVersion[1]);

  printf("Using OpenGL: %d.%d\n", glVersion[0], glVersion[1]);
  printf("Renderer used: %s\n", glGetString(GL_RENDERER));
  printf("Shading Language: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

  glClearColor(1.0, 0.0, 0.0, 1.0);

  //* Graphics buffer */
  Buffer buffer;
  buffer.width = buffer_width;
  buffer.height = buffer_height;
  buffer.data = new uint32_t[buffer.width * buffer.height];

  buffer_clear(&buffer, 0);

  //* Texture for presenting buffer to OpenGL */
  GLuint buffer_texture;
  glGenTextures(1, &buffer_texture);
  // specify image format and standard parameters
  glBindTexture(GL_TEXTURE_2D, buffer_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, buffer.width, buffer.height, 0,
               GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, buffer.data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // vertex array object (VAO) for generating fullscreen triangle
  GLuint fullscreen_triangle_vao;
  glGenVertexArrays(1, &fullscreen_triangle_vao);
  // glBindVertexArray(fullscreen_triangle_vao);

  // Shaders for displaying buffers
  const char *fragment_shader =
      "\n"
      "#version 330\n"
      "\n"
      "uniform sampler2D buffer;\n"
      "noperspective in vec2 TexCoord;\n"
      "\n"
      "out vec3 outColor;\n"
      "\n"
      "void main(void){\n"
      "    outColor = texture(buffer, TexCoord).rgb;\n"
      "}\n";

  const char *vertex_shader =
      "\n"
      "#version 330\n"
      "\n"
      "noperspective out vec2 TexCoord;\n"
      "\n"
      "void main(void){\n"
      "\n"
      "    TexCoord.x = (gl_VertexID == 2)? 2.0: 0.0;\n"
      "    TexCoord.y = (gl_VertexID == 1)? 2.0: 0.0;\n"
      "    \n"
      "    gl_Position = vec4(2.0 * TexCoord - 1.0, 0.0, 1.0);\n"
      "}\n";

  // Compile 2 shaders into code the GPU can understand and linked into a shader
  // program
  GLuint shader_id = glCreateProgram();
  // Create vertex shader
  {
    GLuint shader_vp = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(shader_vp, 1, &vertex_shader, 0);
    glCompileShader(shader_vp);
    validate_shader(shader_vp, vertex_shader);
    glAttachShader(shader_id, shader_vp);

    glDeleteShader(shader_vp);
  }
  // Create fragment shader
  {
    GLuint shader_fp = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(shader_fp, 1, &fragment_shader, 0);
    glCompileShader(shader_fp);
    validate_shader(shader_fp, fragment_shader);
    glAttachShader(shader_id, shader_fp);

    glDeleteShader(shader_fp);
  }

  glLinkProgram(shader_id);

  if (!validate_program(shader_id)) {
    fprintf(stderr, "Error while validating shader.\n");
    glfwTerminate();
    glDeleteVertexArrays(1, &fullscreen_triangle_vao);
    delete[] buffer.data;
    return -1;
  }

  glUseProgram(shader_id);

  GLint location = glGetUniformLocation(shader_id, "buffer");
  glUniform1i(location, 0);

  // OpenGL setup for Buffer Display
  glDisable(GL_DEPTH_TEST);
  glBindVertexArray(fullscreen_triangle_vao);

  //* Game */

  // Sprite bitmap
  Sprite alien_sprite;
  alien_sprite.width = 11;
  alien_sprite.height = 8;
  alien_sprite.data = new uint8_t[11 * 8]{
      0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, // ..@.....@..
      0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, // ...@...@...
      0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, // ..@@@@@@@..
      0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0, // .@@.@@@.@@.
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // @@@@@@@@@@@
      1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, // @.@@@@@@@.@
      1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, // @.@.....@.@
      0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0  // ...@@.@@...
  };

  Sprite alien_sprite_alt; // alternate bitmap for animation
  alien_sprite_alt.width = 11;
  alien_sprite_alt.height = 8;
  alien_sprite_alt.data = new uint8_t[88]{
      0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, // ..@.....@..
      1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, // @..@...@..@
      1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, // @.@@@@@@@.@
      1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, // @@@.@@@.@@@
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // @@@@@@@@@@@
      0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, // .@@@@@@@@@.
      0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, // ..@.....@..
      0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0  // .@.......@.
  };

  // Player bitmap
  Sprite player_sprite;
  player_sprite.width = 11;
  player_sprite.height = 7;
  player_sprite.data = new uint8_t[11 * 7]{
      0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, // .....@.....
      0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, // ....@@@....
      0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, // ....@@@....
      0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, // .@@@@@@@@@.
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // @@@@@@@@@@@
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // @@@@@@@@@@@
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // @@@@@@@@@@@
  };

  Game game;

  game.width = buffer.width;
  game.height = buffer.height;
  game.num_aliens = 60;
  game.aliens = new Alien[game.num_aliens];

  game.player.x = buffer.width / 2;
  game.player.y = 32;
  game.player.life = 3;

  // Position the aliens
  // TODO: customize this for multiple stages
  for (size_t yi = 0; yi < 5; ++yi) {
    for (size_t xi = 0; xi < 12; ++xi) {
      game.aliens[yi * 12 + xi].x = 16 * xi + 20;
      game.aliens[yi * 12 + xi].y = 17 * yi + 128;
    }
  }

  //* Animation */
  SpriteAnimation *alien_animation = new SpriteAnimation;

  alien_animation->loop = true;
  alien_animation->num_frames = 2;
  alien_animation->frame_duration = 10;
  alien_animation->time = 0;

  alien_animation->frames = new Sprite *[2];
  alien_animation->frames[0] = &alien_sprite;
  alien_animation->frames[1] = &alien_sprite_alt;

  // V-sync mode on
  // https://www.glfw.org/docs/latest/group__context.html#ga6d4e0cdf151b5e579bd67f13202994ed
  glfwSwapInterval(1);

  uint32_t clear_color = rgb_to_uint32(0, 128, 0);

  int player_move_dir = 1;

  while (!glfwWindowShouldClose(window)) {
    //  double buffering scheme; the "front" buffer is used for displaying an
    //  image, while the "back" buffer is used for drawing

    buffer_clear(&buffer, clear_color);

    // Draw everything
    for (size_t ai = 0; ai < game.num_aliens; ++ai) {
      const Alien &alien = game.aliens[ai];
      size_t current_frame =
          alien_animation->time / alien_animation->frame_duration;
      const Sprite &sprite = *alien_animation->frames[current_frame];
      buffer_sprite_draw(&buffer, sprite, alien.x, alien.y,
                         rgb_to_uint32(128, 0, 0));
    }

    buffer_sprite_draw(&buffer, player_sprite, game.player.x, game.player.y,
                       rgb_to_uint32(128, 0, 0));

    // Render animations every iteration
    ++alien_animation->time;
    if (alien_animation->time ==
        alien_animation->num_frames * alien_animation->frame_duration) {
      if (alien_animation->loop) {
        alien_animation->time = 0;
      } else {
        delete alien_animation;
        alien_animation = nullptr;
      }
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, buffer.width, buffer.height,
                    GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, buffer.data);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glfwSwapBuffers(window);

    // Player's move animation
    if (game.player.x + player_sprite.width + player_move_dir >= game.width) {
      game.player.x = game.width - player_sprite.width - player_move_dir;
      player_move_dir *= -1;
    } else if (game.player.x + player_move_dir <= 0) {
      game.player.x = 0;
      player_move_dir *= -1;
    } else {
      game.player.x += player_move_dir;
    }

    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();

  glDeleteVertexArrays(1, &fullscreen_triangle_vao);

  delete[] alien_sprite.data;
  delete[] buffer.data;

  return 0;
}
