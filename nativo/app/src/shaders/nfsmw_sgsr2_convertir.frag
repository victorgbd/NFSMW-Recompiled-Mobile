#version 450
// nfsmw - native renderer: Snapdragon GSR 2, convert pass (render resolution). See nfsmw_gsr2.cpp.
//
// Adapted from sgsr/v2/include/glsl_2_pass_fs/sgsr2_convert.fs of https://github.com/SnapdragonStudios/snapdragon-gsr
// (REQUEST_NDC_Y_UP: the game's matrices are Direct3D ones). Changes:
//   - No velocity texture: the game draws no motion vectors, so the motion always comes from the depth and
//     clipToPrevClip (the camera's motion; cars and traffic move with the camera's motion only).
//   - The depth comes from the scene's depth target, which can be larger than the frame (the 1280x1280 target
//     of a 1280x720 frame, or a supersampled scene): its uv = uv * profundidad_escala + profundidad_desplazamiento.
//   - Reversed depth (1 near, 0 far) is turned into the usual one for the comparisons (invertida).
//   - .w = HUD mask: 1 where the final frame (InputColor) differs from the composited scene the game copied right
//     before the HUD (EscenaSinHud). The upscale pass takes no history there.
//   - Texture coordinate from gl_FragCoord, with the presenter's vertex shader (guest_output_triangle_strip_rect_vs).
//
// The header (nfsmw_sgsr2_convertir_ps.h) is generated with (from nativo/):
//   python tools/compilar_shader_presentador.py app/src/shaders/nfsmw_sgsr2_convertir.frag app/src/shaders
//
// ---------------------------------------------------------------------------------------------------------------
// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
//
// Redistribution and use in source and binary forms, with or without modification, are permitted provided that
// the following conditions are met:
// 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the
//    following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
//    following disclaimer in the documentation and/or other materials provided with the distribution.
// 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
//    products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// ---------------------------------------------------------------------------------------------------------------

precision highp float;
precision highp int;

// nfsmw::gsr2::ParametrosGpu (std140, 176 bytes).
layout(set = 0, binding = 0, std140) uniform Parametros {
  vec4 clipToPrevClip[4];
  vec2 renderSize;
  vec2 outputSize;
  vec2 renderSizeRcp;
  vec2 outputSizeRcp;
  vec2 jitterOffset;
  vec2 scaleRatio;
  vec2 profundidad_escala;
  vec2 profundidad_desplazamiento;
  vec2 profundidad_rcp;  // 1 / size of the depth image
  vec2 relleno0;
  float cameraFovAngleHor;
  float minLerpContribution;
  float reset;
  float invertida;
  float mascara_umbral;
  float hay_escena;     // 1 if EscenaSinHud is the composited scene of this frame
  float escena_rb;      // 1 if EscenaSinHud has red and blue swapped relative to InputColor
  float relleno1;
} params;

layout(set = 0, binding = 1) uniform texture2D InputDepth;  // depth-only view of the scene's depth target
layout(set = 0, binding = 2) uniform texture2D InputColor;  // the final frame, with the HUD
layout(set = 0, binding = 3) uniform texture2D EscenaSinHud;
layout(set = 0, binding = 4) uniform sampler muestreador;

layout(location = 0) out vec4 MotionDepthClipAlphaBuffer;

