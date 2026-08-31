#include "camera_info.h"
#include "keyboard.h"
#include "lang.h"
#include "conf.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_lang.h"
#include "gui_batt.h"
#include "gui_mbox.h"
#include "modes.h"

#include "module_def.h"

void gui_game_menu_kbd_process();
int gui_snake_kbd_process();
void gui_snake_draw();

gui_handler GUI_MODE_SNAKE = 
    /*GUI_MODE_SNAKE*/  { GUI_MODE_MODULE, gui_snake_draw, gui_snake_kbd_process, gui_game_menu_kbd_process, 0, GUI_MODE_FLAG_NODRAWRESTORE };

//-------------------------------------------------------------------
int snake_scale;
int snake_screen_size;
int screen_left;
int screen_top;

#define SNAKE_MAX_LENGTH 0xF0
#define SNAKE_ELEMENT_SIZE 8
#define RING_WIDTH 40
#define RING_HEIGHT 24
enum{
  DIR_LEFT = 0,
  DIR_DOWN,
  DIR_UP,
  DIR_RIGHT,
};
unsigned char ring[RING_WIDTH][RING_HEIGHT];
unsigned short snake_head;
unsigned short snake_tail;
unsigned char snake[SNAKE_MAX_LENGTH][2];
unsigned int points;
unsigned char level;
unsigned char direction;
char str_buf[40];

const unsigned char labyrinth[4][RING_HEIGHT][RING_WIDTH] = {
    {
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     },
    {
     "########################################",
     "#                                      #",
     "#                                      #",
     "#                                      #",
     "#     ############                     #",
     "#                #                     #",
     "#                #                     #",
     "#                #######################",
     "#                #                     #",
     "#                #                     #",
     "#                #                     #",
     "#                       ######         #",
     "#                       #    #         #",
     "#            ############    #         #",
     "#           #                #         #",
     "#          #                 #         #",
     "#         #                  #         #",
     "#        #                   #         #",
     "#       #                    #         #",
     "#       #                    #         #",
     "#                            #         #",
     "#                            #         #",
     "#                            #         #",
     "########################################",
     },

    {
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                ##########              ",
     "                         #              ",
     "                         #              ",
     "                         #              ",
     "                  ########              ",
     "                  #                     ",
     "                  #                     ",
     "                  #                     ",
     "                  #                     ",
     "                                        ",
     "                  #                     ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     "                                        ",
     },

    {
     "                                        ",
     "                  #                     ",
     "                  #                     ",
     "                  #                     ",
     "     ##########################         ",
     "                           #            ",
     "                           #            ",
     "            ##########     #            ",
     "    #                      ####         ",
     "    #                                   ",
     "    #                                   ",
     "    #                                   ",
     "    #                                   ",
     "    #                                   ",
     "    #################           #       ",
     "                                #       ",
     "            #                   #       ",
     "     ########                   #       ",
     "                                #       ",
     "       ##########################       ",
     "                                #       ",
     "                                #       ",
     "                                #       ",
     "                                        ",
     },
};

const char head[8][8] = {
  "     GG ",
  "    GBBG",
  "    G BG",
  " GGGGGGG",
  "RGGGGGGG",
  " GGGGGGG",
  "        ",
  "        ",
};

const char tail[8][8] = {
  "        ",
  "        ",
  "        ",
  "G GGG GG",
  "GGGGGGGG",
  "GGG GGG ",
  "        ",
  "        ",
};

const char tail2[8][8] = {
  "        ",
  "        ",
  "        ",
  " GG     ",
  "GGGG    ",
  "GGGGG   ",
  "  GGG   ",
  "  GG    ",
};

const char apple[8][8] = {
  "        ",
  " GGGB   ",
  "GGG B   ",
  " G RRR  ",
  "  RRRRR ",
  "  RRRRR ",
  "  RRRRR ",
  "   RRR  ",
};

const char wall[8][8] = {
  "OOOOO OO",
  "OOOOO OO",
  "        ",
  "OO OOOOO",
  "OO OOOOO",
  "        ",
  "OOOOO OO",
  "OOOOO OO",
};

