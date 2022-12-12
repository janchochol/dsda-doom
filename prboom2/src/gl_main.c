/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *  Copyright 2007 by
 *  Andrey Budko, Roman Marchenko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *
 *---------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gl_opengl.h"

#include "z_zone.h"
#include <math.h>
#include <SDL.h>
#include "doomtype.h"
#include "w_wad.h"
#include "d_event.h"
#include "v_video.h"
#include "doomstat.h"
#include "r_bsp.h"
#include "r_main.h"
#include "r_draw.h"
#include "r_sky.h"
#include "r_plane.h"
#include "r_data.h"
#include "r_things.h"
#include "r_fps.h"
#include "p_maputl.h"
#include "m_bbox.h"
#include "lprintf.h"
#include "gl_intern.h"
#include "gl_struct.h"
#include "p_spec.h"
#include "i_system.h"
#include "i_video.h"
#include "i_main.h"
#include "am_map.h"
#include "sc_man.h"
#include "st_stuff.h"
#include "hu_stuff.h"
#include "e6y.h"//e6y

#include "dsda/configuration.h"
#include "dsda/map_format.h"
#include "dsda/render_stats.h"
#include "dsda/settings.h"
#include "dsda/stretch.h"
#include "dsda/gl/render_scale.h"
#include "dsda/utility.h"

int gl_preprocessed = false;

int gl_spriteindex;
int scene_has_overlapped_sprites;

int gl_blend_animations;

float gldepthmin, gldepthmax;

unsigned int invul_method;
float bw_red = 0.3f;
float bw_green = 0.59f;
float bw_blue = 0.11f;

extern const int tran_filter_pct;

dboolean use_fog=false;

GLfloat gl_texture_filter_anisotropic;

extern int gld_paletteIndex;

//sprites
const float gl_spriteclip_threshold_f = 10.f / MAP_COEFF;
const float gl_mask_sprite_threshold_f = 0.5f;

int fog_density=200;
static float extra_red=0.0f;
static float extra_green=0.0f;
static float extra_blue=0.0f;
static float extra_alpha=0.0f;

GLfloat gl_whitecolor[4]={1.0f,1.0f,1.0f,1.0f};

GLfloat cm2RGB[CR_LIMIT + 1][4] =
{
  {0.50f ,0.00f, 0.00f, 1.00f}, //CR_BRICK
  {1.00f ,1.00f, 1.00f, 1.00f}, //CR_TAN
  {1.00f ,1.00f, 1.00f, 1.00f}, //CR_GRAY
  {0.00f ,1.00f, 0.00f, 1.00f}, //CR_GREEN
  {0.50f ,0.20f, 1.00f, 1.00f}, //CR_BROWN
  {1.00f ,1.00f, 0.00f, 1.00f}, //CR_GOLD
  {1.00f ,0.00f, 0.00f, 1.00f}, //CR_DEFAULT
  {0.80f ,0.80f, 1.00f, 1.00f}, //CR_BLUE
  {1.00f ,0.50f, 0.25f, 1.00f}, //CR_ORANGE
  {1.00f ,1.00f, 0.00f, 1.00f}, //CR_YELLOW
  {0.50f ,0.50f, 1.00f, 1.00f}, //CR_BLUE2
  {0.00f ,0.00f, 0.00f, 1.00f}, //CR_BLACK
  {0.50f ,0.00f, 0.50f, 1.00f}, //CR_PURPLE
  {1.00f ,1.00f, 1.00f, 1.00f}, //CR_WHITE
  {1.00f ,0.00f, 0.00f, 1.00f}, //CR_RED
  {1.00f ,1.00f, 1.00f, 1.00f}, //CR_LIMIT
};

dsda_bfg_tracers_t dsda_bfg_tracers = { 0 };

void SetFrameTextureMode(void)
{
  if (SceneInTexture)
  {
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
  }
  else
  if (invul_method & INVUL_BW)
  {
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,GL_DOT3_RGB);
    glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB,GL_PRIMARY_COLOR);
    glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB,GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB,GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_RGB,GL_SRC_COLOR);
  }

  glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
  glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
  glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
}

void gld_InitTextureParams(void)
{
  typedef struct tex_format_s
  {
    int tex_format;
    const char *tex_format_name;
  } tex_format_t;

  tex_format_t tex_formats[] = {
    {GL_RGBA2,   "GL_RGBA2"},
    {GL_RGBA4,   "GL_RGBA4"},
    {GL_RGB5_A1, "GL_RGB5_A1"},
    {GL_RGBA8,   "GL_RGBA8"},
    {GL_RGBA,    "GL_RGBA"},
    {0, NULL}
  };

  int i;

  const char* gl_tex_format_string = dsda_StringConfig(dsda_config_gl_tex_format_string);

  gl_texture_filter_anisotropic =
    (GLfloat) (1 << dsda_IntConfig(dsda_config_gl_texture_filter_anisotropic));

  i = 0;
  while (tex_formats[i].tex_format_name)
  {
    if (!strcasecmp(gl_tex_format_string, tex_formats[i].tex_format_name))
    {
      gl_tex_format = tex_formats[i].tex_format;
      lprintf(LO_DEBUG, "Using texture format %s.\n", tex_formats[i].tex_format_name);
      break;
    }
    i++;
  }
}

const int gl_colorbuffer_bits = 32;
const int gl_depthbuffer_bits = 24;
int gl_render_multisampling;

void gld_MultisamplingInit(void)
{
  gl_render_multisampling = dsda_IntConfig(dsda_config_gl_render_multisampling);
  gl_render_multisampling -= (gl_render_multisampling % 2);

  if (gl_render_multisampling)
  {
    SDL_GL_SetAttribute( SDL_GL_BUFFER_SIZE, gl_colorbuffer_bits );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, gl_depthbuffer_bits );
    SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, gl_render_multisampling );
    SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
  }
  else
  {
    SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 0 );
    SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 0 );
  }
}

void gld_MultisamplingSet(void)
{
  if (gl_render_multisampling)
  {
    extern int map_use_multisampling;

    int use_multisampling = map_use_multisampling || automap_off;

    gld_EnableMultisample(use_multisampling);
  }
}

int gld_LoadGLDefs(const char * defsLump)
{
  typedef enum
  {
    TAG_SKYBOX,
    TAG_DETAIL,

    TAG_MAX
  } gldef_type_e;

  // these are the core types available in the *DEFS lump
  static const char *CoreKeywords[TAG_MAX + 1] =
  {
    "skybox",
    "detail",

    NULL
  };

  int result = false;

  if (W_LumpNameExists(defsLump))
  {
    SC_OpenLump(defsLump);

    // Get actor class name.
    while (SC_GetString())
    {
      switch (SC_MatchString(CoreKeywords))
      {
      case TAG_SKYBOX:
        result = true;
        gld_ParseSkybox();
        break;
      case TAG_DETAIL:
        result = true;
        gld_ParseDetail();
        break;
      }
    }

    SC_Close();
  }

  return result;
}



void gld_Init(int width, int height)
{
  GLfloat params[4]={0.0f,0.0f,1.0f,0.0f};

  lprintf(LO_DEBUG, "GL_VENDOR: %s\n",glGetString(GL_VENDOR));
  lprintf(LO_DEBUG, "GL_RENDERER: %s\n",glGetString(GL_RENDERER));
  lprintf(LO_DEBUG, "GL_VERSION: %s\n",glGetString(GL_VERSION));
  lprintf(LO_DEBUG, "GL_EXTENSIONS:\n");
  {
    char ext_name[256];
    const char *extensions = (const char*)glGetString(GL_EXTENSIONS);
    const char *rover = extensions;
    const char *p = rover;

    while (*rover)
    {
      size_t len = 0;
      p = rover;
      while (*p && *p != ' ')
      {
        p++;
        len++;
      }
      if (*p)
      {
        len = MIN(len, sizeof(ext_name)-1);
        memset(ext_name, 0, sizeof(ext_name));
        strncpy(ext_name, rover, len);
        lprintf(LO_DEBUG,"\t%s\n", ext_name);
      }
      rover = p;
      while (*rover && *rover == ' ')
        rover++;
    }
  }

  gld_InitOpenGL();
  gld_InitPalettedTextures();
  gld_InitTextureParams();

  dsda_GLSetRenderViewport();
  dsda_GLSetRenderViewportScissor();

  glClearColor(0.0f, 0.5f, 0.5f, 1.0f);
  glClearDepth(1.0f);

  glEnable(GL_BLEND);
  glEnable(GL_DEPTH_CLAMP_NV);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST); // proff_dis
  glShadeModel(GL_FLAT);
  gld_EnableTexture2D(GL_TEXTURE0_ARB, true);
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_ALPHA_TEST);
  glAlphaFunc(GL_GEQUAL,0.5f);
  glDisable(GL_CULL_FACE);
  glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);

  glTexGenfv(GL_Q,GL_EYE_PLANE,params);
  glTexGenf(GL_S,GL_TEXTURE_GEN_MODE,GL_EYE_LINEAR);
  glTexGenf(GL_T,GL_TEXTURE_GEN_MODE,GL_EYE_LINEAR);
  glTexGenf(GL_Q,GL_TEXTURE_GEN_MODE,GL_EYE_LINEAR);

  //e6y
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  gld_Finish();
  glClear(GL_COLOR_BUFFER_BIT);
  glClearColor(0.0f, 0.5f, 0.5f, 1.0f);

  // e6y
  // if you have a prior crash in the game,
  // you can restore the gamma values to at least a linear value
  // with -resetgamma command-line switch
  gld_ResetGammaRamp();

  gld_InitLightTable();
  gld_InitSky();
  M_ChangeLightMode();
  M_ChangeAllowFog();

  gld_InitDetail();

#ifdef HAVE_LIBSDL2_IMAGE
  gld_InitMapPics();
  gld_InitHiRes();
#endif

  // Create FBO object and associated render targets
  gld_InitFBO();
  I_AtExit(gld_FreeScreenSizeFBO, true, "gld_FreeScreenSizeFBO", exit_priority_normal);

  if(!gld_LoadGLDefs("GLBDEFS"))
  {
    gld_LoadGLDefs("GLDEFS");
  }

  gld_ResetLastTexture();

  I_AtExit(gld_CleanMemory, true, "gld_CleanMemory", exit_priority_normal); //e6y

  glsl_Init(); // elim - Required for fuzz shader, even if lighting mode not set to "shaders"
}

void gld_InitCommandLine(void)
{
}

//
// Textured automap
//

static int C_DECL dicmp_visible_subsectors_by_pic(const void *a, const void *b)
{
  return (*((const subsector_t *const *)b))->sector->floorpic -
         (*((const subsector_t *const *)a))->sector->floorpic;
}

static int visible_subsectors_count_prev = -1;
void gld_ResetTexturedAutomap(void)
{
  visible_subsectors_count_prev = -1;
}

static int map_textured_trans;
static int map_textured_overlay_trans;
static int map_lines_overlay_trans;

void gld_ResetAutomapTransparency(void)
{
  map_textured_trans = dsda_IntConfig(dsda_config_map_textured_trans);
  map_textured_overlay_trans = dsda_IntConfig(dsda_config_map_textured_overlay_trans);
  map_lines_overlay_trans = dsda_IntConfig(dsda_config_map_lines_overlay_trans);
}

void gld_MapDrawSubsectors(player_t *plr, int fx, int fy, fixed_t mx, fixed_t my, int fw, int fh, fixed_t scale)
{
  static subsector_t **visible_subsectors = NULL;
  static int visible_subsectors_size = 0;
  int visible_subsectors_count;

  int i;
  float alpha;
  float coord_scale;
  GLTexture *gltexture;

  alpha = (float)(automap_overlay ? map_textured_overlay_trans : map_textured_trans) / 100.0f;
  if (alpha == 0)
    return;

  if (numsubsectors > visible_subsectors_size)
  {
    visible_subsectors_size = numsubsectors;
    visible_subsectors = Z_Realloc(visible_subsectors, visible_subsectors_size * sizeof(visible_subsectors[0]));
  }

  visible_subsectors_count = 0;
  if (dsda_RevealAutomap())
  {
    visible_subsectors_count = numsubsectors;
  }
  else
  {
    for (i = 0; i < numsubsectors; i++)
    {
      visible_subsectors_count += map_subsectors[i];
    }
  }

  // Do not sort static visible_subsectors array at all
  // if there are no new visible subsectors.
  if (visible_subsectors_count != visible_subsectors_count_prev)
  {
    visible_subsectors_count_prev = visible_subsectors_count;

    visible_subsectors_count = 0;
    for (i = 0; i < numsubsectors; i++)
    {
      if ((map_subsectors[i] && !(subsectors[i].sector->flags & SECF_HIDDEN)) || dsda_RevealAutomap())
      {
        visible_subsectors[visible_subsectors_count++] = &subsectors[i];
      }
    }

    // sort subsectors by texture
    qsort(visible_subsectors, visible_subsectors_count,
      sizeof(visible_subsectors[0]), dicmp_visible_subsectors_by_pic);
  }

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glScissor(fx, SCREENHEIGHT - (fy + fh), fw, fh);
  glEnable(GL_SCISSOR_TEST);

  if (automap_rotate)
  {
    float pivotx = (float)(fx + fw / 2);
    float pivoty = (float)(fy + fh / 2);

    float rot = -(float)(ANG90 - viewangle) / (float)(1u << 31) * 180.0f;

    // Apply the automap's rotation to the vertexes.
    glTranslatef(pivotx, pivoty, 0.0f);
    glRotatef(rot, 0.0f, 0.0f, 1.0f);
    glTranslatef(-pivotx, -pivoty, 0.0f);
  }

  glTranslatef(
    (float)fx - (float)mx / (float)FRACUNIT * (float)scale / (float)FRACUNIT,
    (float)fy + (float)fh + (float)my / (float)FRACUNIT * (float)scale / (float)FRACUNIT,
    0);
  coord_scale = (float)scale / (float)(1<<FRACTOMAPBITS) / (float)FRACUNIT * MAP_COEFF;
  glScalef(-coord_scale, -coord_scale, 1.0f);

  for (i = 0; i < visible_subsectors_count; i++)
  {
    subsector_t *sub = visible_subsectors[i];
    int ssidx = sub - subsectors;

    if (sub->sector->bbox[BOXLEFT] > am_frame.bbox[BOXRIGHT] ||
      sub->sector->bbox[BOXRIGHT] < am_frame.bbox[BOXLEFT] ||
      sub->sector->bbox[BOXBOTTOM] > am_frame.bbox[BOXTOP] ||
      sub->sector->bbox[BOXTOP] < am_frame.bbox[BOXBOTTOM])
    {
      continue;
    }

    gltexture = gld_RegisterFlat(flattranslation[sub->sector->floorpic], true, V_IsUILightmodeIndexed());
    if (gltexture)
    {
      sector_t tempsec;
      int floorlight;
      float light;
      float floor_uoffs, floor_voffs;
      int loopnum;

      // For lighting and texture determination
      sector_t *sec = R_FakeFlat(sub->sector, &tempsec, &floorlight, NULL, false);

      gld_BindFlat(gltexture, 0);
      light = gld_Calc2DLightLevel(floorlight);
      gld_StaticLightAlpha(light, alpha);

      // Find texture origin.
      floor_uoffs = (float)sec->floor_xoffs/(float)(FRACUNIT*64);
      floor_voffs = (float)sec->floor_yoffs/(float)(FRACUNIT*64);

      for (loopnum = 0; loopnum < subsectorloops[ssidx].loopcount; loopnum++)
      {
        int vertexnum;
        GLLoopDef *currentloop = &subsectorloops[ssidx].loops[loopnum];

        if (!currentloop)
          continue;

        // set the mode (GL_TRIANGLE_FAN)
        glBegin(currentloop->mode);
        // go through all vertexes of this loop
        for (vertexnum = currentloop->vertexindex; vertexnum < (currentloop->vertexindex + currentloop->vertexcount); vertexnum++)
        {
          glTexCoord2f(flats_vbo[vertexnum].u + floor_uoffs, flats_vbo[vertexnum].v + floor_voffs);
          glVertex3f(flats_vbo[vertexnum].x, flats_vbo[vertexnum].z, 0);
        }
        glEnd();
      }
    }
  }

  // [XA] reset lighting so it doesn't interfere with
  // other drawing steps (e.g. indexed lightmode UI)
  gld_StaticLight(1.0f);

  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glDisable(GL_SCISSOR_TEST);
}

