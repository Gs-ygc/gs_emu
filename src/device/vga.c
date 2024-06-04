/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <common.h>
#include <device/map.h>
#include <stdio.h>
#include <string.h>


#ifdef CONFIG_VGA_SIZE_1600x1200
#define SCREEN_W 1600
#define SCREEN_H 1200
#else
#define SCREEN_W (MUXDEF(CONFIG_VGA_SIZE_800x600, 800, 400))
#define SCREEN_H (MUXDEF(CONFIG_VGA_SIZE_800x600, 600, 300))
#endif

static uint32_t screen_width() {
  return MUXDEF(CONFIG_TARGET_AM, io_read(AM_GPU_CONFIG).width, SCREEN_W);
}

static uint32_t screen_height() {
  return MUXDEF(CONFIG_TARGET_AM, io_read(AM_GPU_CONFIG).height, SCREEN_H);
}

static uint32_t screen_size() {
  return screen_width() * screen_height() * sizeof(uint32_t);
}

static void *vmem = NULL;
static uint32_t *vgactl_port_base = NULL;

#ifdef CONFIG_VGA_SHOW_SCREEN
#ifndef CONFIG_TARGET_AM
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
//将要用到的字体
int FONT_SIZE=16;
TTF_Font *font = NULL;
char *font_file="/usr/local/share/fonts/nerd-fonts/terminal-fonts/fonts/Meslo/Meslo LG M Regular Nerd Font Complete Mono Windows Compatible.ttf";
//字体的颜色
SDL_Color textColor = { 255, 255, 255, 255 };

static void init_screen() {
  SDL_Window *window = NULL;
  char title[128];
  sprintf(title, "%s-NEMU", str(__GUEST_ISA__));
  SDL_Init(SDL_INIT_VIDEO);
  SDL_CreateWindowAndRenderer(
      SCREEN_W * (MUXDEF(CONFIG_VGA_SIZE_400x300, 2, 1)),
      SCREEN_H * (MUXDEF(CONFIG_VGA_SIZE_400x300, 2, 1)),
      0, &window, &renderer);
  SDL_SetWindowTitle(window, title);
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
      SDL_TEXTUREACCESS_STATIC, SCREEN_W, SCREEN_H);
  SDL_RenderPresent(renderer);

  if (TTF_Init() == -1) {
        fprintf(stderr, "Could not initialize TTF: %s\n", TTF_GetError());
  }
  
  font = TTF_OpenFont( font_file, FONT_SIZE );
  if (!font) {
      fprintf(stderr, "Could not open font: %s\n", TTF_GetError());
  }
}
#include <pthread.h>

pthread_t update_theads=0;
// 帧率统计变量
int frame_count = 0;

static inline void *update_screen(void *arg) {
    SDL_UpdateTexture(texture, NULL, vmem, SCREEN_W * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    return 0;
  // // update_screen();
  // frame_count=0;
  // // 统计帧数
  
}
#else
static void init_screen() {}

static inline void update_screen() {
  io_write(AM_GPU_FBDRAW, 0, 0, vmem, screen_width(), screen_height(), true);
}
#endif
#endif



void vga_update_screen() {
  // TODO: call `update_screen()` when the sync register is non-zero,
  // then zero out the sync register
  frame_count++;
  if(frame_count <=24) return;
  frame_count=0;
  update_screen(NULL);
  // 获取当前帧的开始时间
  
}

void render_text_to_vmem(const char *text, TTF_Font *font, SDL_Color color, int x, int y) {
    SDL_Surface *text_surface = TTF_RenderText_Blended_Wrapped(font, text, color,700);
    if (text_surface == NULL) {
        fprintf(stderr, "Failed to render text: %s\n", TTF_GetError());
        return;
    }

    // 确保 SDL_Surface 的格式与 vmem 的格式一致
    SDL_PixelFormat *format = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
    SDL_Surface *converted_surface = SDL_ConvertSurface(text_surface, format, 0);
    SDL_FreeSurface(text_surface); // 不再需要原始的 surface
    SDL_FreeFormat(format);

    if (converted_surface == NULL) {
        fprintf(stderr, "Failed to convert surface: %s\n", SDL_GetError());
        return;
    }

    // 将转换后的文本渲染结果拷贝到 vmem 中
    uint32_t *vmem32 = (uint32_t *)vmem;
    for (int i = 0; i < converted_surface->h; ++i) {
        for (int j = 0; j < converted_surface->w; ++j) {
            int vmem_x = x + j;
            int vmem_y = y + i;
            if (vmem_x >= 0 && vmem_x < SCREEN_W && vmem_y >= 0 && vmem_y < SCREEN_H) {
                uint32_t *pixels = (uint32_t *)converted_surface->pixels;
                uint32_t pixel = pixels[i * converted_surface->w + j];
                uint8_t alpha = (pixel >> 24) & 0xFF;
                if (alpha > 0) {
                    vmem32[vmem_y * SCREEN_W + vmem_x] = pixel;
                }
            }
        }
    }

    SDL_FreeSurface(converted_surface);
}

void init_vga() {
  vgactl_port_base = (uint32_t *)new_space(8);
  vgactl_port_base[0] = (screen_width() << 16) | screen_height();
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map ("vgactl", CONFIG_VGA_CTL_PORT, vgactl_port_base, 8, NULL);
#else
  add_mmio_map("vgactl", CONFIG_VGA_CTL_MMIO, vgactl_port_base, 8, NULL);
#endif

  vmem = new_space(screen_size());
  add_mmio_map("vmem", CONFIG_FB_ADDR, vmem, screen_size(), NULL);
  IFDEF(CONFIG_VGA_SHOW_SCREEN, init_screen());
  IFDEF(CONFIG_VGA_SHOW_SCREEN, memset(vmem, 0, screen_size()));

  char *input_test="Hello world\n";
  render_text_to_vmem(input_test, font, textColor, 100, 100);
  update_screen(NULL);

}
