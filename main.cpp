#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstdint>
#include <cstdio>

#define GAME_MAX_BULLETS 128

bool game_running = false;
int move_dir = 0;
bool fire_pressed = 0;

//* Callbacks */
void error_callback(int error, const char *description) {
  fprintf(stderr, "Error: %s\n", description);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods) {
  switch (key) {
  case GLFW_KEY_ESCAPE:
    if (action == GLFW_PRESS)
      game_running = false;
    break;
  case GLFW_KEY_RIGHT:
    if (action == GLFW_PRESS)
      move_dir += 1;
    else if (action == GLFW_RELEASE)
      move_dir -= 1;
    break;
  case GLFW_KEY_LEFT:
    if (action == GLFW_PRESS)
      move_dir -= 1;
    else if (action == GLFW_RELEASE)
      move_dir += 1;
    break;
  case GLFW_KEY_SPACE:
    if (action == GLFW_RELEASE)
      fire_pressed = true;
    break;
  default:
    break;
  }
}

//* Enums */
enum AlienType : uint8_t {
  ALIEN_DEAD = 0,
  ALIEN_TYPE_A = 1,
  ALIEN_TYPE_B = 2,
  ALIEN_TYPE_C = 3
};

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

struct Bullet {
  size_t x, y;
  int dir;
};

struct Game {
  size_t width, height;
  size_t num_aliens;
  size_t num_bullets;
  Alien *aliens;
  Player player;
  Bullet bullets[GAME_MAX_BULLETS];
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
  glfwSetKeyCallback(window, key_callback);

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

  //* Game *//

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

  // Bullet bitmap
  Sprite bullet_sprite;
  bullet_sprite.width = 1;
  bullet_sprite.height = 3;
  bullet_sprite.data = new uint8_t[3]{
      1, // @
      1, // @
      1  // @
  };

  // Alien types
  Sprite alien_sprites[6];

  alien_sprites[0].width = 8;
  alien_sprites[0].height = 8;
  alien_sprites[0].data = new uint8_t[64]{
      0, 0, 0, 1, 1, 0, 0, 0, // ...@@...
      0, 0, 1, 1, 1, 1, 0, 0, // ..@@@@..
      0, 1, 1, 1, 1, 1, 1, 0, // .@@@@@@.
      1, 1, 0, 1, 1, 0, 1, 1, // @@.@@.@@
      1, 1, 1, 1, 1, 1, 1, 1, // @@@@@@@@
      0, 1, 0, 1, 1, 0, 1, 0, // .@.@@.@.
      1, 0, 0, 0, 0, 0, 0, 1, // @......@
      0, 1, 0, 0, 0, 0, 1, 0  // .@....@.
  };

  alien_sprites[1].width = 8;
  alien_sprites[1].height = 8;
  alien_sprites[1].data = new uint8_t[64]{
      0, 0, 0, 1, 1, 0, 0, 0, // ...@@...
      0, 0, 1, 1, 1, 1, 0, 0, // ..@@@@..
      0, 1, 1, 1, 1, 1, 1, 0, // .@@@@@@.
      1, 1, 0, 1, 1, 0, 1, 1, // @@.@@.@@
      1, 1, 1, 1, 1, 1, 1, 1, // @@@@@@@@
      0, 0, 1, 0, 0, 1, 0, 0, // ..@..@..
      0, 1, 0, 1, 1, 0, 1, 0, // .@.@@.@.
      1, 0, 1, 0, 0, 1, 0, 1  // @.@..@.@
  };

  alien_sprites[2].width = 11;
  alien_sprites[2].height = 8;
  alien_sprites[2].data = new uint8_t[88]{
      0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, // ..@.....@..
      0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, // ...@...@...
      0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, // ..@@@@@@@..
      0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0, // .@@.@@@.@@.
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // @@@@@@@@@@@
      1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, // @.@@@@@@@.@
      1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, // @.@.....@.@
      0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0  // ...@@.@@...
  };