void gld_DrawTriangleStrip(GLWall *wall, gl_strip_coords_t *c)
{
  glBegin(GL_TRIANGLE_STRIP);

  glTexCoord2fv((const GLfloat*)&c->t[0]);
  glVertex3fv((const GLfloat*)&c->v[0]);

  glTexCoord2fv((const GLfloat*)&c->t[1]);
  glVertex3fv((const GLfloat*)&c->v[1]);

  glTexCoord2fv((const GLfloat*)&c->t[2]);
  glVertex3fv((const GLfloat*)&c->v[2]);

  glTexCoord2fv((const GLfloat*)&c->t[3]);
  glVertex3fv((const GLfloat*)&c->v[3]);

  glEnd();
}

void gld_BeginUIDraw(void)
{
  if (V_IsWorldLightmodeIndexed())
  {
    gld_InitColormapTextures();
    glsl_SetMainShaderActive();
    gld_StaticLight(1.0f); // UI is always "fullbright"
    gl_ui_lightmode_indexed = true;
  }
}

void gld_EndUIDraw(void)
{
  if (V_IsWorldLightmodeIndexed())
  {
    gl_ui_lightmode_indexed = false;
    glsl_SetActiveShader(NULL);
    return;
  }
}

void gld_BeginAutomapDraw(void)
{
  if (V_IsWorldLightmodeIndexed())
  {
    gld_InitColormapTextures();
    gl_automap_lightmode_indexed = true;
  }
}

void gld_EndAutomapDraw(void)
{
  if (V_IsWorldLightmodeIndexed())
  {
    gl_automap_lightmode_indexed = false;
    return;
  }
}

void gld_DrawNumPatch_f(float x, float y, int lump, int cm, enum patch_translation_e flags)
{
  GLTexture *gltexture;
  float fU1,fU2,fV1,fV2;
  float width,height;
  float xpos, ypos;
  int cmap;
  int leftoffset, topoffset;

  //e6y
  dboolean bFakeColormap;

  cmap = ((flags & VPT_TRANS) ? cm : CR_DEFAULT);
  gltexture=gld_RegisterPatch(lump, cmap, false, V_IsUILightmodeIndexed());
  gld_BindPatch(gltexture, cmap);

  if (!gltexture)
    return;
  fV1=0.0f;
  fV2=gltexture->scaleyfac;
  if (flags & VPT_FLIP)
  {
    fU1=gltexture->scalexfac;
    fU2=0.0f;
  }
  else
  {
    fU1=0.0f;
    fU2=gltexture->scalexfac;
  }

  if (flags & VPT_NOOFFSET)
  {
    leftoffset = 0;
    topoffset = 0;
  }
  else
  {
    leftoffset = gltexture->leftoffset;
    topoffset = gltexture->topoffset;
  }

  // [FG] automatically center wide patches without horizontal offset
  if (gltexture->width > 320 && leftoffset == 0)
    x -= (float)(gltexture->width - 320) / 2;

  if (flags & VPT_STRETCH_MASK)
  {
    stretch_param_t *params = dsda_StretchParams(flags);

    xpos   = (float)((x - leftoffset) * params->video->width)  / 320.0f + params->deltax1;
    ypos   = (float)((y - topoffset)  * params->video->height) / 200.0f + params->deltay1;
    width  = (float)(gltexture->realtexwidth     * params->video->width)  / 320.0f;
    height = (float)(gltexture->realtexheight    * params->video->height) / 200.0f;
  }
  else
  {
    xpos   = (float)(x - leftoffset);
    ypos   = (float)(y - topoffset);
    width  = (float)(gltexture->realtexwidth);
    height = (float)(gltexture->realtexheight);
  }

  bFakeColormap =
    (gltexture->flags & GLTEXTURE_HIRES) &&
    (lumpinfo[lump].flags & LUMP_CM2RGB);
  if (bFakeColormap)
  {
    if (!(flags & VPT_TRANS) && (cm != CR_GRAY))
    {
      cm = CR_RED;
    }
    glColor3f(cm2RGB[cm][0], cm2RGB[cm][1], cm2RGB[cm][2]);//, cm2RGB[cm][3]);
  }
  else
  {
    // e6y
    // This is a workaround for some on-board Intel video cards.
    // Do you know more elegant solution?
    glColor3f(1.0f, 1.0f, 1.0f);
  }

  glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(fU1, fV1); glVertex2f((xpos),(ypos));
    glTexCoord2f(fU1, fV2); glVertex2f((xpos),(ypos+height));
    glTexCoord2f(fU2, fV1); glVertex2f((xpos+width),(ypos));
    glTexCoord2f(fU2, fV2); glVertex2f((xpos+width),(ypos+height));
  glEnd();

  if (bFakeColormap)
  {
    glColor3f(1.0f,1.0f,1.0f);
  }
}

void gld_DrawNumPatch(int x, int y, int lump, int cm, enum patch_translation_e flags)
{
  gld_DrawNumPatch_f((float)x, (float)y, lump, cm, flags);
}

void gld_FillFlat(int lump, int x, int y, int width, int height, enum patch_translation_e flags)
{
  GLTexture *gltexture;
  float fU1, fU2, fV1, fV2;

  //e6y: Boom colormap should not be applied for background
  int saved_boom_cm = boom_cm;
  boom_cm = 0;

  gltexture = gld_RegisterFlat(lump, false, V_IsUILightmodeIndexed());
  gld_BindFlat(gltexture, 0);

  //e6y
  boom_cm = saved_boom_cm;

  if (!gltexture)
    return;

  if (flags & VPT_STRETCH)
  {
    x = x * SCREENWIDTH / 320;
    y = y * SCREENHEIGHT / 200;
    width = width * SCREENWIDTH / 320;
    height = height * SCREENHEIGHT / 200;
  }

  fU1 = 0;
  fV1 = 0;
  fU2 = (float)width / (float)gltexture->realtexwidth;
  fV2 = (float)height / (float)gltexture->realtexheight;

  glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(fU1, fV1); glVertex2f((float)(x),(float)(y));
    glTexCoord2f(fU1, fV2); glVertex2f((float)(x),(float)(y + height));
    glTexCoord2f(fU2, fV1); glVertex2f((float)(x + width),(float)(y));
    glTexCoord2f(fU2, fV2); glVertex2f((float)(x + width),(float)(y + height));
  glEnd();
}

void gld_FillPatch(int lump, int x, int y, int width, int height, enum patch_translation_e flags)
{
  GLTexture *gltexture;
  float fU1, fU2, fV1, fV2;

  //e6y: Boom colormap should not be applied for background
  int saved_boom_cm = boom_cm;
  boom_cm = 0;

  gltexture = gld_RegisterPatch(lump, CR_DEFAULT, false, V_IsUILightmodeIndexed());
  gld_BindPatch(gltexture, CR_DEFAULT);

  if (!gltexture)
    return;

  x = x - gltexture->leftoffset;
  y = y - gltexture->topoffset;

  //e6y
  boom_cm = saved_boom_cm;

  if (flags & VPT_STRETCH)
  {
    x = x * SCREENWIDTH / 320;
    y = y * SCREENHEIGHT / 200;
    width = width * SCREENWIDTH / 320;
    height = height * SCREENHEIGHT / 200;
  }

  fU1 = 0;
  fV1 = 0;
  fU2 = (float)width / (float)gltexture->realtexwidth;
  fV2 = (float)height / (float)gltexture->realtexheight;

  glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(fU1, fV1); glVertex2f((float)(x),(float)(y));
    glTexCoord2f(fU1, fV2); glVertex2f((float)(x),(float)(y + height));
    glTexCoord2f(fU2, fV1); glVertex2f((float)(x + width),(float)(y));
    glTexCoord2f(fU2, fV2); glVertex2f((float)(x + width),(float)(y + height));
  glEnd();
}

// [XA] some UI functions may run before fullcolormap has
// been initialized, since that's is done in SetupFrame;
// use colormaps[0] as a fallback in such a case.
const lighttable_t *gld_GetActiveColormap()
{
  if (V_IsAutomapLightmodeIndexed())
    return colormaps[0];
  else if (fixedcolormap)
    return fixedcolormap;
  else if (fullcolormap)
    return fullcolormap;
  else
    return colormaps[0];
}

// [XA] quicky indexed color-lookup function for a couple
// of things that aren't done in the shader for the indexed
// lightmode. ideally this will go away in the future.
color_rgb_t gld_LookupIndexedColor(int index)
{
  color_rgb_t color;
  const unsigned char *playpal;

  if (V_IsUILightmodeIndexed() || V_IsAutomapLightmodeIndexed())
  {
    int gtlump = W_CheckNumForName2("GAMMATBL", ns_prboom);
    const byte * gtable = (const byte*)W_LumpByNum(gtlump) + 256 * usegamma;
    const lighttable_t *colormap = gld_GetActiveColormap();

    playpal = V_GetPlaypal() + (gld_paletteIndex*PALETTE_SIZE);

    color.r = gtable[playpal[colormap[index] * 3 + 0]];
    color.g = gtable[playpal[colormap[index] * 3 + 1]];
    color.b = gtable[playpal[colormap[index] * 3 + 2]];
  }
  else
  {
    playpal = V_GetPlaypal();

    color.r = playpal[3 * index + 0];
    color.g = playpal[3 * index + 1];
    color.b = playpal[3 * index + 2];
  }

  return color;
}

void gld_DrawLine_f(float x0, float y0, float x1, float y1, int BaseColor)
{
  const unsigned char *playpal = V_GetPlaypal();
  color_rgb_t color;
  unsigned char a;
  map_line_t *line;

  a = (automap_overlay ? map_lines_overlay_trans * 255 / 100 : 255);
  if (a == 0)
    return;

  color = gld_LookupIndexedColor(BaseColor);

  line = M_ArrayGetNewItem(&map_lines, sizeof(line[0]));

  line->point[0].x = x0;
  line->point[0].y = y0;
  line->point[0].r = color.r;
  line->point[0].g = color.g;
  line->point[0].b = color.b;
  line->point[0].a = a;

  line->point[1].x = x1;
  line->point[1].y = y1;
  line->point[1].r = color.r;
  line->point[1].g = color.g;
  line->point[1].b = color.b;
  line->point[1].a = a;
}

void gld_DrawLine(int x0, int y0, int x1, int y1, int BaseColor)
{
  gld_DrawLine_f((float)x0, (float)y0, (float)x1, (float)y1, BaseColor);
}

void gld_DrawWeapon(int weaponlump, vissprite_t *vis, int lightlevel)
{
  GLTexture *gltexture;
  float fU1,fU2,fV1,fV2;
  int x1,y1,x2,y2;
  float light;

  gltexture=gld_RegisterPatch(firstspritelump+weaponlump, CR_DEFAULT, false, V_IsWorldLightmodeIndexed());
  if (!gltexture)
    return;
  gld_BindPatch(gltexture, CR_DEFAULT);
  fU1=0;
  fV1=0;
  fU2=gltexture->scalexfac;
  fV2=gltexture->scaleyfac;
  // e6y
  // More precise weapon drawing:
  // Shotgun from DSV3_War looks correctly now. Especially during movement.
  // There is no more line of graphics under certain weapons.
  x1=viewwindowx+vis->x1;
  x2=x1+(int)((float)gltexture->realtexwidth*pspritexscale_f);
  y1=viewwindowy+centery-(int)(((float)vis->texturemid/(float)FRACUNIT)*pspriteyscale_f);
  y2=y1+(int)((float)gltexture->realtexheight*pspriteyscale_f)+1;
  // e6y: don't do the gamma table correction on the lighting
  light = (float)lightlevel / 255.0f;

  // e6y
  // Fix of no warning (flashes between shadowed and solid)
  // when invisibility is about to go
  if (/*(viewplayer->mo->flags & MF_SHADOW) && */!vis->colormap)
  {
    glsl_SetFuzzShaderActive();
    glsl_SetFuzzTextureDimensions((float)gltexture->realtexwidth, (float)gltexture->realtexheight);
    glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
    glAlphaFunc(GL_GEQUAL,0.1f);
    glColor4f(0.2f,0.2f,0.2f,0.01f);
  }
  else
  {
    if (viewplayer->mo->flags & g_mf_translucent)
      gld_StaticLightAlpha(light,(float)tran_filter_pct/100.0f);
    else
      gld_StaticLight(light);
  }
  glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(fU1, fV1); glVertex2f((float)(x1),(float)(y1));
    glTexCoord2f(fU1, fV2); glVertex2f((float)(x1),(float)(y2));
    glTexCoord2f(fU2, fV1); glVertex2f((float)(x2),(float)(y1));
    glTexCoord2f(fU2, fV2); glVertex2f((float)(x2),(float)(y2));
  glEnd();
  if(!vis->colormap)
  {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glAlphaFunc(GL_GEQUAL,0.3f);
  }
  glColor3f(1.0f,1.0f,1.0f);
  glsl_SetFuzzShaderInactive();
}