static void draw_element(int x,int y,const char element[8][8],char angle){
  x = screen_left + x * snake_screen_size;
  y = screen_top + y * snake_screen_size;
  int xx,yy,px,py;
  for(xx=0;xx<SNAKE_ELEMENT_SIZE;xx++)
    for(yy=0;yy<SNAKE_ELEMENT_SIZE;yy++){
      char c = COLOR_RED;
      char col = 0;

      if(angle == 0) col = element[yy][xx];
      if(angle == 1) col = element[xx][7-yy];
      if(angle == 2) col = element[xx][yy];
      if(angle == 3) col = element[yy][7-xx];
      if(angle == 4) col = element[7-xx][yy];
      switch(col){
        case 'R': c = COLOR_RED; break;
        case 'G': c = COLOR_GREEN; break;
        case 'B': c = COLOR_BLACK; break;
        case 'b': c = COLOR_BLUE; break;
        case 'Y': c = COLOR_YELLOW; break;
        case 'O': c = 0x28; break;
        default: c = COLOR_WHITE; break;
      };
      for (px=xx*snake_scale; px<(xx+1)*snake_scale; px+=1)
          for (py=yy*snake_scale; py<(yy+1)*snake_scale; py+=1)
              draw_pixel( x+px, y+py, c );
    }
}



static void load_laby(int num){
  int x,y;
  for(x=0;x<RING_WIDTH;x++)
    for(y=0;y<RING_HEIGHT;y++){
      ring[x][y] = labyrinth[num][y][x];
      if(ring[x][y] != ' '){
        draw_element(x,y,wall,0);
      }else{
        draw_rectangle(screen_left + x * snake_screen_size,
                       screen_top + y * snake_screen_size,
                       screen_left + x * snake_screen_size+snake_screen_size-1,
                       screen_top + y * snake_screen_size+snake_screen_size-1, MAKE_COLOR(COLOR_WHITE,COLOR_WHITE), RECT_BORDER0|DRAW_FILLED);
      }
    }
}

static void snake_generate_apple()
{
  int x,y,i;
  while(1)
  {
    randomagain:
    x = rand() % RING_WIDTH;
    y = rand() % RING_HEIGHT;
    if(ring[x][y] != ' ') continue;
    i = snake_tail;
    while(1){
      if(snake[i][0] == x && snake[i][1] == y) goto randomagain;
      if(i == snake_head) break;
      i = (i+1)%SNAKE_MAX_LENGTH;
    }
    draw_element(x,y,apple,0);
    ring[x][y] = 1;
    break;
  }
}

static void snake_start()
{
  int i;
  load_laby(rand()%4);
  snake_head = 2;
  snake_tail = 0;
  level = 5;
  points = 0;
  for(i=snake_tail;i<=snake_head;i++){
    snake[i][0] = 2+i;
    snake[i][1] = 2;
    draw_rectangle(screen_left + snake[i][0] * snake_screen_size,
                   screen_top + snake[i][1] * snake_screen_size,
                   screen_left + snake[i][0] * snake_screen_size+snake_screen_size-1,
                   screen_top + snake[i][1] * snake_screen_size+snake_screen_size-1, MAKE_COLOR(COLOR_WHITE,COLOR_RED), RECT_BORDER1|DRAW_FILLED);
  }
  direction = DIR_RIGHT;
  for(i=0;i<10;i++)
    snake_generate_apple();
}

static void game_over()
{
    draw_rectangle(camera_screen.disp_left,0,camera_screen.disp_right,camera_screen.height-1, MAKE_COLOR(COLOR_GREY,COLOR_GREY), RECT_BORDER0|DRAW_FILLED);
    sprintf(str_buf,lang_str(LANG_SNAKE_POINTS),points);
    draw_string(camera_screen.disp_width / 2 - strlen(str_buf)*FONT_WIDTH/2,camera_screen.height/2-FONT_HEIGHT/2,str_buf, MAKE_COLOR(COLOR_GREY, COLOR_BLUE));
    msleep(3000);
    snake_start();
}

