// Converts the presenter's effect pixel shaders (sdk/src/ui/shaders/vulkan_spirv/guest_output_*_ps.h, the SPIR-V
// VulkanPresenter uses) to GLSL ES 3.10 for GlesPresenter, and writes them to
// sdk/src/ui/gles/gles_presenter_efectos.h. Run it again only if those shaders change.
//
// Each shader comes out in two variants:
//   - intermediate: drawn into a texture. GL and Vulkan number the rows of a texture the same way for the same
//     gl_FragCoord, so it is the shader as is.
//   - final: drawn into the window, whose row 0 is the bottom of the screen. gl_FragCoord.y is flipped with the
//     uniform nfsmw_alto (the window height) before the shader reads it.
// The texture is "origen" (unit 0) and the push constants become the uniform struct "constantes" with the members
// m0, m1, m2 in the order of the presenter's constant structs (BilinearConstants, FsrEasuConstants...).
//
// Build (from nativo/, with the VS Build Tools clang):
//   clang++ -std=c++20 -O1 -I sdk/thirdparty/spirv-cross tools/gles_efectos_presentador.cpp
//     sdk/thirdparty/spirv-cross/spirv_{cross,parser,cross_parsed_ir,cfg,glsl}.cpp -o gles_efectos.exe
// Run from nativo/: gles_efectos.exe [folder to also write each .frag into, to check them with glslang]

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "spirv_glsl.hpp"

#include "../sdk/src/ui/shaders/vulkan_spirv/guest_output_bilinear_dither_ps.h"
#include "../sdk/src/ui/shaders/vulkan_spirv/guest_output_bilinear_ps.h"
#include "../sdk/src/ui/shaders/vulkan_spirv/guest_output_ffx_cas_resample_dither_ps.h"
#include "../sdk/src/ui/shaders/vulkan_spirv/guest_output_ffx_cas_resample_ps.h"
#include "../sdk/src/ui/shaders/vulkan_spirv/guest_output_ffx_cas_sharpen_dither_ps.h"
#include "../sdk/src/ui/shaders/vulkan_spirv/guest_output_ffx_cas_sharpen_ps.h"
#include "../sdk/src/ui/shaders/vulkan_spirv/guest_output_ffx_fsr_easu_ps.h"
#include "../sdk/src/ui/shaders/vulkan_spirv/guest_output_ffx_fsr_rcas_dither_ps.h"
#include "../sdk/src/ui/shaders/vulkan_spirv/guest_output_ffx_fsr_rcas_ps.h"
#include "../sdk/src/ui/shaders/vulkan_spirv/guest_output_sgsr1_ps.h"