void gld_FillBlock(int x, int y, int width, int height, int col)
{
  const unsigned char *playpal = V_GetPlaypal();
  color_rgb_t color = gld_LookupIndexedColor(col);

  gld_EnableTexture2D(GL_TEXTURE0_ARB, false);

  glColor3f((float)color.r/255.0f,
            (float)color.g/255.0f,
            (float)color.b/255.0f);

  glBegin(GL_TRIANGLE_STRIP);
    glVertex2i( x, y );
    glVertex2i( x, y+height );
    glVertex2i( x+width, y );
    glVertex2i( x+width, y+height );
  glEnd();
  glColor3f(1.0f,1.0f,1.0f);
  gld_EnableTexture2D(GL_TEXTURE0_ARB, true);
}

void gld_SetPalette(int palette)
{
  static int last_palette = 0;
  extra_red=0.0f;
  extra_green=0.0f;
  extra_blue=0.0f;
  extra_alpha=0.0f;

  if (palette < 0)
    palette = last_palette;
  last_palette = palette;

  // [XA] store the current palette so
  // the indexed lightmode can use it.
  // if we're actually in indexed mode,
  // then we're all done here.
  gld_SetIndexedPalette(palette);
  if (V_IsWorldLightmodeIndexed())
    return;

  if (palette > 0)
  {
    if (palette <= 8)
    {
      // doom [0] 226 1 1
      extra_red = 1.0f;
      extra_green = 0.0f;
      extra_blue = 0.0f;
      extra_alpha = (float) palette / 9.0f;
    }
    else if (palette <= 12)
    {
      // doom [0] 108 94 35
      palette = palette - 8;
      extra_red = 1.0f;
      extra_green = 0.9f;
      extra_blue = 0.3f;
      extra_alpha = (float) palette / 10.0f;
    }
    else if (!hexen && palette == 13)
    {
      extra_red = 0.4f;
      extra_green = 1.0f;
      extra_blue = 0.0f;
      extra_alpha = 0.2f;
    }
    else if (hexen)
    {
      if (palette <= 20)
      {
        // hexen [0] = 35 74 29
        palette = palette - 12;
        extra_red = 0.5f;
        extra_green = 1.0f;
        extra_blue = 0.4f;
        extra_alpha = (float) palette / 27.f;
      }
      else if (palette == 21)
      {
        // hexen [0] = 1 1 113
        extra_red = 0.0f;
        extra_green = 0.0f;
        extra_blue = 1.0f;
        extra_alpha = 0.4f;
      }
      else if (palette <= 24)
      {
        // hexen [...] = 66, 51, 36
        palette = 24 - palette;
        extra_red = 1.0f;
        extra_green = 1.0f;
        extra_blue = 1.0f;
        extra_alpha = 0.14f + 0.06f * palette;
      }
      else if (palette <= 27)
      {
        // hexen [0] = 76 56 1
        palette = 27 - palette;
        extra_red = 1.0f;
        extra_green = 0.7f;
        extra_blue = 0.0f;
        extra_alpha = 0.14f + 0.06f * palette;
      }
    }
  }
  if (extra_red > 1.0f)
    extra_red = 1.0f;
  if (extra_green > 1.0f)
    extra_green = 1.0f;
  if (extra_blue > 1.0f)
    extra_blue = 1.0f;
  if (extra_alpha > 1.0f)
    extra_alpha = 1.0f;
}

unsigned char *gld_ReadScreen(void)
{ // NSM convert to static
  static unsigned char *scr = NULL;
  static unsigned char *buffer = NULL;
  static int scr_size = 0;

  int src_row, dest_row, size, pixels_per_row;

  pixels_per_row = gl_window_width * 3;
  size = pixels_per_row * gl_window_height;
  if (!scr || size > scr_size)
  {
    scr_size = size;
    scr = Z_Realloc(scr, size);
    buffer = Z_Realloc(buffer, size);
  }

  if (buffer && scr)
  {
    GLint pack_aligment;
    glGetIntegerv(GL_PACK_ALIGNMENT, &pack_aligment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glFlush();
    glReadPixels(0, 0, gl_window_width, gl_window_height, GL_RGB, GL_UNSIGNED_BYTE, scr);

    glPixelStorei(GL_PACK_ALIGNMENT, pack_aligment);

    gld_ApplyGammaRamp(scr, pixels_per_row, gl_window_width, gl_window_height);

    // GL textures are bottom up, so copy the rows in reverse to flip vertically
    for (src_row = gl_window_height - 1, dest_row = 0; src_row >= 0; --src_row, ++dest_row)
    {
      memcpy(&buffer[dest_row * pixels_per_row],
              &scr[src_row * pixels_per_row],
              pixels_per_row);
    }
  }

  return buffer;
}

GLvoid gld_Set2DMode(void)
{
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(
    (GLdouble) 0,
    (GLdouble) SCREENWIDTH,
    (GLdouble) SCREENHEIGHT,
    (GLdouble) 0,
    (GLdouble) -1.0,
    (GLdouble) 1.0
  );
  glDisable(GL_DEPTH_TEST);
}

void gld_InitDrawScene(void)
{
  gld_ResetDrawInfo();
}

void gld_Finish(void)
{
  gld_Set2DMode();
  SDL_GL_SwapWindow(sdl_window);
}

GLuint flats_vbo_id = 0; // ID of VBO

vbo_xyz_uv_t *flats_vbo = NULL;

GLSeg *gl_segs=NULL;
GLSeg *gl_lines=NULL;

byte rendermarker=0;
byte *segrendered; // true if sector rendered (only here for malloc)
byte *linerendered[2]; // true if linedef rendered (only here for malloc)

float roll     = 0.0f;
float yaw      = 0.0f;
float inv_yaw  = 0.0f;
float pitch    = 0.0f;
float cos_inv_yaw, sin_inv_yaw;
float paperitems_pitch;
float cos_paperitems_pitch, sin_paperitems_pitch;

#define __glPi 3.14159265358979323846

void gld_Clear(void)
{
  int clearbits = 0;

  // flashing red HOM indicators
  if (dsda_IntConfig(dsda_config_flashing_hom))
  {
    clearbits |= GL_COLOR_BUFFER_BIT;
    glClearColor (gametic % 20 < 9 ? 1.0f : 0.0f, 0.0f, 0.0f, 1.0f);
  }

  if (gl_use_stencil)
    clearbits |= GL_STENCIL_BUFFER_BIT;

  clearbits |= GL_DEPTH_BUFFER_BIT;

  if (clearbits)
    glClear(clearbits);
}

void gld_StartDrawScene(void)
{
  // Progress fuzz time seed
  glsl_SetFuzzTime(gametic);

  gld_MultisamplingSet();

  gld_SetPalette(-1);

  glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  glScissor(viewwindowx, SCREENHEIGHT-(viewheight+viewwindowy), viewwidth, viewheight);
  glEnable(GL_SCISSOR_TEST);
  // Player coordinates
  xCamera=-(float)viewx/MAP_SCALE;
  yCamera=(float)viewy/MAP_SCALE;
  zCamera=(float)viewz/MAP_SCALE;

  yaw=270.0f-(float)(viewangle>>ANGLETOFINESHIFT)*360.0f/FINEANGLES;
  inv_yaw=180.0f-yaw;
  cos_inv_yaw = (float)cos(inv_yaw * M_PI / 180.f);
  sin_inv_yaw = (float)sin(inv_yaw * M_PI / 180.f);

  gl_spriteindex = 0;

  //e6y: fog in frame
  gl_use_fog = gl_fog && !frame_fixedcolormap && !boom_cm;

//e6y
  mlook_or_fov = dsda_MouseLook() || (gl_render_fov != FOV90);
  if(!mlook_or_fov)
  {
    pitch = 0.0f;
    paperitems_pitch = 0.0f;

    skyXShift = -2.0f * ((yaw + 90.0f) / 90.0f);
    skyYShift = 200.0f / 319.5f;
  }
  else
  {
    float f = viewPitch * 2 + 50 / skyscale;
    f = BETWEEN(0, 127, f);
    skyXShift = -2.0f * ((yaw + 90.0f) / 90.0f / skyscale);
    skyYShift = f / 128.0f + 200.0f / 320.0f / skyscale;

    pitch = (float)(viewpitch>>ANGLETOFINESHIFT) * 360.0f / FINEANGLES;
    paperitems_pitch = ((pitch > 87.0f && pitch <= 90.0f) ? 87.0f : pitch);
    viewPitch = (pitch > 180 ? pitch - 360 : pitch);
  }
  cos_paperitems_pitch = (float)cos(paperitems_pitch * M_PI / 180.f);
  sin_paperitems_pitch = (float)sin(paperitems_pitch * M_PI / 180.f);

  gld_InitFrameSky();

  invul_method = 0;
  if (players[displayplayer].fixedcolormap == 32)
  {
    if (gl_boom_colormaps && !gl_has_hires)
    {
      invul_method = INVUL_CM;
    }
    else
    {
      if (gl_version >= OPENGL_VERSION_1_3)
      {
        invul_method = INVUL_BW;
      }
      else
      {
        invul_method = INVUL_INV;
      }
    }
  }

  // elim - Always enabled (when supported) for upscaling with GL exclusive disabled
  SceneInTexture = gl_ext_framebuffer_object;

  // Vortex: Set FBO object
  if (SceneInTexture)
  {
    GLEXT_glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, glSceneImageFBOTexID);
  }

  SetFrameTextureMode();

  gld_Clear();

  glEnable(GL_DEPTH_TEST);

  glMatrixMode(GL_PROJECTION);
  glLoadMatrixf(projMatrix);

  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixf(modelMatrix);

  gld_InitColormapTextures();

  rendermarker++;
  scene_has_overlapped_sprites = false;
  scene_has_wall_details = 0;
  scene_has_flat_details = 0;
}

//e6y
void gld_ProcessExtraAlpha(void)
{
  if (extra_alpha>0.0f && !invul_method)
  {
    float current_color[4];
    glGetFloatv(GL_CURRENT_COLOR, current_color);
    glDisable(GL_ALPHA_TEST);
    glColor4f(extra_red, extra_green, extra_blue, extra_alpha);
    gld_EnableTexture2D(GL_TEXTURE0_ARB, false);
    glBegin(GL_TRIANGLE_STRIP);
      glVertex2f( 0.0f, 0.0f);
      glVertex2f( 0.0f, (float)SCREENHEIGHT);
      glVertex2f( (float)SCREENWIDTH, 0.0f);
      glVertex2f( (float)SCREENWIDTH, (float)SCREENHEIGHT);
    glEnd();
    gld_EnableTexture2D(GL_TEXTURE0_ARB, true);
    glEnable(GL_ALPHA_TEST);
    glColor4f(current_color[0], current_color[1], current_color[2], current_color[3]);
  }
}

//e6y
static void gld_InvertScene(void)
{
  glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
  glColor4f(1,1,1,1);
  gld_EnableTexture2D(GL_TEXTURE0_ARB, false);
  glBegin(GL_TRIANGLE_STRIP);
    glVertex2f( 0.0f, 0.0f);
    glVertex2f( 0.0f, (float)SCREENHEIGHT);
    glVertex2f( (float)SCREENWIDTH, 0.0f);
    glVertex2f( (float)SCREENWIDTH, (float)SCREENHEIGHT);
  glEnd();
  gld_EnableTexture2D(GL_TEXTURE0_ARB, true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void gld_EndDrawScene(void)
{
  glDisable(GL_POLYGON_SMOOTH);

  glViewport(0, 0, SCREENWIDTH, SCREENHEIGHT);
  gl_EnableFog(false);
  gld_Set2DMode();

  glsl_SetMainShaderActive();
  R_DrawPlayerSprites();
  glsl_SetActiveShader(NULL);

  // e6y
  // Effect of invulnerability uses a colormap instead of hard-coding now
  // See nuts.wad
  // http://www.doomworld.com/idgames/index.php?id=11402

  // Vortex: Black and white effect
  if (SceneInTexture)
  {
    // Vortex: Restore original RT
    GLEXT_glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    glBindTexture(GL_TEXTURE_2D, glSceneImageTextureFBOTexID);

    // Setup blender
    if (invul_method & INVUL_BW)
    {
      glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);
      glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,GL_DOT3_RGB);
      glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB,GL_PRIMARY_COLOR);
      glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB,GL_SRC_COLOR);
      glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB,GL_TEXTURE);
      glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_RGB,GL_SRC_COLOR);

      glColor3f(0.3f, 0.3f, 0.4f);
    }
    else
    {
      glColor3f(1.0f, 1.0f, 1.0f);
    }

    // Setup GL camera for drawing the render texture
    dsda_GLFullscreenOrtho2D();
    dsda_GLSetRenderViewport();
    // elim - Prevent undrawn parts of game scene texture being rendered into the viewport
    dsda_GLSetRenderSceneScissor();
    glBegin(GL_TRIANGLE_STRIP);
    {
      glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 0.0f);
      glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, gl_window_height);
      glTexCoord2f(1.0f, 1.0f); glVertex2f((float)gl_window_width, 0.0f);
      glTexCoord2f(1.0f, 0.0f); glVertex2f((float)gl_window_width, (float)gl_window_height);
    }
    glEnd();

    // elim - Set the scissor back to the full viewport so post-scene draws can happen (ie StatusBar)
    dsda_GLSetRenderViewportScissor();

    gld_Set2DMode();

    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
  }
  else
  {
    if (invul_method & INVUL_INV)
    {
      gld_InvertScene();
    }
    if (invul_method & INVUL_BW)
    {
      glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    }
  }

  glColor3f(1.0f,1.0f,1.0f);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_ALPHA_TEST);
}