int gui_snake_kbd_process()
{
    switch (kbd_get_autoclicked_key()) {
        case KEY_UP:
            if(direction != DIR_DOWN) direction = DIR_UP;
            break;
        case KEY_DOWN:
            if(direction != DIR_UP) direction = DIR_DOWN;
            break;
        case KEY_LEFT:
                if(direction != DIR_RIGHT) direction = DIR_LEFT;
            break;
        case KEY_RIGHT:
                if(direction != DIR_LEFT) direction = DIR_RIGHT;
            break;
        case KEY_SET:
             
             
            break;
        case KEY_ERASE:
        case KEY_DISPLAY:
            break;
    }
    return 0;
}

void gui_snake_draw()
{
    unsigned short new_head;
    unsigned short new_tail;
    unsigned short new_cord;
    char game_over_flag = 0, draw_head, clear_tail, draw_points = 1;
    unsigned char prevdir = direction;
    unsigned int i;
    new_head = (snake_head + 1) % SNAKE_MAX_LENGTH;
    new_tail = (snake_tail + 1) % SNAKE_MAX_LENGTH;
    draw_head = 1;
    clear_tail = 1;
    if (direction == DIR_RIGHT)
    {
        new_cord = (snake[snake_head][0] + 1) % RING_WIDTH;
        snake[new_head][0] = new_cord;
        snake[new_head][1] = snake[snake_head][1];
    }
    else if (direction == DIR_LEFT)
    {
        new_cord = snake[snake_head][0] - 1;
        if (new_cord == 0xFFFF)
            new_cord = RING_WIDTH - 1;
        snake[new_head][0] = new_cord;
        snake[new_head][1] = snake[snake_head][1];
    }
    else if (direction == DIR_UP)
    {
        new_cord = snake[snake_head][1] - 1;
        if (new_cord == 0xFFFF)
            new_cord = RING_HEIGHT - 1;
        snake[new_head][0] = snake[snake_head][0];
        snake[new_head][1] = new_cord;
    }
    else if (direction == DIR_DOWN)
    {
        new_cord = (snake[snake_head][1] + 1) % RING_HEIGHT;
        snake[new_head][0] = snake[snake_head][0];
        snake[new_head][1] = new_cord;
    }

    i = snake_tail;
    while (1)
    {
        if (snake[i][0] == snake[new_head][0] && snake[i][1] == snake[new_head][1])
        {
            game_over_flag = 1;
            break;
        }
        if (i == snake_head)
            break;
        i = (i + 1) % SNAKE_MAX_LENGTH;
    }

    if (ring[snake[new_head][0]][snake[new_head][1]] == 1)
    {
        ring[snake[new_head][0]][snake[new_head][1]] = ' ';
        snake_generate_apple();
        points += level;
        draw_points = 1;
        if ((new_head + 1) % SNAKE_MAX_LENGTH != snake_tail)
        {
            new_tail = snake_tail;
            clear_tail = 0;
        }
    }
    else if (ring[snake[new_head][0]][snake[new_head][1]] != ' ')
    {
        game_over_flag = 1;
    }
    if (draw_head)
    {
        draw_element(snake[new_head][0], snake[new_head][1], head, direction);

        new_cord = snake_head - 1;
        if (new_cord == 0xFFFF)
            new_cord = SNAKE_MAX_LENGTH - 1;
        if (snake[new_cord][0] > snake[new_head][0] && snake[new_cord][1] > snake[new_head][1] && direction == DIR_UP)
            draw_element(snake[snake_head][0], snake[snake_head][1], tail2, 2);
        else if (snake[new_cord][0] > snake[new_head][0] && snake[new_cord][1] > snake[new_head][1] && direction == DIR_LEFT)
            draw_element(snake[snake_head][0], snake[snake_head][1], tail2, 0);
        else if (snake[new_cord][0] < snake[new_head][0] && snake[new_cord][1] > snake[new_head][1] && direction == DIR_UP)
            draw_element(snake[snake_head][0], snake[snake_head][1], tail2, 4);
        else if (snake[new_cord][0] < snake[new_head][0] && snake[new_cord][1] > snake[new_head][1] && direction == DIR_RIGHT)
            draw_element(snake[snake_head][0], snake[snake_head][1], tail2, 3);
        else if (snake[new_cord][0] < snake[new_head][0] && snake[new_cord][1] < snake[new_head][1] && direction == DIR_RIGHT)
            draw_element(snake[snake_head][0], snake[snake_head][1], tail2, 2);
        else if (snake[new_cord][0] < snake[new_head][0] && snake[new_cord][1] < snake[new_head][1] && direction == DIR_DOWN)
            draw_element(snake[snake_head][0], snake[snake_head][1], tail2, 0);
        else if (snake[new_cord][0] > snake[new_head][0] && snake[new_cord][1] < snake[new_head][1] && direction == DIR_LEFT)
            draw_element(snake[snake_head][0], snake[snake_head][1], tail2, 4);
        else if (snake[new_cord][0] > snake[new_head][0] && snake[new_cord][1] < snake[new_head][1] && direction == DIR_DOWN)
            draw_element(snake[snake_head][0], snake[snake_head][1], tail2, 3);
        else if (prevdir == direction && (direction == DIR_RIGHT || direction == DIR_LEFT))
            draw_element(snake[snake_head][0], snake[snake_head][1], tail, 0);
        else if (prevdir == direction && (direction == DIR_UP || direction == DIR_DOWN))
            draw_element(snake[snake_head][0], snake[snake_head][1], tail, 1);
    }
    prevdir = direction;
    if (clear_tail)
    {
        draw_rectangle(screen_left + snake[snake_tail][0] * snake_screen_size,
                       screen_top + snake[snake_tail][1] * snake_screen_size,
                       screen_left + snake[snake_tail][0] * snake_screen_size + snake_screen_size - 1,
                       screen_top + snake[snake_tail][1] * snake_screen_size + snake_screen_size - 1, MAKE_COLOR(COLOR_WHITE, COLOR_WHITE), RECT_BORDER0 | DRAW_FILLED);
    }
    snake_head = new_head;
    snake_tail = new_tail;
    msleep(100);
    if (game_over_flag)
    {
        game_over();
        game_over_flag = 0;
    }
    if (draw_points)
    {
        draw_points = 0;
        sprintf(str_buf, lang_str(LANG_SNAKE_POINTS), points);
        draw_string(camera_screen.disp_width/2 - strlen(str_buf)*FONT_WIDTH/2, camera_screen.height-FONT_HEIGHT-4, str_buf, MAKE_COLOR(COLOR_GREY, COLOR_BLUE));
    }
}

