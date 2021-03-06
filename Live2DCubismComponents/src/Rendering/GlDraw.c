/*
 * Copyright(c) Live2D Inc. All rights reserved.
 * 
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at http://live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */


#include <Live2DCubismRendering.h>


// -------- //
// REQUIRES //
// -------- //

#include "Local.h"

#include <Live2DCubismCore.h>
#include <Live2DCubismRenderingInternal.h>


// ----- //
// TYPES //
// ----- //

/// Draw context.
typedef struct DrawContext
{
  /// Active renderer.
  const csmGlRenderer* Renderer;

  /// User matrix.
  const GLfloat* Mvp;

  /// Available textures.
  const GLuint* Textures;


  /// Currently active program.
  GlProgram ActiveProgram;

  /// Currently set blend mode.
  GLuint ActiveBlendMode;

  /// Currently set texture.
  GLuint ActiveTexture;

  /// Currently set opacity.
  float ActiveOpacity;
}
DrawContext;


// --------- //
// VARIABLES //
// --------- //

/// Blend scales table.
static GLenum BlendScale[3][4] =
{
  /// Normal blending.
  {GL_ONE , GL_ONE_MINUS_SRC_ALPHA , GL_ONE , GL_ONE_MINUS_SRC_ALPHA},

  /// Additive blending.
  {GL_SRC_ALPHA , GL_ONE , GL_ZERO , GL_ONE},

  /// Multiplicative blending.
  {GL_DST_COLOR , GL_ONE_MINUS_SRC_ALPHA , GL_ZERO , GL_ONE}
};


// --------- //
// FUNCTIONS //
// --------- //

/// Initializes a context.
///
/// @param  context   Context to initialize.
/// @param  renderer  Active renderer.
/// @param  mvp       User matrix.
/// @param  textures  Textures available for drawing.
static void InitializeDrawContext(DrawContext* context, const csmGlRenderer* renderer, const GLfloat* mvp, const GLuint* textures)
{
  // Initialize context.
  context->Renderer = renderer;
  context->Mvp = mvp;
  context->Textures = textures;


  context->ActiveProgram = 0;
  context->ActiveBlendMode = 3;
  context->ActiveTexture = 0;
  context->ActiveOpacity = -1.0f;


  // Initialize OpenGL state.
  ActivateGlProgram(GlNonMaskedProgram);
  SetGlMvp(mvp);
}


/// Sets OpenGL states for a render drawable.
///
/// @param  context         Current draw context.
/// @param  renderDrawable  Drawable to set state for.
static void SetGlState(DrawContext* context, const csmRenderDrawable* renderDrawable)
{
  const csmRenderDrawable* mask;
  int d, m, maskCount;
  GLuint maskTexture;
  GlProgram program;
  

  // Pick non-masked program as default.
  program = GlNonMaskedProgram;


  // Handle masking.
  d = (int)(renderDrawable - context->Renderer->RenderDrawables);
  maskCount = csmGetDrawableMaskCounts(context->Renderer->Model)[d];


  if (maskCount > 0)
  {
    // Set OpenGL state for drawing masks.
    // TODO  Only do this if necessary.
    ActivateGlMaskbuffer();


    ActivateGlProgram(GlMaskProgram);
    SetGlMvp(context->Mvp);
    SetGlOpacity(context->ActiveOpacity);
    SetGlDiffuseTexture(context->ActiveTexture);


    // Enforce normal blending.
    glBlendFuncSeparate(BlendScale[0][0],
                        BlendScale[0][1],
                        BlendScale[0][2],
                        BlendScale[0][3]);


    // Draw masks.
    for (m = 0; m < maskCount; ++m)
    {
      mask = &context->Renderer->RenderDrawables[csmGetDrawableMasks(context->Renderer->Model)[d][m]];


      glDrawElements(GL_TRIANGLES,
                    csmGetRenderDrawableGlIndexCount(mask),
                    GL_UNSIGNED_SHORT,
                    csmGetRenderDrawableGlIndicesOffset(mask));
    }


    // Fetch mask texture and trigger program change.
    maskTexture = DeactivateGlMaskbuffer();
    program = GlMaskedProgram;
  }


  // Set program.
  if (context->ActiveProgram != program)
  {
    context->ActiveProgram = program;


    // Set program, matrix (, and mask texture).
    ActivateGlProgram(program);
    SetGlMvp(context->Mvp);


    if (program == GlMaskedProgram)
    {
      SetGlMaskTexture(maskTexture);
    }


    // Force refresh of other states.
    context->ActiveBlendMode = 3;
    context->ActiveTexture = 0;
    context->ActiveOpacity = -1.0f;
  }


  // Set diffuse texture.
  if (context->Textures[renderDrawable->TextureIndex] != context->ActiveTexture)
  {
    context->ActiveTexture = context->Textures[renderDrawable->TextureIndex];


    SetGlDiffuseTexture(context->ActiveTexture);
  }


  // Set blend state.
  if (renderDrawable->BlendMode != context->ActiveBlendMode)
  {
    context->ActiveBlendMode = renderDrawable->BlendMode;


    glEnable(GL_BLEND);


    glBlendFuncSeparate(BlendScale[context->ActiveBlendMode][0],
                        BlendScale[context->ActiveBlendMode][1],
                        BlendScale[context->ActiveBlendMode][2],
                        BlendScale[context->ActiveBlendMode][3]);
  }


  // Set opacity.
  if (renderDrawable->Opacity != context->ActiveOpacity)
  {
    context->ActiveOpacity = renderDrawable->Opacity;


    SetGlOpacity(context->ActiveOpacity);
  }
}


// -------------- //
// IMPLEMENTATION //
// -------------- //

void csmGlDraw(const csmGlRenderer* renderer, const GLfloat* mvp, const GLuint* textures)
{
  const RenderDrawable* renderDrawable;
  DrawContext context;
  int d;


  // Don't draw barebone renderers...
  if (renderer->IsBarebone)
  {
    return;
  }


  // Prepare context and with it GL states.
  InitializeDrawContext(&context, renderer, mvp, textures);


  // Bind geometry.
  glBindVertexArray(renderer->VertexArray);


  // Draw.
  for (d = 0; d < renderer->DrawableCount; ++d)
  {
    // Fetch render drawable.
    renderDrawable = renderer->RenderDrawables + renderer->SortedDrawables[d].DrawableIndex;


    // Skip non-visible drawables.
    if (!renderDrawable->IsVisible)
    {
      continue;
    }


    // Update OpenGL state.
    SetGlState(&context, renderDrawable);


    // Draw geometry.
    glDrawElements(GL_TRIANGLES,
                   csmGetRenderDrawableGlIndexCount(renderDrawable),
                   GL_UNSIGNED_SHORT,
                   csmGetRenderDrawableGlIndicesOffset(renderDrawable));
  }
}