static void gld_AddDrawWallItem(GLDrawItemType itemtype, void *itemdata)
{
  if (gl_blend_animations)
  {
    anim_t *anim;
    int currpic, nextpic;
    GLWall *wall = (GLWall*)itemdata;
    float oldalpha = wall->alpha;
    dboolean indexed = V_IsWorldLightmodeIndexed();

    switch (itemtype)
    {
    case GLDIT_FWALL:
      anim = anim_flats[wall->gltexture->index - firstflat].anim;
      if (anim)
      {
        wall->alpha = 1.0f - ((float)tic_vars.frac + ((leveltime - 1) % anim->speed) * 65536.0f) / (65536.0f * anim->speed);
        gld_AddDrawItem(GLDIT_FAWALL, itemdata);

        currpic = wall->gltexture->index - firstflat - anim->basepic;
        nextpic = anim->basepic + (currpic + 1) % anim->numpics;
        wall->alpha = oldalpha;
        wall->gltexture = gld_RegisterFlat(nextpic, true, indexed);
      }
      break;
    case GLDIT_WALL:
    case GLDIT_MWALL:
      anim = anim_textures[wall->gltexture->index].anim;
      if (anim)
      {
        if (itemtype == GLDIT_WALL || itemtype == GLDIT_MWALL)
        {
          wall->alpha = 1.0f - ((float)tic_vars.frac + ((leveltime - 1) % anim->speed) * 65536.0f) / (65536.0f * anim->speed);
          gld_AddDrawItem(GLDIT_AWALL, itemdata);

          currpic = wall->gltexture->index - anim->basepic;
          nextpic = anim->basepic + (currpic + 1) % anim->numpics;
          wall->alpha = oldalpha;
          wall->gltexture = gld_RegisterTexture(nextpic, true, false, indexed);
        }
      }
      break;
    }
  }

  if (((GLWall*)itemdata)->gltexture->detail)
    scene_has_wall_details++;

  gld_AddDrawItem(itemtype, itemdata);
}

/*****************
 *               *
 * Walls         *
 *               *
 *****************/

static void gld_DrawWall(GLWall *wall)
{
  int has_detail;
  unsigned int flags;

  dsda_RecordDrawSeg();

  has_detail =
    scene_has_details &&
    gl_arb_multitexture &&
    (wall->flag < GLDWF_SKY) &&
    (wall->gltexture->detail);

  // Do not repeat middle texture vertically
  // to avoid visual glitches for textures with holes
  if ((wall->flag == GLDWF_M2S) && (wall->flag < GLDWF_SKY))
    flags = GLTEXTURE_CLAMPY;
  else
    flags = 0;

  gld_BindTexture(wall->gltexture, flags);
  gld_BindDetailARB(wall->gltexture, has_detail);

  if (!wall->gltexture)
  {
    glColor4f(1.0f,0.0f,0.0f,1.0f);
  }

  if (has_detail)
  {
    gld_DrawWallWithDetail(wall);
  }
  else
  {
    if ((wall->flag == GLDWF_TOPFLUD) || (wall->flag == GLDWF_BOTFLUD))
    {
      gl_strip_coords_t c;

      gld_BindFlat(wall->gltexture, 0);

      gld_SetupFloodStencil(wall);
      gld_SetupFloodedPlaneCoords(wall, &c);
      gld_SetupFloodedPlaneLight(wall);
      gld_DrawTriangleStrip(wall, &c);

      gld_ClearFloodStencil(wall);
    }
    else
    {
      gld_StaticLightAlpha(wall->light, wall->alpha);

      glBegin(GL_TRIANGLE_FAN);

      // lower left corner
      glTexCoord2f(wall->ul,wall->vb);
      glVertex3f(wall->glseg->x1,wall->ybottom,wall->glseg->z1);

      // split left edge of wall
      if (!wall->glseg->fracleft)
        gld_SplitLeftEdge(wall, false);

      // upper left corner
      glTexCoord2f(wall->ul,wall->vt);
      glVertex3f(wall->glseg->x1,wall->ytop,wall->glseg->z1);

      // upper right corner
      glTexCoord2f(wall->ur,wall->vt);
      glVertex3f(wall->glseg->x2,wall->ytop,wall->glseg->z2);

      // split right edge of wall
      if (!wall->glseg->fracright)
        gld_SplitRightEdge(wall, false);

      // lower right corner
      glTexCoord2f(wall->ur,wall->vb);
      glVertex3f(wall->glseg->x2,wall->ybottom,wall->glseg->z2);

      glEnd();
    }
  }
}

#define LINE seg->linedef
#define CALC_Y_VALUES(w, lineheight, floor_height, ceiling_height)\
  (w).ytop=((float)(ceiling_height)/(float)MAP_SCALE)+SMALLDELTA;\
  (w).ybottom=((float)(floor_height)/(float)MAP_SCALE)-SMALLDELTA;\
  lineheight=((float)fabs(((ceiling_height)/(float)FRACUNIT)-((floor_height)/(float)FRACUNIT)))

#define OU(w,seg) (((float)((seg)->sidedef->textureoffset)/(float)FRACUNIT)/(float)(w).gltexture->buffer_width)
#define OV(w,seg) (((float)((seg)->sidedef->rowoffset)/(float)FRACUNIT)/(float)(w).gltexture->buffer_height)
#define OV_PEG(w,seg,v_offset) (OV((w),(seg))-(((float)(v_offset)/(float)FRACUNIT)/(float)(w).gltexture->buffer_height))
#define URUL(w, seg, backseg, linelength)\
  if (backseg){\
    (w).ur=OU((w),(seg));\
    (w).ul=(w).ur+((linelength)/(float)(w).gltexture->buffer_width);\
  }else{\
    (w).ul=OU((w),(seg));\
    (w).ur=(w).ul+((linelength)/(float)(w).gltexture->buffer_width);\
  }

#define CALC_TEX_VALUES_TOP(w, seg, backseg, peg, linelength, lineheight)\
  (w).flag=GLDWF_TOP;\
  URUL(w, seg, backseg, linelength);\
  if (peg){\
    (w).vb=OV((w),(seg))+((w).gltexture->scaleyfac);\
    (w).vt=((w).vb-((float)(lineheight)/(float)(w).gltexture->buffer_height));\
  }else{\
    (w).vt=OV((w),(seg));\
    (w).vb=(w).vt+((float)(lineheight)/(float)(w).gltexture->buffer_height);\
  }

#define CALC_TEX_VALUES_MIDDLE1S(w, seg, backseg, peg, linelength, lineheight)\
  (w).flag=GLDWF_M1S;\
  URUL(w, seg, backseg, linelength);\
  if (peg){\
    (w).vb=OV((w),(seg))+((w).gltexture->scaleyfac);\
    (w).vt=((w).vb-((float)(lineheight)/(float)(w).gltexture->buffer_height));\
  }else{\
    (w).vt=OV((w),(seg));\
    (w).vb=(w).vt+((float)(lineheight)/(float)(w).gltexture->buffer_height);\
  }

#define CALC_TEX_VALUES_BOTTOM(w, seg, backseg, peg, linelength, lineheight, v_offset)\
  (w).flag=GLDWF_BOT;\
  URUL(w, seg, backseg, linelength);\
  if (peg){\
    (w).vb=OV_PEG((w),(seg),(v_offset))+((w).gltexture->scaleyfac);\
    (w).vt=((w).vb-((float)(lineheight)/(float)(w).gltexture->buffer_height));\
  }else{\
    (w).vt=OV((w),(seg));\
    (w).vb=(w).vt+((float)(lineheight)/(float)(w).gltexture->buffer_height);\
  }