int gui_snake_init()
{
    snake_scale = camera_screen.height / 240;
    if (snake_scale < 1) snake_scale = 1;
    snake_screen_size = SNAKE_ELEMENT_SIZE * snake_scale;
    screen_left = camera_screen.disp_left + (camera_screen.disp_width - RING_WIDTH * snake_screen_size)/2;
    screen_top = (camera_screen.height - FONT_HEIGHT - 4 - RING_HEIGHT * snake_screen_size)/2;

    draw_rectangle(camera_screen.disp_left,0,camera_screen.disp_right,camera_screen.height-1, MAKE_COLOR(COLOR_GREY,COLOR_GREY), RECT_BORDER0|DRAW_FILLED);
    snake_start();
    gui_set_mode(&GUI_MODE_SNAKE);
    return 1;
}

int basic_module_init() {
  return gui_snake_init();
}

#include "simple_game.c"

/******************** Module Information structure ******************/

ModuleInfo _module_info =
{
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    SIMPLE_MODULE_VERSION,      // Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,         // Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,       // Specify platform dependency

    -LANG_MENU_GAMES_SNAKE,     // Module name
    MTYPE_GAME,

    &_librun.base,

    ANY_VERSION,                // CONF version
    CAM_SCREEN_VERSION,         // CAM SCREEN version
    ANY_VERSION,                // CAM SENSOR version
    ANY_VERSION,                // CAM INFO version

    0,
};

/*************** END OF AUXILARY PART *******************/
