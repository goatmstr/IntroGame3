#define LOG(argument) std::cout << argument << '\n'
#define STB_IMAGE_IMPLEMENTATION
#define GL_SILENCE_DEPRECATION
#define GL_GLEXT_PROTOTYPES 1
#define NUMBER_OF_ENEMIES 3
#define FIXED_TIMESTEP 0.0166666f
#define ACC_OF_GRAVITY -1.0f
#define BLOCK_COUNT 13

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include "Entity.h"

// ����� STRUCTS AND ENUMS �����//
struct GameState
{
    Entity* player;
    Entity* world;
    Entity* winscreen;
    Entity* losescreen;
    bool win_game = false;
    bool lose_game = false;
};

// ����� CONSTANTS ����� //
const int WINDOW_WIDTH = 640,
WINDOW_HEIGHT = 480;

const float BG_RED = 0.1922f,
BG_BLUE = 0.549f,
BG_GREEN = 0.9059f,
BG_OPACITY = 1.0f;

const int VIEWPORT_X = 0,
VIEWPORT_Y = 0,
VIEWPORT_WIDTH = WINDOW_WIDTH,
VIEWPORT_HEIGHT = WINDOW_HEIGHT;

const char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

const float MILLISECONDS_IN_SECOND = 1000.0;
const char  SPRITESHEET_FILEPATH[] = "assets/george_0.png";
const char  LOSESCREEN_FILEPATH[] = "assets/FAILED.png";
const char  WINSCREEN_FILEPATH[] = "assets/ACCOMPLISHED.png";
const char  WINBLOCK_FILEPATH[] = "assets/grass.png";
const char  LOSEBLOCK_FILEPATH[] = "assets/lava.png";

const int NUMBER_OF_TEXTURES = 1;  // to be generated, that is
const GLint LEVEL_OF_DETAIL = 0;  // base image level; Level n is the nth mipmap reduction image
const GLint TEXTURE_BORDER = 0;  // this value MUST be zero

// ������VARIABLES ����� //
GameState g_game_state;

SDL_Window* g_display_window;
bool g_game_is_running = true;

ShaderProgram g_shader_program;
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks = 0.0f;
float g_time_accumulator = 0.0f;

// ���� GENERAL FUNCTIONS ���� //
GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);

    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }

    GLuint textureID;
    glGenTextures(NUMBER_OF_TEXTURES, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(image);

    return textureID;
}