void gld_AddWall(seg_t *seg)
{
  extern sector_t *poly_frontsector;
  extern dboolean poly_add_line;
  GLWall wall;
  GLTexture *temptex;
  sector_t *frontsector;
  sector_t *backsector;
  sector_t ftempsec; // needed for R_FakeFlat
  sector_t btempsec; // needed for R_FakeFlat
  float lineheight, linelength;
  int rellight = 0;
  int backseg;
  dboolean fix_sky_bleed = false;
  dboolean indexed;

  int side = (seg->sidedef == &sides[seg->linedef->sidenum[0]] ? 0 : 1);
  if (linerendered[side][seg->linedef->iLineID] == rendermarker)
    return;
  linerendered[side][seg->linedef->iLineID] = rendermarker;
  linelength = lines[seg->linedef->iLineID].texel_length;
  wall.glseg=&gl_lines[seg->linedef->iLineID];
  backseg = seg->sidedef != &sides[seg->linedef->sidenum[0]];

  indexed = V_IsWorldLightmodeIndexed();

  if (poly_add_line)
  {
    int i = seg->linedef->iLineID;
    // hexen_note: find some way to do this only on update?
    gl_lines[i].x1=-(float)lines[i].v1->x/(float)MAP_SCALE;
    gl_lines[i].z1= (float)lines[i].v1->y/(float)MAP_SCALE;
    gl_lines[i].x2=-(float)lines[i].v2->x/(float)MAP_SCALE;
    gl_lines[i].z2= (float)lines[i].v2->y/(float)MAP_SCALE;

    if (!poly_frontsector)
      return;
    frontsector=R_FakeFlat(poly_frontsector, &ftempsec, NULL, NULL, false); // for boom effects
  }
  else
  {
    if (!seg->frontsector)
      return;
    frontsector=R_FakeFlat(seg->frontsector, &ftempsec, NULL, NULL, false); // for boom effects
  }

  if (!frontsector)
    return;

  // e6y: fake contrast stuff
  // Original doom added/removed one light level ((1<<LIGHTSEGSHIFT) == 16)
  // for walls exactly vertical/horizontal on the map
  if (fake_contrast)
  {
    rellight = seg->linedef->dx == 0 ? +gl_rellight : seg->linedef->dy==0 ? -gl_rellight : 0;
  }
  wall.light=gld_CalcLightLevel(frontsector->lightlevel+rellight+gld_GetGunFlashLight());
  wall.fogdensity = gld_CalcFogDensity(frontsector, frontsector->lightlevel, GLDIT_WALL);
  wall.alpha=1.0f;
  wall.gltexture=NULL;
  wall.seg = seg; //e6y

  if (!seg->backsector) /* onesided */
  {
    if (frontsector->ceilingpic==skyflatnum)
    {
      wall.ytop=MAXCOORD;
      wall.ybottom=(float)frontsector->ceilingheight/MAP_SCALE;
      gld_AddSkyTexture(&wall, frontsector->sky, frontsector->sky, SKY_CEILING);
    }
    if (frontsector->floorpic==skyflatnum)
    {
      wall.ytop=(float)frontsector->floorheight/MAP_SCALE;
      wall.ybottom=-MAXCOORD;
      gld_AddSkyTexture(&wall, frontsector->sky, frontsector->sky, SKY_FLOOR);
    }
    temptex=gld_RegisterTexture(texturetranslation[seg->sidedef->midtexture], true, false, indexed);
    if (temptex && frontsector->ceilingheight > frontsector->floorheight)
    {
      wall.gltexture=temptex;
      CALC_Y_VALUES(wall, lineheight, frontsector->floorheight, frontsector->ceilingheight);
      CALC_TEX_VALUES_MIDDLE1S(
        wall, seg, backseg, (LINE->flags & ML_DONTPEGBOTTOM)>0,
        linelength, lineheight
      );
      gld_AddDrawWallItem(GLDIT_WALL, &wall);
    }
  }
  else /* twosided */
  {
    sector_t *fs, *bs;
    int toptexture, midtexture, bottomtexture;
    fixed_t floor_height,ceiling_height;
    fixed_t max_floor, min_floor;
    fixed_t max_ceiling, min_ceiling;
    //fixed_t max_floor_tex, min_ceiling_tex;

    backsector=R_FakeFlat(seg->backsector, &btempsec, NULL, NULL, true); // for boom effects
    if (!backsector)
      return;

    if (frontsector->floorheight > backsector->floorheight)
    {
      max_floor = frontsector->floorheight;
      min_floor = backsector->floorheight;
    }
    else
    {
      max_floor = backsector->floorheight;
      min_floor = frontsector->floorheight;
    }

    if (frontsector->ceilingheight > backsector->ceilingheight)
    {
      max_ceiling = frontsector->ceilingheight;
      min_ceiling = backsector->ceilingheight;
    }
    else
    {
      max_ceiling = backsector->ceilingheight;
      min_ceiling = frontsector->ceilingheight;
    }

    //max_floor_tex = max_floor + seg->sidedef->rowoffset;
    //min_ceiling_tex = min_ceiling + seg->sidedef->rowoffset;

    if (backseg)
    {
      fs = backsector;
      bs = frontsector;
    }
    else
    {
      fs = frontsector;
      bs = backsector;
    }

    toptexture = texturetranslation[seg->sidedef->toptexture];
    midtexture = texturetranslation[seg->sidedef->midtexture];
    bottomtexture = texturetranslation[seg->sidedef->bottomtexture];

    /* toptexture */
    ceiling_height=frontsector->ceilingheight;
    floor_height=backsector->ceilingheight;
    if (frontsector->ceilingpic==skyflatnum)// || backsector->ceilingpic==skyflatnum)
    {
      wall.ytop= MAXCOORD;
      if (
          // e6y
          // There is no more HOM in the starting area on Memento Mori map29 and on map30.
          // Old code:
          // (backsector->ceilingheight==backsector->floorheight) &&
          // (backsector->ceilingpic==skyflatnum)
          (backsector->ceilingpic==skyflatnum) &&
          (backsector->ceilingheight<=backsector->floorheight)
         )
      {
        // e6y
        // There is no more visual glitches with sky on Icarus map14 sector 187
        // Old code: wall.ybottom=(float)backsector->floorheight/MAP_SCALE;
        wall.ybottom=((float)(backsector->floorheight +
          (seg->sidedef->rowoffset > 0 ? seg->sidedef->rowoffset : 0)))/MAP_SCALE;
        gld_AddSkyTexture(&wall, frontsector->sky, backsector->sky, SKY_CEILING);
      }
      else
      {
        if (bs->ceilingpic == skyflatnum && fs->ceilingpic != skyflatnum &&
          toptexture == NO_TEXTURE && midtexture == NO_TEXTURE)
        {
          wall.ybottom=(float)min_ceiling/MAP_SCALE;
          if (bs->ceilingheight < fs->floorheight)
          {
            fix_sky_bleed = true;
          }
          gld_AddSkyTexture(&wall, frontsector->sky, backsector->sky, SKY_CEILING);
        }
        else
        {
          if (((backsector->ceilingpic != skyflatnum && toptexture != NO_TEXTURE) && midtexture == NO_TEXTURE) ||
            backsector->ceilingpic != skyflatnum ||
            backsector->ceilingheight <= frontsector->floorheight)
          {
            if (frontsector->ceilingpic == skyflatnum && frontsector->ceilingheight < backsector->floorheight)
            {
              wall.ybottom=(float)min_ceiling/MAP_SCALE;
              fix_sky_bleed = true;
            }
            else
            {
              wall.ybottom=(float)max_ceiling/MAP_SCALE;
            }
            gld_AddSkyTexture(&wall, frontsector->sky, backsector->sky, SKY_CEILING);
          }
        }
      }
    }
    if (floor_height<ceiling_height)
    {
      if (!((frontsector->ceilingpic==skyflatnum) && (backsector->ceilingpic==skyflatnum)))
      {
        temptex=gld_RegisterTexture(toptexture, true, false, indexed);
        if (!temptex && gl_use_stencil && backsector &&
          !(seg->linedef->r_flags & RF_ISOLATED) &&
          /*frontsector->ceilingpic != skyflatnum && */backsector->ceilingpic != skyflatnum &&
          !(backsector->flags & NULL_SECTOR))
        {
          wall.ytop=((float)(ceiling_height)/(float)MAP_SCALE)+SMALLDELTA;
          wall.ybottom=((float)(floor_height)/(float)MAP_SCALE)-SMALLDELTA;
          if (wall.ybottom >= zCamera)
          {
            wall.flag=GLDWF_TOPFLUD;
            temptex=gld_RegisterFlat(flattranslation[seg->backsector->ceilingpic], true, indexed);
            if (temptex)
            {
              wall.gltexture=temptex;
              gld_AddDrawWallItem(GLDIT_FWALL, &wall);
            }
          }
        }
        else
        if (temptex)
        {
          wall.gltexture=temptex;
          CALC_Y_VALUES(wall, lineheight, floor_height, ceiling_height);
          CALC_TEX_VALUES_TOP(
            wall, seg, backseg, (LINE->flags & (/*e6y ML_DONTPEGBOTTOM | */ML_DONTPEGTOP))==0,
            linelength, lineheight
          );
          gld_AddDrawWallItem(GLDIT_WALL, &wall);
        }
      }
    }

    /* midtexture */
    //e6y
    if (comp[comp_maskedanim])
      temptex=gld_RegisterTexture(seg->sidedef->midtexture, true, false, indexed);
    else

    // e6y
    // Animated middle textures with a zero index should be forced
    // See spacelab.wad (http://www.doomworld.com/idgames/index.php?id=6826)
    temptex=gld_RegisterTexture(midtexture, true, true, indexed);
    if (temptex && seg->sidedef->midtexture != NO_TEXTURE && backsector->ceilingheight>frontsector->floorheight)
    {
      int top, bottom;
      wall.gltexture=temptex;

      if ( (LINE->flags & ML_DONTPEGBOTTOM) >0)
      {
        //floor_height=max_floor_tex;
        floor_height=MAX(seg->frontsector->floorheight, seg->backsector->floorheight)+(seg->sidedef->rowoffset);
        ceiling_height=floor_height+(wall.gltexture->realtexheight<<FRACBITS);
      }
      else
      {
        //ceiling_height=min_ceiling_tex;
        ceiling_height=MIN(seg->frontsector->ceilingheight, seg->backsector->ceilingheight)+(seg->sidedef->rowoffset);
        floor_height=ceiling_height-(wall.gltexture->realtexheight<<FRACBITS);
      }

      // Depending on missing textures and possible plane intersections
      // decide which planes to use for the polygon
      if (seg->frontsector != seg->backsector ||
        seg->frontsector->heightsec != -1)
      {
        sector_t *f, *b;

        f = (seg->frontsector->heightsec == -1 ? seg->frontsector : &ftempsec);
        b = (seg->backsector->heightsec == -1 ? seg->backsector : &btempsec);

        // Set up the top
        if (frontsector->ceilingpic != skyflatnum ||
            backsector->ceilingpic != skyflatnum)
        {
          if (toptexture == NO_TEXTURE)
            // texture is missing - use the higher plane
            top = MAX(f->ceilingheight, b->ceilingheight);
          else
            top = MIN(f->ceilingheight, b->ceilingheight);
        }
        else
          top = ceiling_height;

        // Set up the bottom
        if (frontsector->floorpic != skyflatnum ||
            backsector->floorpic != skyflatnum ||
            frontsector->floorheight != backsector->floorheight)
        {
          if (seg->sidedef->bottomtexture == NO_TEXTURE)
            // texture is missing - use the lower plane
            bottom = MIN(f->floorheight, b->floorheight);
          else
            // normal case - use the higher plane
            bottom = MAX(f->floorheight, b->floorheight);
        }
        else
        {
          bottom = floor_height;
        }

        //let's clip away some unnecessary parts of the polygon
        if (ceiling_height < top)
          top = ceiling_height;
        if (floor_height > bottom)
          bottom = floor_height;
      }
      else
      {
        // both sides of the line are in the same sector
        top = ceiling_height;
        bottom = floor_height;
      }

      if (top <= bottom)
        goto bottomtexture;

      wall.ytop = (float)top/(float)MAP_SCALE;
      wall.ybottom = (float)bottom/(float)MAP_SCALE;

      wall.flag = GLDWF_M2S;
      URUL(wall, seg, backseg, linelength);

      wall.vt = (float)((-top + ceiling_height))/(float)wall.gltexture->realtexheight;
      wall.vb = (float)((-bottom + ceiling_height))/(float)wall.gltexture->realtexheight;

      /* Adjust the final float value accounting for the fixed point conversion */
      wall.vt /= FRACUNIT;
      wall.vb /= FRACUNIT;

      if (seg->linedef->tranlump >= 0)
        wall.alpha=(float)tran_filter_pct/100.0f;
      gld_AddDrawWallItem((wall.alpha == 1.0f ? GLDIT_MWALL : GLDIT_TWALL), &wall);
      wall.alpha=1.0f;
    }
bottomtexture:
    /* bottomtexture */
    ceiling_height=backsector->floorheight;
    floor_height=frontsector->floorheight;
    if (frontsector->floorpic==skyflatnum)
    {
      wall.ybottom=-MAXCOORD;
      if (
          (backsector->ceilingheight==backsector->floorheight) &&
          (backsector->floorpic==skyflatnum)
         )
      {
        wall.ytop=(float)backsector->floorheight/MAP_SCALE;
        gld_AddSkyTexture(&wall, frontsector->sky, backsector->sky, SKY_FLOOR);
      }
      else
      {
        if (bottomtexture == NO_TEXTURE && midtexture == NO_TEXTURE)
        {
          wall.ytop=(float)max_floor/MAP_SCALE;
          gld_AddSkyTexture(&wall, frontsector->sky, backsector->sky, SKY_CEILING);
        }
        else
        {
          if ((bottomtexture != NO_TEXTURE && midtexture == NO_TEXTURE) ||
            backsector->floorpic != skyflatnum ||
            backsector->floorheight >= frontsector->ceilingheight)
          {
            wall.ytop=(float)min_floor/MAP_SCALE;
            gld_AddSkyTexture(&wall, frontsector->sky, backsector->sky, SKY_FLOOR);
          }
        }
      }
    }
    if (floor_height<ceiling_height)
    {
      temptex=gld_RegisterTexture(bottomtexture, true, false, indexed);
      if (!temptex && gl_use_stencil && backsector &&
        !(seg->linedef->r_flags & RF_ISOLATED) &&
        /*frontsector->floorpic != skyflatnum && */backsector->floorpic != skyflatnum &&
        !(backsector->flags & NULL_SECTOR))
      {
        wall.ytop=((float)(ceiling_height)/(float)MAP_SCALE)+SMALLDELTA;
        wall.ybottom=((float)(floor_height)/(float)MAP_SCALE)-SMALLDELTA;
        if (wall.ytop <= zCamera)
        {
          wall.flag = GLDWF_BOTFLUD;
          temptex=gld_RegisterFlat(flattranslation[seg->backsector->floorpic], true, indexed);
          if (temptex)
          {
            wall.gltexture=temptex;
            gld_AddDrawWallItem(GLDIT_FWALL, &wall);
          }
        }
      }
      else
      if (temptex)
      {
        fixed_t rowoffset = seg->sidedef->rowoffset;
        wall.gltexture=temptex;
        if (fix_sky_bleed)
        {
          ceiling_height = MIN(frontsector->ceilingheight, backsector->ceilingheight);
          seg->sidedef->rowoffset += (MAX(frontsector->floorheight, backsector->floorheight) - min_ceiling);
        }
        CALC_Y_VALUES(wall, lineheight, floor_height, ceiling_height);
        CALC_TEX_VALUES_BOTTOM(
          wall, seg, backseg, (LINE->flags & ML_DONTPEGBOTTOM)>0,
          linelength, lineheight,
          floor_height-frontsector->ceilingheight
        );
        gld_AddDrawWallItem(GLDIT_WALL, &wall);
        seg->sidedef->rowoffset = rowoffset;
      }
    }
  }
}

#undef LINE
#undef CALC_Y_VALUES
#undef OU
#undef OV
#undef OV_PEG
#undef CALC_TEX_VALUES_TOP
#undef CALC_TEX_VALUES_MIDDLE1S
#undef CALC_TEX_VALUES_BOTTOM
#undef ADDWALL

/*****************
 *               *
 * Flats         *
 *               *
 *****************/

static void gld_DrawFlat(GLFlat *flat)
{
  int loopnum; // current loop number
  GLLoopDef *currentloop; // the current loop
  dboolean has_detail;
  int has_offset;
  unsigned int flags;

  dsda_RecordVisPlane();

  has_detail =
    scene_has_details &&
    gl_arb_multitexture &&
    flat->gltexture->detail;

  has_offset = (has_detail || (flat->flags & GLFLAT_HAVE_OFFSET));

  if ((sectorloops[flat->sectornum].flags & SECTOR_CLAMPXY) && (!has_detail) &&
      (flat->gltexture->flags & GLTEXTURE_HIRES) &&
      !(flat->flags & GLFLAT_HAVE_OFFSET))
    flags = GLTEXTURE_CLAMPXY;
  else
    flags = 0;

  gld_BindFlat(flat->gltexture, flags);
  gld_StaticLightAlpha(flat->light, flat->alpha);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glTranslatef(0.0f,flat->z,0.0f);

  if (has_offset)
  {
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glTranslatef(flat->uoffs, flat->voffs, 0.0f);
  }

  gld_BindDetailARB(flat->gltexture, has_detail);
  if (has_detail)
  {
    float w, h, dx, dy;
    detail_t *detail = flat->gltexture->detail;

    GLEXT_glActiveTextureARB(GL_TEXTURE1_ARB);
    gld_StaticLightAlpha(flat->light, flat->alpha);

    glPushMatrix();

    w = flat->gltexture->detail_width;
    h = flat->gltexture->detail_height;
    dx = detail->offsetx;
    dy = detail->offsety;

    if ((flat->flags & GLFLAT_HAVE_OFFSET) || dx || dy)
    {
      glTranslatef(flat->uoffs * w + dx, flat->voffs * h + dy, 0.0f);
    }

    glScalef(w, h, 1.0f);
  }

  if (flat->sectornum>=0)
  {
    // go through all loops of this sector
    for (loopnum=0; loopnum<sectorloops[flat->sectornum].loopcount; loopnum++)
    {
      // set the current loop
      currentloop=&sectorloops[flat->sectornum].loops[loopnum];
      glDrawArrays(currentloop->mode,currentloop->vertexindex,currentloop->vertexcount);
    }
  }

  //e6y
  if (has_detail)
  {
    glPopMatrix();
    GLEXT_glActiveTextureARB(GL_TEXTURE0_ARB);
  }

  if (has_offset)
  {
    glPopMatrix();
  }

  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
}

// gld_AddFlat
//
// This draws on flat for the sector "num"
// The ceiling boolean indicates if the flat is a floor(false) or a ceiling(true)