  alien_sprites[3].width = 11;
  alien_sprites[3].height = 8;
  alien_sprites[3].data = new uint8_t[88]{
      0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, // ..@.....@..
      1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, // @..@...@..@
      1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, // @.@@@@@@@.@
      1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, // @@@.@@@.@@@
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // @@@@@@@@@@@
      0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, // .@@@@@@@@@.
      0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, // ..@.....@..
      0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0  // .@.......@.
  };

  alien_sprites[4].width = 12;
  alien_sprites[4].height = 8;
  alien_sprites[4].data = new uint8_t[96]{
      0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, // ....@@@@....
      0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, // .@@@@@@@@@@.
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // @@@@@@@@@@@@
      1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, // @@@..@@..@@@
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // @@@@@@@@@@@@
      0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, // ...@@..@@...
      0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, // ..@@.@@.@@..
      1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1  // @@........@@
  };

  alien_sprites[5].width = 12;
  alien_sprites[5].height = 8;
  alien_sprites[5].data = new uint8_t[96]{
      0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, // ....@@@@....
      0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, // .@@@@@@@@@@.
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // @@@@@@@@@@@@
      1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, // @@@..@@..@@@
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // @@@@@@@@@@@@
      0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, // ..@@@..@@@..
      0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, // .@@..@@..@@.
      0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0  // ..@@....@@..
  };

  Sprite alien_death_sprite;
  alien_death_sprite.width = 13;
  alien_death_sprite.height = 7;
  alien_death_sprite.data = new uint8_t[91]{
      0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, // .@..@...@..@.
      0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, // ..@..@.@..@..
      0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, // ...@.....@...
      1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, // @@.........@@
      0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, // ...@.....@...
      0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, // ..@..@.@..@..
      0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0  // .@..@...@..@.
  };

  // Init Game
  Game game;

  game.width = buffer.width;
  game.height = buffer.height;
  game.num_aliens = 60;
  game.num_bullets = 0;
  game.aliens = new Alien[game.num_aliens];

  game.player.x = buffer.width / 2;
  game.player.y = 32;
  game.player.life = 3;

  // Position the aliens
  // TODO: customize this for multiple stages
  for (size_t yi = 0; yi < 5; ++yi) {
    for (size_t xi = 0; xi < 12; ++xi) {
      Alien &alien = game.aliens[yi * 12 + xi];
      alien.type = (5 - yi) / 2 + 1;

      const Sprite &sprite = alien_sprites[2 * (alien.type - 1)];

      alien.x = 16 * xi + 20 + (alien_death_sprite.width - sprite.width) / 2;
      alien.y = 17 * yi + 128;
    }
  }

  //* Animation */
  SpriteAnimation alien_animation[3];

  for (size_t i = 0; i < 3; ++i) {
    alien_animation[i].loop = true;
    alien_animation[i].num_frames = 2;
    alien_animation[i].frame_duration = 10;
    alien_animation[i].time = 0;

    alien_animation[i].frames = new Sprite *[2];
    alien_animation[i].frames[0] = &alien_sprites[2 * i];
    alien_animation[i].frames[1] = &alien_sprites[2 * i + 1];
  }

  // V-sync mode on
  // https://www.glfw.org/docs/latest/group__context.html#ga6d4e0cdf151b5e579bd67f13202994ed
  glfwSwapInterval(1);

  uint32_t clear_color = rgb_to_uint32(0, 128, 0);

  int player_move_dir = 1;

  // Death
  uint8_t *death_counters = new uint8_t[game.num_aliens];
  for (size_t i = 0; i < game.num_aliens; i++) {
    death_counters[i] = 10;
  }