void initialise()
{
    SDL_Init(SDL_INIT_VIDEO);
    g_display_window = SDL_CreateWindow("George Lands in Minecraft",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);

#ifdef _WINDOWS
    glewInit();
#endif

    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    g_shader_program.Load(V_SHADER_PATH, F_SHADER_PATH);

    g_view_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

    g_shader_program.SetProjectionMatrix(g_projection_matrix);
    g_shader_program.SetViewMatrix(g_view_matrix);

    glUseProgram(g_shader_program.programID);

    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);

    g_game_state.winscreen = new Entity[3];
    g_game_state.losescreen = new Entity[3];
    g_game_state.winscreen[0].set_position(glm::vec3(0.0f, 0.0f, 0.0f));
    
    g_game_state.winscreen->m_texture_id = load_texture(WINSCREEN_FILEPATH);

    g_game_state.losescreen[0].set_position(glm::vec3(0.0f, 0.0f, 0.0f));
    
    g_game_state.losescreen->m_texture_id = load_texture(LOSESCREEN_FILEPATH);

    // ����� SHIP ����� //
    g_game_state.player = new Entity();
    g_game_state.player->set_position(glm::vec3(0.0f, 3.5f, 0.0f));
    g_game_state.player->set_movement(glm::vec3(0.0f));
    g_game_state.player->set_acceleration(glm::vec3(0.0f, ACC_OF_GRAVITY * 0.1, 0.0f));
    g_game_state.player->set_speed(0.0f);
    g_game_state.player->m_texture_id = load_texture(SPRITESHEET_FILEPATH);

    g_game_state.player->m_walking[g_game_state.player->LEFT] = new int[4] { 1, 5, 9, 13 };
    g_game_state.player->m_walking[g_game_state.player->RIGHT] = new int[4] { 3, 7, 11, 15 };
    g_game_state.player->m_walking[g_game_state.player->UP] = new int[4] { 2, 6, 10, 14 };
    g_game_state.player->m_walking[g_game_state.player->DOWN] = new int[4] { 0, 4, 8, 12 };

    g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->RIGHT];  // start George looking right
    g_game_state.player->m_animation_frames = 4;
    g_game_state.player->m_animation_index = 0;
    g_game_state.player->m_animation_time = 0.0f;
    g_game_state.player->m_animation_cols = 4;
    g_game_state.player->m_animation_rows = 4;

    // ����� WORLD ����� //
    g_game_state.world = new Entity[BLOCK_COUNT];

    for (int i = 0; i < 11; i++)
    {
        g_game_state.world[i].set_position(glm::vec3(i - 5.0f, -4.0f, 0.0f));
        g_game_state.world[i].m_texture_id = load_texture(LOSEBLOCK_FILEPATH);
        g_game_state.world[i].win_block = false;
        g_game_state.world[i].update(0.0f, NULL, 0);
    }

    for (int i = 11; i < 13; i++) {
        g_game_state.world[i].win_block = true;
    }

    g_game_state.world[11].set_position(glm::vec3(-4.0f, -3.0f, 0.0f));
    g_game_state.world[11].m_texture_id = load_texture(WINBLOCK_FILEPATH);
    g_game_state.world[11].update(0.0f, NULL, 0);

    g_game_state.world[12].set_position(glm::vec3(4.0f, -3.0f, 0.0f));
    g_game_state.world[12].m_texture_id = load_texture(WINBLOCK_FILEPATH);
    g_game_state.world[12].update(0.0f, NULL, 0);

    



    // ����� GENERAL ����� //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    // VERY IMPORTANT: If nothing is pressed, we don't want to go anywhere
    g_game_state.player->set_movement(glm::vec3(0.0f));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
            // End game
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_game_is_running = false;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:
                // Quit the game with a keystroke
                g_game_is_running = false;
                break;

            default:
                break;
            }

        default:
            break;
        }
    }

    const Uint8* key_state = SDL_GetKeyboardState(NULL);

    if (key_state[SDL_SCANCODE_LEFT])
    {
        g_game_state.player->move_left();
        g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->LEFT];
    }
    else if (key_state[SDL_SCANCODE_RIGHT])
    {
        g_game_state.player->move_right();
        g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->RIGHT];
    }

    // This makes sure that the player can't move faster diagonally
    if (glm::length(g_game_state.player->get_movement()) > 1.0f)
    {
        g_game_state.player->set_movement(glm::normalize(g_game_state.player->get_movement()));
    }
}

void update()
{
    // ����� DELTA TIME ����� //
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND; // get the current number of ticks
    float delta_time = ticks - g_previous_ticks; // the delta time is the difference from the last frame
    g_previous_ticks = ticks;

    // ����� FIXED TIMESTEP ����� //
    // STEP 1: Keep track of how much time has passed since last step
    delta_time += g_time_accumulator;

    // STEP 2: Accumulate the ammount of time passed while we're under our fixed timestep
    if (delta_time < FIXED_TIMESTEP)
    {
        g_time_accumulator = delta_time;
        return;
    }

    // STEP 3: Once we exceed our fixed timestep, apply that elapsed time into the objects' update function invocation
    while (delta_time >= FIXED_TIMESTEP)
    {
        // Notice that we're using FIXED_TIMESTEP as our delta time
        int i = g_game_state.player->update(FIXED_TIMESTEP, g_game_state.world, BLOCK_COUNT);
        if (i > -1) {
            if (g_game_state.world[i].win_block) {
                g_game_state.win_game = true;
            }
            else {
                g_game_state.lose_game = true;
            }
        }
        if (i == -99) {
            g_game_state.lose_game = true;
        }

        delta_time -= FIXED_TIMESTEP;
    }

    g_time_accumulator = delta_time;
}

void render()
{
    // ����� GENERAL ����� //
    glClear(GL_COLOR_BUFFER_BIT);

    // ����� PLAYER ����� //
    g_game_state.player->render(&g_shader_program);

    // ����� PLATFORM ����� //
    for (int i = 0; i < BLOCK_COUNT; i++) g_game_state.world[i].render(&g_shader_program);
    // ----- WIN/LOSE ----- //
    if (g_game_state.win_game) {
        g_game_state.winscreen[0].render(&g_shader_program);
        
    }
    else if (g_game_state.lose_game) {
        g_game_state.losescreen[0].render(&g_shader_program);
        
    }
    // ����� GENERAL ����� //
    SDL_GL_SwapWindow(g_display_window);
}

void shutdown() { SDL_Quit(); }

// ������DRIVER GAME LOOP ����� /
int main(int argc, char* argv[])
{
    initialise();

    while (g_game_is_running)
    {
        process_input();
        update();
        render();
    }

    shutdown();
    return 0;
}