static void gld_AddFlat(int sectornum, dboolean ceiling, visplane_t *plane)
{
  sector_t *sector; // the sector we want to draw
  sector_t tempsec; // needed for R_FakeFlat
  int floorlightlevel;      // killough 3/16/98: set floor lightlevel
  int ceilinglightlevel;    // killough 4/11/98
  GLFlat flat;
  dboolean indexed;

  if (sectornum<0)
    return;
  flat.sectornum=sectornum;
  sector=&sectors[sectornum]; // get the sector
  sector=R_FakeFlat(sector, &tempsec, &floorlightlevel, &ceilinglightlevel, false); // for boom effects
  flat.flags = (ceiling ? GLFLAT_CEILING : 0);

  indexed = V_IsWorldLightmodeIndexed();

  if (!ceiling) // if it is a floor ...
  {
    if (sector->floorpic == skyflatnum) // don't draw if sky
      return;
    // get the texture. flattranslation is maintained by doom and
    // contains the number of the current animation frame
    flat.gltexture=gld_RegisterFlat(flattranslation[plane->picnum], true, indexed);
    if (!flat.gltexture)
      return;
    // get the lightlevel from floorlightlevel
    flat.light=gld_CalcLightLevel(plane->lightlevel+gld_GetGunFlashLight());
    flat.fogdensity = gld_CalcFogDensity(sector, plane->lightlevel, GLDIT_FLOOR);
    // calculate texture offsets
    if (sector->floor_xoffs | sector->floor_yoffs)
    {
      flat.flags |= GLFLAT_HAVE_OFFSET;
      flat.uoffs=(float)sector->floor_xoffs/(float)(FRACUNIT*64);
      flat.voffs=(float)sector->floor_yoffs/(float)(FRACUNIT*64);
    }
    else
    {
      flat.uoffs=0.0f;
      flat.voffs=0.0f;

      if (map_format.hexen)
      {
        int scrollOffset = leveltime >> 1 & 63;

        switch (plane->special)
        {                       // Handle scrolling flats
          case 201:
          case 202:
          case 203:          // Scroll_North_xxx
            flat.flags |= GLFLAT_HAVE_OFFSET;
            flat.voffs = (float) (scrollOffset << (plane->special - 201) & 63) / 64;
            break;
          case 204:
          case 205:
          case 206:          // Scroll_East_xxx
            flat.flags |= GLFLAT_HAVE_OFFSET;
            flat.uoffs = (float) ((63 - scrollOffset) << (plane->special - 204) & 63) / 64;
            break;
          case 207:
          case 208:
          case 209:          // Scroll_South_xxx
            flat.flags |= GLFLAT_HAVE_OFFSET;
            flat.voffs = (float) ((63 - scrollOffset) << (plane->special - 207) & 63) / 64;
            break;
          case 210:
          case 211:
          case 212:          // Scroll_West_xxx
            flat.flags |= GLFLAT_HAVE_OFFSET;
            flat.uoffs = (float) (scrollOffset << (plane->special - 210) & 63) / 64;
            break;
          case 213:
          case 214:
          case 215:          // Scroll_NorthWest_xxx
            flat.flags |= GLFLAT_HAVE_OFFSET;
            flat.voffs = (float) (scrollOffset << (plane->special - 213) & 63) / 64;
            flat.uoffs = (float) (scrollOffset << (plane->special - 213) & 63) / 64;
            break;
          case 216:
          case 217:
          case 218:          // Scroll_NorthEast_xxx
            flat.flags |= GLFLAT_HAVE_OFFSET;
            flat.voffs = (float) (scrollOffset << (plane->special - 216) & 63) / 64;
            flat.uoffs = (float) ((63 - scrollOffset) << (plane->special - 216) & 63) / 64;
            break;
          case 219:
          case 220:
          case 221:          // Scroll_SouthEast_xxx
            flat.flags |= GLFLAT_HAVE_OFFSET;
            flat.voffs = (float) ((63 - scrollOffset) << (plane->special - 219) & 63) / 64;
            flat.uoffs = (float) ((63 - scrollOffset) << (plane->special - 219) & 63) / 64;
            break;
          case 222:
          case 223:
          case 224:          // Scroll_SouthWest_xxx
            flat.flags |= GLFLAT_HAVE_OFFSET;
            flat.voffs = (float) ((63 - scrollOffset) << (plane->special - 222) & 63) / 64;
            flat.uoffs = (float) (scrollOffset << (plane->special - 222) & 63) / 64;
            break;
          default:
            break;
        }
      }
      else if (heretic)
      {
        switch (plane->special)
        {
          case 20:
          case 21:
          case 22:
          case 23:
          case 24:           // Scroll_East
            flat.flags |= GLFLAT_HAVE_OFFSET;
            flat.uoffs = (float) ((63 - ((leveltime >> 1) & 63)) << (plane->special - 20) & 63) / 64;
            break;
          case 4:            // Scroll_EastLavaDamage
            flat.flags |= GLFLAT_HAVE_OFFSET;
            flat.uoffs = (float) (((63 - ((leveltime >> 1) & 63)) << 3) & 63) / 64;
            break;
        }
      }
    }
  }
  else // if it is a ceiling ...
  {
    if (sector->ceilingpic == skyflatnum) // don't draw if sky
      return;
    // get the texture. flattranslation is maintained by doom and
    // contains the number of the current animation frame
    flat.gltexture=gld_RegisterFlat(flattranslation[plane->picnum], true, indexed);
    if (!flat.gltexture)
      return;
    // get the lightlevel from ceilinglightlevel
    flat.light=gld_CalcLightLevel(plane->lightlevel+gld_GetGunFlashLight());
    flat.fogdensity = gld_CalcFogDensity(sector, plane->lightlevel, GLDIT_CEILING);
    // calculate texture offsets
    if (sector->ceiling_xoffs | sector->ceiling_yoffs)
    {
      flat.flags |= GLFLAT_HAVE_OFFSET;
      flat.uoffs=(float)sector->ceiling_xoffs/(float)(FRACUNIT*64);
      flat.voffs=(float)sector->ceiling_yoffs/(float)(FRACUNIT*64);
    }
    else
    {
      flat.uoffs=0.0f;
      flat.voffs=0.0f;
    }
  }

  // get height from plane
  flat.z=(float)plane->height/MAP_SCALE;

  if (gl_blend_animations)
  {
    anim_t *anim = anim_flats[flat.gltexture->index - firstflat].anim;
    if (anim)
    {
      int currpic, nextpic;

      flat.alpha = 1.0f - ((float)tic_vars.frac + ((leveltime - 1) % anim->speed) * 65536.0f) / (65536.0f * anim->speed);
      gld_AddDrawItem(((flat.flags & GLFLAT_CEILING) ? GLDIT_ACEILING : GLDIT_AFLOOR), &flat);

      currpic = flat.gltexture->index - firstflat - anim->basepic;
      nextpic = anim->basepic + (currpic + 1) % anim->numpics;
      flat.gltexture = gld_RegisterFlat(nextpic, true, indexed);
    }
  }

  flat.alpha = 1.0;

  if (flat.gltexture->detail)
    scene_has_flat_details++;

  gld_AddDrawItem(((flat.flags & GLFLAT_CEILING) ? GLDIT_CEILING : GLDIT_FLOOR), &flat);
}

void gld_AddPlane(int subsectornum, visplane_t *floor, visplane_t *ceiling)
{
  subsector_t *subsector;

  subsector = &subsectors[subsectornum];
  if (!subsector)
    return;

  // render the floor
  if (floor && floor->height < viewz)
    gld_AddFlat(subsector->sector->iSectorID, false, floor);
  // render the ceiling
  if (ceiling && ceiling->height > viewz)
    gld_AddFlat(subsector->sector->iSectorID, true, ceiling);
}

/*****************
 *               *
 * Sprites       *
 *               *
 *****************/

static void gld_DrawSprite(GLSprite *sprite)
{
  GLint blend_src, blend_dst;
  int restore = 0;

  dsda_RecordVisSprite();

  gld_BindPatch(sprite->gltexture,sprite->cm);

  if (!(sprite->flags & MF_NO_DEPTH_TEST))
  {
    if(sprite->flags & g_mf_shadow)
    {
      glsl_SetFuzzShaderActive();
      glsl_SetFuzzTextureDimensions((float)sprite->gltexture->width, (float)sprite->gltexture->height);
      glGetIntegerv(GL_BLEND_SRC, &blend_src);
      glGetIntegerv(GL_BLEND_DST, &blend_dst);
      glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
      glAlphaFunc(GL_GEQUAL,0.1f);
      glColor4f(0.2f,0.2f,0.2f,0.33f);
      restore = 1;
    }
    else
    {
      if(sprite->flags & g_mf_translucent)
        gld_StaticLightAlpha(sprite->light,(float)tran_filter_pct/100.0f);
      else
        gld_StaticLight(sprite->light);
    }
  }

  if (!(sprite->flags & (MF_SOLID | MF_SPAWNCEILING)))
  {
    float x1, x2, x3, x4, z1, z2, z3, z4;
    float y1, y2, cy, ycenter, y1c, y2c;
    float y1z2_y, y2z2_y;

    ycenter = (float)fabs(sprite->y1 - sprite->y2) * 0.5f;
    y1c = sprite->y1 - ycenter;
    y2c = sprite->y2 - ycenter;
    cy = sprite->y + ycenter;

    y1z2_y = -(y1c * sin_paperitems_pitch);
    y2z2_y = -(y2c * sin_paperitems_pitch);

    x1 = +(sprite->x1 * cos_inv_yaw - y1z2_y * sin_inv_yaw) + sprite->x;
    x2 = +(sprite->x2 * cos_inv_yaw - y1z2_y * sin_inv_yaw) + sprite->x;
    x3 = +(sprite->x1 * cos_inv_yaw - y2z2_y * sin_inv_yaw) + sprite->x;
    x4 = +(sprite->x2 * cos_inv_yaw - y2z2_y * sin_inv_yaw) + sprite->x;

    y1 = +(y1c * cos_paperitems_pitch) + cy;
    y2 = +(y2c * cos_paperitems_pitch) + cy;

    z1 = -(sprite->x1 * sin_inv_yaw + y1z2_y * cos_inv_yaw) + sprite->z;
    z2 = -(sprite->x2 * sin_inv_yaw + y1z2_y * cos_inv_yaw) + sprite->z;
    z3 = -(sprite->x1 * sin_inv_yaw + y2z2_y * cos_inv_yaw) + sprite->z;
    z4 = -(sprite->x2 * sin_inv_yaw + y2z2_y * cos_inv_yaw) + sprite->z;

    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(sprite->ul, sprite->vt); glVertex3f(x1, y1, z1);
    glTexCoord2f(sprite->ur, sprite->vt); glVertex3f(x2, y1, z2);
    glTexCoord2f(sprite->ul, sprite->vb); glVertex3f(x3, y2, z3);
    glTexCoord2f(sprite->ur, sprite->vb); glVertex3f(x4, y2, z4);
    glEnd();
  }
  else
  {
    float x1, x2, y1, y2, z1, z2;

    x1 = +(sprite->x1 * cos_inv_yaw) + sprite->x;
    x2 = +(sprite->x2 * cos_inv_yaw) + sprite->x;

    y1 = sprite->y + sprite->y1;
    y2 = sprite->y + sprite->y2;

    z2 = -(sprite->x1 * sin_inv_yaw) + sprite->z;
    z1 = -(sprite->x2 * sin_inv_yaw) + sprite->z;

    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(sprite->ul, sprite->vt); glVertex3f(x1, y1, z2);
    glTexCoord2f(sprite->ur, sprite->vt); glVertex3f(x2, y1, z1);
    glTexCoord2f(sprite->ul, sprite->vb); glVertex3f(x1, y2, z2);
    glTexCoord2f(sprite->ur, sprite->vb); glVertex3f(x2, y2, z1);
    glEnd();
  }

  if (restore)
  {
    glBlendFunc(blend_src, blend_dst);
    glAlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold_f);
    glsl_SetFuzzShaderInactive();
  }
}

static void gld_AddHealthBar(mobj_t* thing, GLSprite *sprite)
{
  if (((thing->flags & (MF_COUNTKILL | MF_CORPSE)) == MF_COUNTKILL) && (thing->health > 0))
  {
    GLHealthBar hbar;
    int health_percent = thing->health * 100 / thing->info->spawnhealth;

    hbar.cm = -1;
    if (health_percent <= 50)
      hbar.cm = CR_RED;
    else if (health_percent <= 99)
      hbar.cm = CR_YELLOW;
    else if (health_percent <= 0)
      hbar.cm = CR_GREEN;

    if (hbar.cm >= 0)
    {
      float sx2 = (float)thing->radius / 2.0f / MAP_SCALE;
      float sx1 = sx2 - (float)health_percent * (float)thing->radius / 100.0f / MAP_SCALE;
      float sx3 = -sx2;

      hbar.x1 = +(sx1 * cos_inv_yaw) + sprite->x;
      hbar.x2 = +(sx2 * cos_inv_yaw) + sprite->x;
      hbar.x3 = +(sx3 * cos_inv_yaw) + sprite->x;

      hbar.z1 = -(sx1 * sin_inv_yaw) + sprite->z;
      hbar.z2 = -(sx2 * sin_inv_yaw) + sprite->z;
      hbar.z3 = -(sx3 * sin_inv_yaw) + sprite->z;

      hbar.y = sprite->y + sprite->y1 + 2.0f / MAP_COEFF;

      gld_AddDrawItem(GLDIT_HBAR, &hbar);
    }
  }
}

static void gld_DrawHealthBars(void)
{
  int i, count;
  int cm = -1;

  count = gld_drawinfo.num_items[GLDIT_HBAR];
  if (count > 0)
  {
    gld_EnableTexture2D(GL_TEXTURE0_ARB, false);

    glBegin(GL_LINES);
    for (i = count - 1; i >= 0; i--)
    {
      GLHealthBar *hbar = gld_drawinfo.items[GLDIT_HBAR][i].item.hbar;
      if (hbar->cm != cm)
      {
        cm = hbar->cm;
        glColor4f(cm2RGB[cm][0], cm2RGB[cm][1], cm2RGB[cm][2], 1.0f);
      }

      glVertex3f(hbar->x1, hbar->y, hbar->z1);
      glVertex3f(hbar->x2, hbar->y, hbar->z2);
    }
    glEnd();

    glColor4f(0.5f, 0.5f, 0.5f, 1.0f);
    glBegin(GL_LINES);
    for (i = count - 1; i >= 0; i--)
    {
      GLHealthBar *hbar = gld_drawinfo.items[GLDIT_HBAR][i].item.hbar;

      glVertex3f(hbar->x1, hbar->y, hbar->z1);
      glVertex3f(hbar->x3, hbar->y, hbar->z3);
    }
    glEnd();

    gld_EnableTexture2D(GL_TEXTURE0_ARB, true);
  }
}