namespace {

struct Shader {
  const char* nombre;  // in GuestOutputPaintEffect order
  const uint32_t* palabras;
  size_t bytes;
};

#define SHADER(n) {#n, n, sizeof(n)}
const Shader kShaders[] = {
    SHADER(guest_output_bilinear_ps),
    SHADER(guest_output_bilinear_dither_ps),
    SHADER(guest_output_ffx_cas_sharpen_ps),
    SHADER(guest_output_ffx_cas_sharpen_dither_ps),
    SHADER(guest_output_ffx_cas_resample_ps),
    SHADER(guest_output_ffx_cas_resample_dither_ps),
    SHADER(guest_output_ffx_fsr_easu_ps),
    SHADER(guest_output_ffx_fsr_rcas_ps),
    SHADER(guest_output_ffx_fsr_rcas_dither_ps),
    SHADER(guest_output_sgsr1_ps),
};
#undef SHADER

std::string Convertir(const Shader& s, size_t& miembros) {
  spirv_cross::CompilerGLSL c(s.palabras, s.bytes / sizeof(uint32_t));
  spirv_cross::CompilerGLSL::Options o = c.get_common_options();
  o.es = true;
  o.version = 310;
  o.vulkan_semantics = false;
  o.fragment.default_float_precision = spirv_cross::CompilerGLSL::Options::Highp;
  o.fragment.default_int_precision = spirv_cross::CompilerGLSL::Options::Highp;
  c.set_common_options(o);
  // Images read with texelFetch have no sampler in SPIR-V: GLSL needs one anyway.
  c.build_dummy_sampler_for_combined_images();
  c.build_combined_image_samplers();
  for (const spirv_cross::CombinedImageSampler& r : c.get_combined_image_samplers()) {
    c.set_name(r.combined_id, "origen");
    c.set_decoration(r.combined_id, spv::DecorationBinding, 0);
  }
  const spirv_cross::ShaderResources recursos = c.get_shader_resources();
  miembros = 0;
  for (const spirv_cross::Resource& pc : recursos.push_constant_buffers) {
    c.set_name(pc.id, "constantes");
    c.set_name(pc.base_type_id, "Constantes");
    const spirv_cross::SPIRType& tipo = c.get_type(pc.base_type_id);
    miembros = tipo.member_types.size();
    for (uint32_t i = 0; i < tipo.member_types.size(); ++i) {
      c.set_member_name(pc.base_type_id, i, "m" + std::to_string(i));
    }
  }
  return c.compile();
}

// The final variant: gl_FragCoord with its y measured from the top of the window.
std::string Final(std::string glsl) {
  const std::string declaracion = "uniform float nfsmw_alto;\nvec4 nfsmw_FragCoord;\n";
  for (size_t i = glsl.find("gl_FragCoord"); i != std::string::npos; i = glsl.find("gl_FragCoord", i)) {
    glsl.replace(i, 12, "nfsmw_FragCoord");
    i += 15;
  }
  const size_t main = glsl.find("void main()\n{\n");
  if (main == std::string::npos) {
    return {};
  }
  glsl.insert(main + 14, "    nfsmw_FragCoord = vec4(gl_FragCoord.x, nfsmw_alto - gl_FragCoord.y, gl_FragCoord.zw);\n");
  glsl.insert(main, declaracion);
  return glsl;
}

void Literal(std::ofstream& h, const std::string& texto) {
  h << "R\"NFSMW(" << texto << ")NFSMW\"";
}

}  // namespace

int main(int argc, char** argv) {
  const std::string comprobar = argc > 1 ? argv[1] : "";
  std::ofstream h("sdk/src/ui/gles/gles_presenter_efectos.h", std::ios::binary);
  if (!h) {
    std::fprintf(stderr, "run it from nativo/\n");
    return 1;
  }
  h << "#pragma once\n"
       "// Generated by nativo/tools/gles_efectos_presentador.cpp from sdk/src/ui/shaders/vulkan_spirv: the presenter's\n"
       "// effect pixel shaders (bilinear, AMD CAS and FSR 1.0, Snapdragon GSR 1) in GLSL ES 3.10. Do not edit by hand.\n"
       "\n"
       "#include <cstdint>\n"
       "\n"
       "namespace rex::ui::gles::efectos {\n"
       "\n"
       "struct Efecto {\n"
       "  const char* nombre;\n"
       "  // Drawn into a texture (as is) and into the window (gl_FragCoord.y flipped with nfsmw_alto).\n"
       "  const char* intermedio;\n"
       "  const char* final;\n"
       "  // Members of the uniform struct \"constantes\" (m0, m1...).\n"
       "  uint32_t miembros;\n"
       "};\n"
       "\n"
       "// In Presenter::GuestOutputPaintEffect order.\n"
       "inline constexpr Efecto kEfectos[] = {\n";
  for (const Shader& s : kShaders) {
    size_t miembros = 0;
    std::string intermedio;
    try {
      intermedio = Convertir(s, miembros);
    } catch (const std::exception& e) {
      std::fprintf(stderr, "%s: %s\n", s.nombre, e.what());
      return 1;
    }
    const std::string final = Final(intermedio);
    if (final.empty()) {
      std::fprintf(stderr, "%s: no main()\n", s.nombre);
      return 1;
    }
    h << "    {\"" << s.nombre << "\",\n     ";
    Literal(h, intermedio);
    h << ",\n     ";
    Literal(h, final);
    h << ",\n     " << miembros << "},\n";
    if (!comprobar.empty()) {
      std::ofstream(comprobar + "/" + s.nombre + "_intermedio.frag", std::ios::binary) << intermedio;
      std::ofstream(comprobar + "/" + s.nombre + "_final.frag", std::ios::binary) << final;
    }
    std::printf("%s: %zu + %zu caracteres, %zu constantes\n", s.nombre, intermedio.size(), final.size(), miembros);
  }
  h << "};\n\n}  // namespace rex::ui::gles::efectos\n";
  return 0;
}