void main() {
  highp vec2 texCoord = gl_FragCoord.xy * params.renderSizeRcp;
  ivec2 InputPos = ivec2(gl_FragCoord.xy);

  // Nearest depth of the 4x4 around the pixel, in the depth image.
  highp vec2 uv_profundidad = texCoord * params.profundidad_escala + params.profundidad_desplazamiento;
  highp vec2 gatherCoord = uv_profundidad - vec2(0.5) * params.profundidad_rcp;
  vec4 btmLeft = textureGather(sampler2D(InputDepth, muestreador), gatherCoord, 0);
  vec2 v10 = vec2(params.profundidad_rcp.x * 2.0, 0.0);
  vec4 btmRight = textureGather(sampler2D(InputDepth, muestreador), gatherCoord + v10, 0);
  vec2 v12 = vec2(0.0, params.profundidad_rcp.y * 2.0);
  vec4 topLeft = textureGather(sampler2D(InputDepth, muestreador), gatherCoord + v12, 0);
  vec2 v14 = vec2(params.profundidad_rcp.x * 2.0, params.profundidad_rcp.y * 2.0);
  vec4 topRight = textureGather(sampler2D(InputDepth, muestreador), gatherCoord + v14, 0);
  if (params.invertida > 0.5) {
    btmLeft = vec4(1.0) - btmLeft;
    btmRight = vec4(1.0) - btmRight;
    topLeft = vec4(1.0) - topLeft;
    topRight = vec4(1.0) - topRight;
  }
  float maxC = min(min(min(btmLeft.z, btmRight.w), topLeft.y), topRight.x);
  float btmLeft4 = min(min(min(btmLeft.y, btmLeft.x), btmLeft.z), btmLeft.w);
  float btmLeftMax9 = min(topLeft.x, min(min(maxC, btmLeft4), btmRight.x));

  float depthclip = 0.0;
  if (maxC < 1.0 - 1.0e-05) {
    float btmRight4 = min(min(min(btmRight.y, btmRight.x), btmRight.z), btmRight.w);
    float topLeft4 = min(min(min(topLeft.y, topLeft.x), topLeft.z), topLeft.w);
    float topRight4 = min(min(min(topRight.y, topRight.x), topRight.z), topRight.w);

    float Wdepth = 0.0;
    float Ksep = 1.37e-05;
    float Kfov = params.cameraFovAngleHor;
    float diagonal_length = length(params.renderSize);
    float Ksep_Kfov_diagonal = Ksep * Kfov * diagonal_length;

    float Depthsep = Ksep_Kfov_diagonal * (1.0 - maxC);
    float EPSILON = 1.19e-07;
    Wdepth += clamp((Depthsep / (abs(maxC - btmLeft4) + EPSILON)), 0.0, 1.0);
    Wdepth += clamp((Depthsep / (abs(maxC - btmRight4) + EPSILON)), 0.0, 1.0);
    Wdepth += clamp((Depthsep / (abs(maxC - topLeft4) + EPSILON)), 0.0, 1.0);
    Wdepth += clamp((Depthsep / (abs(maxC - topRight4) + EPSILON)), 0.0, 1.0);
    depthclip = clamp(1.0 - Wdepth * 0.25, 0.0, 1.0);
  }

  // The camera's motion, from the nearest depth (in the game's depth convention for the matrices).
  float profundidad = params.invertida > 0.5 ? 1.0 - btmLeftMax9 : btmLeftMax9;
  vec2 ScreenPos = vec2(2.0 * texCoord.x - 1.0, 1.0 - 2.0 * texCoord.y);
  vec3 Position = vec3(ScreenPos, profundidad);
  vec4 PreClip = params.clipToPrevClip[3] +
                 ((params.clipToPrevClip[2] * Position.z) +
                  ((params.clipToPrevClip[1] * ScreenPos.y) + (params.clipToPrevClip[0] * ScreenPos.x)));
  vec2 PreScreen = PreClip.xy / PreClip.w;
  vec2 motion = Position.xy - PreScreen;
  if (!(abs(motion.x) < 4.0 && abs(motion.y) < 4.0)) {  // behind the previous camera, or NaN
    motion = vec2(0.0);
    depthclip = 1.0;
  }

  // HUD (and anything else drawn after the composited scene: the raindrops on the screen).
  float hud = 0.0;
  if (params.hay_escena > 0.5) {
    vec3 final_color = texelFetch(sampler2D(InputColor, muestreador), InputPos, 0).rgb;
    vec3 escena = texelFetch(sampler2D(EscenaSinHud, muestreador), InputPos, 0).rgb;
    if (params.escena_rb > 0.5) {
      escena = escena.bgr;
    }
    vec3 diferencia = abs(final_color - escena);
    hud = max(max(diferencia.r, diferencia.g), diferencia.b) > params.mascara_umbral ? 1.0 : 0.0;
  }

  MotionDepthClipAlphaBuffer = vec4(motion, depthclip, hud);
}