static void gld_DrawBFGTracers(void)
{
  int i;
  float sx, sy, sz;

  // Show last BFG tracers for 2 seconds
  if (dsda_bfg_tracers.show && gametic < dsda_bfg_tracers.start_tick + 2 * TICRATE) {
    gld_EnableTexture2D(GL_TEXTURE0_ARB, false);

    glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
    glBegin(GL_LINES);

    sx = -(float)dsda_bfg_tracers.src_x / MAP_SCALE;
    sy = (float)dsda_bfg_tracers.src_z / MAP_SCALE;
    sz = (float)dsda_bfg_tracers.src_y / MAP_SCALE;

    for (i = 0; i < 40; i++) {
      if (dsda_bfg_tracers.hit[i] == true) {
        glVertex3f(sx, sy, sz);
        glVertex3f(-(float)dsda_bfg_tracers.dst_x[i] / MAP_SCALE,
          (float)dsda_bfg_tracers.dst_z[i] / MAP_SCALE,
          (float)dsda_bfg_tracers.dst_y[i] / MAP_SCALE);
      }
    }

    glEnd();

    glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
    glBegin(GL_LINES);

    for (i = 0; i < 40; i++) {
      if (dsda_bfg_tracers.hit[i] == false) {
        glVertex3f(sx, sy, sz);
        glVertex3f(-(float)dsda_bfg_tracers.dst_x[i] / MAP_SCALE,
          (float)dsda_bfg_tracers.dst_z[i] / MAP_SCALE,
          (float)dsda_bfg_tracers.dst_y[i] / MAP_SCALE);
      }
    }

    glEnd();

    gld_EnableTexture2D(GL_TEXTURE0_ARB, true);
  }
}

void gld_ProjectSprite(mobj_t* thing, int lightlevel)
{
  fixed_t   tx;
  spritedef_t   *sprdef;
  spriteframe_t *sprframe;
  int       lump;
  dboolean   flip;
  int heightsec;      // killough 3/27/98

  // transform the origin point
  //e6y
  fixed_t tr_x, tr_y;
  fixed_t fx, fy, fz;
  fixed_t gxt, gyt;
  fixed_t tz;

  GLSprite sprite;
  const rpatch_t* patch;

  int frustum_culling = HaveMouseLook();
  int mlook = HaveMouseLook() || (gl_render_fov > FOV90);

  if (R_ViewInterpolation())
  {
    fx = thing->PrevX + FixedMul (tic_vars.frac, thing->x - thing->PrevX);
    fy = thing->PrevY + FixedMul (tic_vars.frac, thing->y - thing->PrevY);
    fz = thing->PrevZ + FixedMul (tic_vars.frac, thing->z - thing->PrevZ);
  }
  else
  {
    fx = thing->x;
    fy = thing->y;
    fz = thing->z;
  }

  tr_x = fx - viewx;
  tr_y = fy - viewy;

  gxt = FixedMul(tr_x, viewcos);
  gyt = -FixedMul(tr_y, viewsin);

  tz = gxt - gyt;

  // thing is behind view plane?
  if (tz < r_near_clip_plane)
    return;

  gxt = -FixedMul(tr_x, viewsin);
  gyt = FixedMul(tr_y, viewcos);
  tx = -(gyt + gxt);

  //e6y
  if (mlook)
  {
    if (tz >= MINZ && (D_abs(tx) >> 5) > tz)
      return;
  }
  else
  {
    // too far off the side?
    if (D_abs(tx) > (tz << 2))
      return;
  }

  // decide which patch to use for sprite relative to player
#ifdef RANGECHECK
  if ((unsigned) thing->sprite >= (unsigned)num_sprites)
    I_Error ("R_ProjectSprite: Invalid sprite number %i", thing->sprite);
#endif

  sprdef = &sprites[thing->sprite];

#ifdef RANGECHECK
  if ((thing->frame&FF_FRAMEMASK) >= sprdef->numframes)
    I_Error ("R_ProjectSprite: Invalid sprite frame %i : %i", thing->sprite, thing->frame);
#endif

  if (!sprdef->spriteframes)
    I_Error("R_ProjectSprite: Missing spriteframes %i : %i", thing->sprite, thing->frame);

  sprframe = &sprdef->spriteframes[thing->frame & FF_FRAMEMASK];

  if (sprframe->rotate)
  {
    // choose a different rotation based on player view
    angle_t rot;
    angle_t ang = R_PointToAngle2(viewx, viewy, fx, fy);
    if (sprframe->lump[0] == sprframe->lump[1])
    {
      rot = (ang - thing->angle + (angle_t)(ANG45/2)*9) >> 28;
    }
    else
    {
      rot = (ang - thing->angle + (angle_t)(ANG45 / 2) * 9 -
        (angle_t)(ANG180 / 16)) >> 28;
    }
    lump = sprframe->lump[rot];
    flip = (dboolean)(sprframe->flip & (1 << rot));
  }
  else
  {
    // use single rotation for all views
    lump = sprframe->lump[0];
    flip = (dboolean)(sprframe->flip & 1);
  }
  lump += firstspritelump;

  patch = R_PatchByNum(lump);
  thing->patch_width = patch->width;

  // killough 4/9/98: clip things which are out of view due to height
  if(!mlook)
  {
    int x1, x2;
    fixed_t xscale = FixedDiv(projection, tz);
    /* calculate edges of the shape
    * cph 2003/08/1 - fraggle points out that this offset must be flipped
    * if the sprite is flipped; e.g. FreeDoom imp is messed up by this. */
    if (flip)
      tx -= (patch->width - patch->leftoffset) << FRACBITS;
    else
      tx -= patch->leftoffset << FRACBITS;

    x1 = (centerxfrac + FixedMul(tx, xscale)) >> FRACBITS;
    tx += patch->width << FRACBITS;
    x2 = ((centerxfrac + FixedMul (tx, xscale) - FRACUNIT/2) >> FRACBITS);

    // off the side?
    if (x1 > viewwidth || x2 < 0)
      return;
  }

  // killough 3/27/98: exclude things totally separated
  // from the viewer, by either water or fake ceilings
  // killough 4/11/98: improve sprite clipping for underwater/fake ceilings

  heightsec = thing->subsector->sector->heightsec;
  if (heightsec != -1)   // only clip things which are in special sectors
  {
    int phs = viewplayer->mo->subsector->sector->heightsec;
    fixed_t gzt = fz + (patch->topoffset << FRACBITS);
    if (phs != -1 && viewz < sectors[phs].floorheight ?
        fz >= sectors[heightsec].floorheight :
        gzt < sectors[heightsec].floorheight)
      return;
    if (phs != -1 && viewz > sectors[phs].ceilingheight ?
        gzt < sectors[heightsec].ceilingheight && viewz >= sectors[heightsec].ceilingheight :
        fz >= sectors[heightsec].ceilingheight)
      return;
  }

  //e6y FIXME!!!
  if (thing == players[displayplayer].mo && walkcamera.type != 2)
    return;

  sprite.x =-(float)fx / MAP_SCALE;
  sprite.y = (float)fz / MAP_SCALE;
  sprite.z = (float)fy / MAP_SCALE;

  sprite.x2 = (float)patch->leftoffset / MAP_COEFF;
  sprite.x1 = sprite.x2 - ((float)patch->width / MAP_COEFF);
  sprite.y1 = (float)patch->topoffset / MAP_COEFF;
  sprite.y2 = sprite.y1 - ((float)patch->height / MAP_COEFF);

  // e6y
  // if the sprite is below the floor, and it's not a hanger/floater/missile,
  // and it's not a fully dead corpse, move it up
  if (
    sprite.y2 < 0 &&
    sprite.y2 >= -gl_spriteclip_threshold_f &&
    !(thing->flags & (MF_SPAWNCEILING|MF_FLOAT|MF_MISSILE|MF_NOGRAVITY)) &&
    !(thing->flags & MF_CORPSE && thing->tics == -1)
  )
  {
    sprite.y1 -= sprite.y2;
    sprite.y2 = 0.0f;
  }

  if (frustum_culling)
  {
    if (!gld_SphereInFrustum(
      sprite.x + cos_inv_yaw * (sprite.x1 + sprite.x2) / 2.0f,
      sprite.y + (sprite.y1 + sprite.y2) / 2.0f,
      sprite.z - sin_inv_yaw * (sprite.x1 + sprite.x2) / 2.0f,
      //1.5 == sqrt(2) + small delta for MF_FOREGROUND
      (float)(MAX(patch->width, patch->height)) / MAP_COEFF / 2.0f * 1.5f))
    {
      return;
    }
  }

  sprite.scale = FixedDiv(projectiony, tz);;
  if ((thing->frame & FF_FULLBRIGHT) || dsda_ShowAliveMonsters())
  {
    sprite.fogdensity = 0.0f;
    sprite.light = 1.0f;
  }
  else
  {
    sprite.light = gld_CalcLightLevel(lightlevel+gld_GetGunFlashLight());
    sprite.fogdensity = gld_CalcFogDensity(thing->subsector->sector, lightlevel, GLDIT_SPRITE);
  }
  if (thing->color)
    sprite.cm = thing->color;
  else
    sprite.cm = CR_LIMIT + (int)((thing->flags & MF_TRANSLATION) >> (MF_TRANSSHIFT));
  sprite.gltexture = gld_RegisterPatch(lump, sprite.cm, true, V_IsWorldLightmodeIndexed());
  if (!sprite.gltexture)
    return;
  sprite.flags = thing->flags;

  if (thing->flags & MF_FOREGROUND)
    scene_has_overlapped_sprites = true;

  sprite.index = gl_spriteindex++;
  sprite.xy = thing->x + (thing->y >> 16);
  sprite.fx = thing->x;
  sprite.fy = thing->y;

  sprite.vt = 0.0f;
  sprite.vb = sprite.gltexture->scaleyfac;
  if (flip)
  {
    sprite.ul = 0.0f;
    sprite.ur = sprite.gltexture->scalexfac;
  }
  else
  {
    sprite.ul = sprite.gltexture->scalexfac;
    sprite.ur = 0.0f;
  }

  //e6y: support for transparent sprites
  if (sprite.flags & MF_NO_DEPTH_TEST)
  {
    gld_AddDrawItem(GLDIT_ASPRITE, &sprite);
  }
  else
  {
    gld_AddDrawItem(((sprite.flags & (MF_SHADOW | MF_TRANSLUCENT)) ? GLDIT_TSPRITE : GLDIT_SPRITE), &sprite);
  }

  if (dsda_ShowHealthBars())
  {
    gld_AddHealthBar(thing, &sprite);
  }
}

/*****************
 *               *
 * Draw          *
 *               *
 *****************/

//e6y
void gld_ProcessWall(GLWall *wall)
{
  // e6y
  // The ultimate 'ATI sucks' fix: Some of ATIs graphics cards are so unprecise when
  // rendering geometry that each and every border between polygons must be seamless,
  // otherwise there are rendering artifacts worse than anything that could be seen
  // on Geforce 2's! Made this a menu option because the speed impact is quite severe
  // and this special handling is not necessary on modern NVidia cards.
  seg_t *seg = wall->seg;

  wall->glseg->fracleft  = 0;
  wall->glseg->fracright = 0;

  gld_RecalcVertexHeights(seg->linedef->v1);
  gld_RecalcVertexHeights(seg->linedef->v2);

  gld_DrawWall(wall);
}

static int C_DECL dicmp_wall(const void *a, const void *b)
{
  GLTexture *tx1 = ((const GLDrawItem *)a)->item.wall->gltexture;
  GLTexture *tx2 = ((const GLDrawItem *)b)->item.wall->gltexture;
  return tx1 - tx2;
}
static int C_DECL dicmp_flat(const void *a, const void *b)
{
  GLTexture *tx1 = ((const GLDrawItem *)a)->item.flat->gltexture;
  GLTexture *tx2 = ((const GLDrawItem *)b)->item.flat->gltexture;
  return tx1 - tx2;
}
static int C_DECL dicmp_sprite(const void *a, const void *b)
{
  GLTexture *tx1 = ((const GLDrawItem *)a)->item.sprite->gltexture;
  GLTexture *tx2 = ((const GLDrawItem *)b)->item.sprite->gltexture;
  return tx1 - tx2;
}

static int C_DECL dicmp_sprite_scale(const void *a, const void *b)
{
  GLSprite *sprite1 = ((const GLDrawItem *)a)->item.sprite;
  GLSprite *sprite2 = ((const GLDrawItem *)b)->item.sprite;

  if (sprite1->scale != sprite2->scale)
  {
    return sprite2->scale - sprite1->scale;
  }
  else
  {
    return sprite1->gltexture - sprite2->gltexture;
  }
}

static void gld_DrawItemsSortByTexture(GLDrawItemType itemtype)
{
  typedef int(C_DECL *DICMP_ITEM)(const void *a, const void *b);

  static DICMP_ITEM itemfuncs[GLDIT_TYPES] = {
    0,
    dicmp_wall, dicmp_wall, dicmp_wall, dicmp_wall, dicmp_wall,
    dicmp_wall, dicmp_wall,
    dicmp_flat, dicmp_flat,
    dicmp_flat, dicmp_flat,
    dicmp_sprite, dicmp_sprite_scale, dicmp_sprite,
    0,
    0,
  };

  if (itemfuncs[itemtype] && gld_drawinfo.num_items[itemtype] > 1)
  {
    qsort(gld_drawinfo.items[itemtype], gld_drawinfo.num_items[itemtype],
      sizeof(gld_drawinfo.items[itemtype][0]), itemfuncs[itemtype]);
  }
}

static int no_overlapped_sprites;
static int C_DECL dicmp_sprite_by_pos(const void *a, const void *b)
{
  GLSprite *s1 = ((const GLDrawItem *)a)->item.sprite;
  GLSprite *s2 = ((const GLDrawItem *)b)->item.sprite;
  int res = s2->xy - s1->xy;
  no_overlapped_sprites = no_overlapped_sprites && res;
  return res;
}

static void gld_DrawItemsSort(GLDrawItemType itemtype, int (C_DECL *PtFuncCompare)(const void *, const void *))
{
  qsort(gld_drawinfo.items[itemtype], gld_drawinfo.num_items[itemtype],
    sizeof(gld_drawinfo.items[itemtype][0]), PtFuncCompare);
}

static void gld_DrawItemsSortSprites(GLDrawItemType itemtype)
{
  static const float delta = 0.2f / MAP_COEFF;
  int i;

  if (scene_has_overlapped_sprites)
  {
    for (i = 0; i < gld_drawinfo.num_items[itemtype]; i++)
    {
      GLSprite *sprite = gld_drawinfo.items[itemtype][i].item.sprite;
      if (sprite->flags & MF_FOREGROUND)
      {
        sprite->index = gl_spriteindex;
        sprite->x -= delta * sin_inv_yaw;
        sprite->z -= delta * cos_inv_yaw;
      }
    }
  }

  gld_DrawItemsSortByTexture(itemtype);
}