  //* START GAME! */
  game_running = true;
  while (!glfwWindowShouldClose(window) && game_running) {
    buffer_clear(&buffer, clear_color);

    // Draw Aliens
    for (size_t ai = 0; ai < game.num_aliens; ++ai) {
      if (!death_counters[ai])
        continue;

      const Alien &alien = game.aliens[ai];

      if (alien.type == ALIEN_DEAD) {
        buffer_sprite_draw(&buffer, alien_death_sprite, alien.x, alien.y,
                           rgb_to_uint32(128, 0, 0));
      } else {
        const SpriteAnimation &animation = alien_animation[alien.type - 1];
        size_t current_frame = animation.time / animation.frame_duration;
        const Sprite &sprite = *animation.frames[current_frame];
        buffer_sprite_draw(&buffer, sprite, alien.x, alien.y,
                           rgb_to_uint32(128, 0, 0));
      }
    }

    // Draw bullets
    for (size_t bi = 0; bi < game.num_bullets; ++bi) {
      const Bullet &bullet = game.bullets[bi];
      const Sprite &sprite = bullet_sprite;
      buffer_sprite_draw(&buffer, sprite, bullet.x, bullet.y,
                         rgb_to_uint32(128, 0, 0));
    }

    buffer_sprite_draw(&buffer, player_sprite, game.player.x, game.player.y,
                       rgb_to_uint32(128, 0, 0));

    // Render animations every iteration
    for (size_t i = 0; i < 3; ++i) {
      ++alien_animation[i].time;
      if (alien_animation[i].time ==
          alien_animation[i].num_frames * alien_animation[i].frame_duration) {
        alien_animation[i].time = 0;
      }
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, buffer.width, buffer.height,
                    GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, buffer.data);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glfwSwapBuffers(window);

    // Player's move animation
    player_move_dir = 2 * move_dir;

    // Alien simulation
    for (size_t ai = 0; ai < game.num_aliens; ++ai) {
      const Alien &alien = game.aliens[ai];
      if (alien.type == ALIEN_DEAD && death_counters[ai]) {
        --death_counters[ai];
      }
    }

    // Bullets simulation
    for (size_t bi = 0; bi < game.num_bullets;) {
      game.bullets[bi].y += game.bullets[bi].dir;
      if (game.bullets[bi].y >= game.height ||
          game.bullets[bi].y < bullet_sprite.height) {
        game.bullets[bi] = game.bullets[game.num_bullets - 1];
        --game.num_bullets;
        continue;
      }

      // Bullet hit
      for (size_t ai = 0; ai < game.num_aliens; ai++) {
        const Alien &alien = game.aliens[ai];
        if (alien.type == ALIEN_DEAD)
          continue;

        const SpriteAnimation &animation = alien_animation[alien.type - 1];
        size_t current_frame = animation.time / animation.frame_duration;
        const Sprite &alien_sprite = *animation.frames[current_frame];
        bool overlap = sprite_overlap_check(bullet_sprite, game.bullets[bi].x,
                                            game.bullets[bi].y, alien_sprite,
                                            alien.x, alien.y);
        if (overlap) {
          game.aliens[ai].type = ALIEN_DEAD;
          // NOTE: Hack to recenter death sprite
          game.aliens[ai].x -=
              (alien_death_sprite.width - alien_sprite.width) / 2;
          game.bullets[bi] = game.bullets[game.num_bullets - 1];
          --game.num_bullets;
          continue;
        }
      }

      ++bi;
    }

    if (player_move_dir != 0) {
      if (game.player.x + player_sprite.width + player_move_dir >= game.width) {
        game.player.x = game.width - player_sprite.width - player_move_dir;
      } else if (game.player.x + player_move_dir <= 0) {
        game.player.x = 0;
      } else {
        game.player.x += player_move_dir;
      }
    }

    // Process bullet events
    if (fire_pressed && game.num_bullets < GAME_MAX_BULLETS) {
      game.bullets[game.num_bullets].x =
          game.player.x + player_sprite.width / 2;
      game.bullets[game.num_bullets].y = game.player.y + player_sprite.height;
      game.bullets[game.num_bullets].dir = 2;
      ++game.num_bullets;
    }
    fire_pressed = false;

    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();

  glDeleteVertexArrays(1, &fullscreen_triangle_vao);

  for (size_t i = 0; i < 6; ++i) {
    delete[] alien_sprites[i].data;
  }

  delete[] alien_death_sprite.data;

  for (size_t i = 0; i < 3; ++i) {
    delete[] alien_animation[i].frames;
  }
  delete[] buffer.data;
  delete[] game.aliens;
  delete[] death_counters;

  return 0;
}
