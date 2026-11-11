#version 450
// Snapdragon Game Super Resolution 1 (GSR 1) for the presenter: one spatial upscaling pass, from the guest output
// (or the previous effect) straight to the effect's output size.
//
// Adapted from sgsr1_shader_mobile_edge_direction.frag of https://github.com/SnapdragonStudios/snapdragon-gsr
// (OperationMode 1 = RGBA with green as luma, UseEdgeDirection on). Changes: the presenter's interface (push
// constants from byte 16, texture in binding 0 and the linear clamp sampler in binding 1, the texture coordinate
// computed from gl_FragCoord like the other effects) and the whole image upscaled (no SR region).
//
// Compiled to sdk/src/ui/shaders/vulkan_spirv/guest_output_sgsr1_ps.h by tools/compilar_shader_presentador.py;
// tools/gles_efectos_presentador.cpp converts that SPIR-V for GlesPresenter.
//
// ---------------------------------------------------------------------------------------------------------------
// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
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

precision mediump float;
precision highp int;

// Presenter::GsrConstants. The vertex stage uses bytes 0-15.
layout(push_constant) uniform Constantes {
  layout(offset = 16) ivec2 output_offset;
  layout(offset = 24) highp vec2 output_size_inv;
  // xy = 1 / input size, zw = input size (the original's ViewportInfo[0]).
  layout(offset = 32) highp vec4 viewport_info;
} constantes;

layout(set = 0, binding = 0) uniform mediump texture2D origen_textura;
layout(set = 0, binding = 1) uniform sampler origen_muestreador;

layout(location = 0) out vec4 out_Target0;

#define ps0 sampler2D(origen_textura, origen_muestreador)

#define EdgeThreshold (8.0 / 255.0)
#define EdgeSharpness 2.0

float fastLanczos2(float x) {
  float wA = x - 4.0;
  float wB = x * wA - wA;
  wA *= wA;
  return wB * wA;
}

vec2 weightY(float dx, float dy, float c, vec3 data) {
  float std = data.x;
  vec2 dir = data.yz;
  float edgeDis = ((dx * dir.y) + (dy * dir.x));
  float x = (((dx * dx) + (dy * dy)) +
             ((edgeDis * edgeDis) * ((clamp(((c * c) * std), 0.0, 1.0) * 0.7) + -1.0)));
  float w = fastLanczos2(x);
  return vec2(w, w * c);
}

vec2 edgeDirection(vec4 left, vec4 right) {
  vec2 dir;
  float RxLz = (right.x + (-left.z));
  float RwLy = (right.w + (-left.y));
  vec2 delta;
  delta.x = (RxLz + RwLy);
  delta.y = (RxLz + (-RwLy));
  float lengthInv = inversesqrt((delta.x * delta.x + 3.075740e-05) + (delta.y * delta.y));
  dir.x = (delta.x * lengthInv);
  dir.y = (delta.y * lengthInv);
  return dir;
}

void main() {
  highp vec4 ViewportInfo = constantes.viewport_info;
  // Same texture coordinate as the bilinear effect: the centre of the output pixel, relative to the effect's
  // rectangle (the final pass may be offset by the letterbox).
  highp vec2 in_TEXCOORD0 =
      (vec2(ivec2(gl_FragCoord.xy) - constantes.output_offset) + 0.5) * constantes.output_size_inv;

  vec4 color;
  color.xyz = textureLod(ps0, in_TEXCOORD0, 0.0).xyz;

  highp vec2 imgCoord = ((in_TEXCOORD0 * ViewportInfo.zw) + vec2(-0.5, 0.5));
  highp vec2 imgCoordPixel = floor(imgCoord);
  highp vec2 coord = (imgCoordPixel * ViewportInfo.xy);
  vec2 pl = (imgCoord + (-imgCoordPixel));
  vec4 left = textureGather(ps0, coord, 1);

  float edgeVote = abs(left.z - left.y) + abs(color.y - left.y) + abs(color.y - left.z);
  if (edgeVote > EdgeThreshold) {
    coord.x += ViewportInfo.x;

    vec4 right = textureGather(ps0, coord + vec2(ViewportInfo.x, 0.0), 1);
    vec4 upDown;
    upDown.xy = textureGather(ps0, coord + vec2(0.0, -ViewportInfo.y), 1).wz;
    upDown.zw = textureGather(ps0, coord + vec2(0.0, ViewportInfo.y), 1).yx;

    float mean = (left.y + left.z + right.x + right.w) * 0.25;
    left = left - vec4(mean);
    right = right - vec4(mean);
    upDown = upDown - vec4(mean);
    color.w = color.y - mean;

    float sum = (((((abs(left.x) + abs(left.y)) + abs(left.z)) + abs(left.w)) +
                  (((abs(right.x) + abs(right.y)) + abs(right.z)) + abs(right.w))) +
                 (((abs(upDown.x) + abs(upDown.y)) + abs(upDown.z)) + abs(upDown.w)));
    float sumMean = 1.014185e+01 / sum;
    float std = (sumMean * sumMean);

    vec3 data = vec3(std, edgeDirection(left, right));
    vec2 aWY = weightY(pl.x, pl.y + 1.0, upDown.x, data);
    aWY += weightY(pl.x - 1.0, pl.y + 1.0, upDown.y, data);
    aWY += weightY(pl.x - 1.0, pl.y - 2.0, upDown.z, data);
    aWY += weightY(pl.x, pl.y - 2.0, upDown.w, data);
    aWY += weightY(pl.x + 1.0, pl.y - 1.0, left.x, data);
    aWY += weightY(pl.x, pl.y - 1.0, left.y, data);
    aWY += weightY(pl.x, pl.y, left.z, data);
    aWY += weightY(pl.x + 1.0, pl.y, left.w, data);
    aWY += weightY(pl.x - 1.0, pl.y - 1.0, right.x, data);
    aWY += weightY(pl.x - 2.0, pl.y - 1.0, right.y, data);
    aWY += weightY(pl.x - 2.0, pl.y, right.z, data);
    aWY += weightY(pl.x - 1.0, pl.y, right.w, data);

    float finalY = aWY.y / aWY.x;
    float maxY = max(max(left.y, left.z), max(right.x, right.w));
    float minY = min(min(left.y, left.z), min(right.x, right.w));
    float deltaY = clamp(EdgeSharpness * finalY, minY, maxY) - color.w;

    // Smooth high contrast input.
    deltaY = clamp(deltaY, -23.0 / 255.0, 23.0 / 255.0);

    color.x = clamp((color.x + deltaY), 0.0, 1.0);
    color.y = clamp((color.y + deltaY), 0.0, 1.0);
    color.z = clamp((color.z + deltaY), 0.0, 1.0);
  }

  color.w = 1.0;  // The alpha channel is not used.
  out_Target0 = color;
}