//
// projected walls
//
void gld_DrawProjectedWalls(GLDrawItemType itemtype)
{
  int i;

  if (gl_use_stencil && gld_drawinfo.num_items[itemtype] > 0)
  {
    // Push bleeding floor/ceiling textures back a little in the z-buffer
    // so they don't interfere with overlapping mid textures.
    glPolygonOffset(1.0f, 128.0f);
    glEnable(GL_POLYGON_OFFSET_FILL);

    glEnable(GL_STENCIL_TEST);
    gld_DrawItemsSortByTexture(itemtype);
    for (i = gld_drawinfo.num_items[itemtype] - 1; i >= 0; i--)
    {
      GLWall *wall = gld_drawinfo.items[itemtype][i].item.wall;

      if (gl_use_fog)
      {
        // calculation of fog density for flooded walls
        if (wall->seg->backsector)
        {
          wall->fogdensity = gld_CalcFogDensity(wall->seg->frontsector,
            wall->seg->backsector->lightlevel, itemtype);
        }

        gld_SetFog(wall->fogdensity);
      }

      gld_ProcessWall(wall);
    }
    glDisable(GL_STENCIL_TEST);

    glPolygonOffset(0.0f, 0.0f);
    glDisable(GL_POLYGON_OFFSET_FILL);
  }
}

void gld_DrawScene(player_t *player)
{
  int i;
  int skybox;

  //e6y: must call it twice for correct initialisation
  glEnable(GL_ALPHA_TEST);

  //e6y: the same with fog
  gl_EnableFog(true);
  gl_EnableFog(false);

  gld_EnableDetail(false);
  gld_InitFrameDetails();

  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glEnableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_COLOR_ARRAY);

  //e6y: skybox
  skybox = 0;
  if (gl_drawskys != skytype_none)
  {
    skybox = gld_DrawBoxSkyBox();
  }

  if (!skybox)
  {
    if (gl_drawskys == skytype_skydome)
    {
      gld_DrawDomeSkyBox();
    }
    //e6y: 3d emulation of screen quad
    if (gl_drawskys == skytype_screen)
    {
      gld_DrawScreenSkybox();
    }
  }

  if (gl_ext_arb_vertex_buffer_object)
  {
    GLEXT_glBindBufferARB(GL_ARRAY_BUFFER, flats_vbo_id);
  }
  glVertexPointer(3, GL_FLOAT, sizeof(flats_vbo[0]), flats_vbo_x);
  glTexCoordPointer(2, GL_FLOAT, sizeof(flats_vbo[0]), flats_vbo_u);

  glsl_SetMainShaderActive();

  //
  // opaque stuff
  //

  glBlendFunc(GL_ONE, GL_ZERO);

  // solid geometry
  glDisable(GL_ALPHA_TEST);

  // enable backside removing
  glEnable(GL_CULL_FACE);

  // floors
  glCullFace(GL_FRONT);
  gld_DrawItemsSortByTexture(GLDIT_FLOOR);
  for (i = gld_drawinfo.num_items[GLDIT_FLOOR] - 1; i >= 0; i--)
  {
    gld_SetFog(gld_drawinfo.items[GLDIT_FLOOR][i].item.flat->fogdensity);
    gld_DrawFlat(gld_drawinfo.items[GLDIT_FLOOR][i].item.flat);
  }

  // ceilings
  glCullFace(GL_BACK);
  gld_DrawItemsSortByTexture(GLDIT_CEILING);
  for (i = gld_drawinfo.num_items[GLDIT_CEILING] - 1; i >= 0; i--)
  {
    gld_SetFog(gld_drawinfo.items[GLDIT_CEILING][i].item.flat->fogdensity);
    gld_DrawFlat(gld_drawinfo.items[GLDIT_CEILING][i].item.flat);
  }

  // disable backside removing
  glDisable(GL_CULL_FACE);

  // detail texture works only with flats and walls
  gld_EnableDetail(false);

  // top, bottom, one-sided walls
  gld_DrawItemsSortByTexture(GLDIT_WALL);
  for (i = gld_drawinfo.num_items[GLDIT_WALL] - 1; i >= 0; i--)
  {
    gld_SetFog(gld_drawinfo.items[GLDIT_WALL][i].item.wall->fogdensity);
    gld_ProcessWall(gld_drawinfo.items[GLDIT_WALL][i].item.wall);
  }

  // masked geometry
  glEnable(GL_ALPHA_TEST);

  gld_DrawItemsSortByTexture(GLDIT_MWALL);

  if (!gl_arb_multitexture && render_usedetail && gl_use_stencil &&
      gld_drawinfo.num_items[GLDIT_MWALL] > 0)
  {
    // opaque mid walls without holes
    for (i = gld_drawinfo.num_items[GLDIT_MWALL] - 1; i >= 0; i--)
    {
      GLWall *wall = gld_drawinfo.items[GLDIT_MWALL][i].item.wall;
      if (!(wall->gltexture->flags & GLTEXTURE_HASHOLES))
      {
        gld_SetFog(wall->fogdensity);
        gld_ProcessWall(wall);
      }
    }

    // opaque mid walls with holes

    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, ~0);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    for (i = gld_drawinfo.num_items[GLDIT_MWALL] - 1; i >= 0; i--)
    {
      GLWall *wall = gld_drawinfo.items[GLDIT_MWALL][i].item.wall;
      if (wall->gltexture->flags & GLTEXTURE_HASHOLES)
      {
        gld_SetFog(wall->fogdensity);
        gld_ProcessWall(wall);
      }
    }

    glStencilFunc(GL_EQUAL, 1, ~0);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    glBlendFunc (GL_DST_COLOR, GL_SRC_COLOR);

    // details for opaque mid walls with holes
    gld_DrawItemsSortByDetail(GLDIT_MWALL);
    for (i = gld_drawinfo.num_items[GLDIT_MWALL] - 1; i >= 0; i--)
    {
      GLWall *wall = gld_drawinfo.items[GLDIT_MWALL][i].item.wall;
      if (wall->gltexture->flags & GLTEXTURE_HASHOLES)
      {
        gld_SetFog(wall->fogdensity);
        gld_DrawWallDetail_NoARB(wall);
      }
    }

    //restoring
    SetFrameTextureMode();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClear(GL_STENCIL_BUFFER_BIT);
    glDisable(GL_STENCIL_TEST);
  }
  else
  {
    // opaque mid walls
    for (i = gld_drawinfo.num_items[GLDIT_MWALL] - 1; i >= 0; i--)
    {
      gld_SetFog(gld_drawinfo.items[GLDIT_MWALL][i].item.wall->fogdensity);
      gld_ProcessWall(gld_drawinfo.items[GLDIT_MWALL][i].item.wall);
    }
  }

  gl_EnableFog(false);
  gld_EnableDetail(false);

  // projected walls
  gld_DrawProjectedWalls(GLDIT_FWALL);

  gl_EnableFog(false);
  glEnable(GL_ALPHA_TEST);

  // normal sky (not a skybox)
  if (!skybox && (gl_drawskys == skytype_none || gl_drawskys == skytype_standard))
  {
    dsda_RecordDrawSegs(gld_drawinfo.num_items[GLDIT_SWALL]);
    // fake strips of sky
    glsl_SetActiveShader(NULL);
    gld_DrawStripsSky();
    glsl_SetMainShaderActive();
  }

  // opaque sprites
  glAlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold_f);
  gld_DrawItemsSortSprites(GLDIT_SPRITE);
  for (i = gld_drawinfo.num_items[GLDIT_SPRITE] - 1; i >= 0; i--)
  {
    gld_SetFog(gld_drawinfo.items[GLDIT_SPRITE][i].item.sprite->fogdensity);
    gld_DrawSprite(gld_drawinfo.items[GLDIT_SPRITE][i].item.sprite);
  }
  glAlphaFunc(GL_GEQUAL, 0.5f);

  // mode for viewing all the alive monsters
  if (dsda_ShowAliveMonsters())
  {
    const int period = 250;
    float color;
    int step = (SDL_GetTicks() % (period * 2)) + 1;
    if (step > period)
    {
      step = period * 2 - step;
    }
    color = 0.1f + 0.9f * (float)step / (float)period;

    R_AddAllAliveMonstersSprites();
    glDisable(GL_DEPTH_TEST);
    gld_DrawItemsSortByTexture(GLDIT_ASPRITE);
    glColor4f(1.0f, color, color, 1.0f);
    for (i = gld_drawinfo.num_items[GLDIT_ASPRITE] - 1; i >= 0; i--)
    {
      gld_DrawSprite(gld_drawinfo.items[GLDIT_ASPRITE][i].item.sprite);
    }
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
  }

  if (dsda_ShowHealthBars())
  {
    glsl_SetActiveShader(NULL);
    gld_DrawHealthBars();
    glsl_SetMainShaderActive();
  }

  if (dsda_ShowBFGTracers())
  {
    gld_DrawBFGTracers();
  }

  //
  // transparent stuff
  //

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  if (gl_blend_animations)
  {
    // enable backside removing
    glEnable(GL_CULL_FACE);

    // animated floors
    glCullFace(GL_FRONT);
    gld_DrawItemsSortByTexture(GLDIT_AFLOOR);
    for (i = gld_drawinfo.num_items[GLDIT_AFLOOR] - 1; i >= 0; i--)
    {
      gld_SetFog(gld_drawinfo.items[GLDIT_AFLOOR][i].item.flat->fogdensity);
      gld_DrawFlat(gld_drawinfo.items[GLDIT_AFLOOR][i].item.flat);
    }

    glCullFace(GL_BACK);
    gld_DrawItemsSortByTexture(GLDIT_ACEILING);
    for (i = gld_drawinfo.num_items[GLDIT_ACEILING] - 1; i >= 0; i--)
    {
      gld_SetFog(gld_drawinfo.items[GLDIT_ACEILING][i].item.flat->fogdensity);
      gld_DrawFlat(gld_drawinfo.items[GLDIT_ACEILING][i].item.flat);
    }

    // disable backside removing
    glDisable(GL_CULL_FACE);
  }

  if (gl_blend_animations)
  {
    gld_DrawItemsSortByTexture(GLDIT_AWALL);
    for (i = gld_drawinfo.num_items[GLDIT_AWALL] - 1; i >= 0; i--)
    {
      gld_SetFog(gld_drawinfo.items[GLDIT_AWALL][i].item.wall->fogdensity);
      gld_ProcessWall(gld_drawinfo.items[GLDIT_AWALL][i].item.wall);
    }

    // projected animated walls
    gld_DrawProjectedWalls(GLDIT_FAWALL);
  }

  /* Transparent sprites and transparent things must be rendered
   * in far-to-near order. The approach used here is to sort in-
   * place by comparing the next farthest items in the queue.
   * There are known limitations to this approach, but it is
   * a trade-off of accuracy for speed.
   * Refer to the discussion below for more detail.
   * https://github.com/coelckers/prboom-plus/pull/262
   */
  if (gld_drawinfo.num_items[GLDIT_TWALL] > 0 || gld_drawinfo.num_items[GLDIT_TSPRITE] > 0)
  {
    int twall_idx   = gld_drawinfo.num_items[GLDIT_TWALL] - 1;
    int tsprite_idx = gld_drawinfo.num_items[GLDIT_TSPRITE] - 1;

    if (tsprite_idx > 0)
      gld_DrawItemsSortSprites(GLDIT_TSPRITE);

    while (twall_idx >= 0 || tsprite_idx >= 0 )
    {
      dboolean draw_tsprite = false;

      /* find out what is next to draw */
      if (twall_idx >= 0 && tsprite_idx >= 0)
      {
        /* both are left to draw, determine
         * which is farther */
        seg_t *twseg = gld_drawinfo.items[GLDIT_TWALL][twall_idx].item.wall->seg;
        int ti;
        for (ti = tsprite_idx; ti >= 0; ti--) {
          /* reconstruct the sprite xy */
          fixed_t tsx = gld_drawinfo.items[GLDIT_TSPRITE][ti].item.sprite->fx;
          fixed_t tsy = gld_drawinfo.items[GLDIT_TSPRITE][ti].item.sprite->fy;

          if (R_PointOnSegSide(tsx, tsy, twseg))
          {
            /* a thing is behind the seg */
            /* do not draw the seg yet */
            draw_tsprite = true;
            break;
          }
        }
      }
      else if (tsprite_idx >= 0)
      {
        /* no transparent walls left, draw a sprite */
        draw_tsprite = true;
      }
      /* fall-through case is draw wall */

      if (draw_tsprite)
      {
        /* transparent sprite is farther, draw it */
        glAlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold_f);
        gld_SetFog(gld_drawinfo.items[GLDIT_TSPRITE][tsprite_idx].item.sprite->fogdensity);
        gld_DrawSprite(gld_drawinfo.items[GLDIT_TSPRITE][tsprite_idx].item.sprite);
        tsprite_idx--;
      }
      else
      {
        glDepthMask(GL_FALSE);
        /* transparent wall is farther, draw it */
        glAlphaFunc(GL_GREATER, 0.0f);
        gld_SetFog(gld_drawinfo.items[GLDIT_TWALL][twall_idx].item.wall->fogdensity);
        gld_ProcessWall(gld_drawinfo.items[GLDIT_TWALL][twall_idx].item.wall);
        glDepthMask(GL_TRUE);
        twall_idx--;
      }
    }
    glAlphaFunc(GL_GEQUAL, 0.5f);
    glEnable(GL_ALPHA_TEST);
  }

  // e6y: detail
  if (!gl_arb_multitexture && render_usedetail)
    gld_DrawDetail_NoARB();

  gld_EnableDetail(false);

  if (gl_ext_arb_vertex_buffer_object)
  {
    // bind with 0, so, switch back to normal pointer operation
    GLEXT_glBindBufferARB(GL_ARRAY_BUFFER, 0);
  }
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_COLOR_ARRAY);

  glsl_SetActiveShader(NULL);
}
