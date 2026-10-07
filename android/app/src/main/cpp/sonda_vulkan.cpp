// NFSMW Recompiled - sonda de Vulkan
//
// Hace, sin el juego, lo primero que haria el juego con la GPU: cargar el
// libvulkan (el del sistema o Turnip, segun los ajustes android_gpu_*), crear
// la instancia y crear el dispositivo CON EMULACION DE GPU, que es donde el SDK
// comprueba que el driver da lo que la emulacion de Xenos necesita.
//
// Escribe un informe de texto en --nfsmw_informe_sonda, que la pantalla de
// inicio de la app lee y ensena. Sirve para responder, antes de tener el juego:
//
//   - se carga Turnip de verdad, o se queda el driver de Qualcomm?
//   - el SDK acepta este movil para emular la GPU?
//   - hay texturas BC (las del juego) o habra que descomprimirlas en CPU?
//   - con el motor nativo: sirve su renderizador en esta GPU, y que adapta
//     (BC en CPU, sin enteros de 64 bits, Vulkan 1.1, Mali)?

#include <android/api-level.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/vulkan/device.h>
#include <rex/ui/vulkan/instance.h>

REXCVAR_DEFINE_STRING(nfsmw_informe_sonda, "", "Android",
                      "Where the Vulkan probe writes its report");

// Definidos por tools/parche_turnip.py en vulkan_instance.cpp, dentro de
// librexruntime.so, que esta en la linea de enlace de libmain.so.
REXCVAR_DECLARE(std::string, android_gpu_driver_name);
REXCVAR_DECLARE(std::string, android_gpu_driver_dir);

namespace {

using rex::ui::vulkan::VulkanDevice;
using rex::ui::vulkan::VulkanInstance;

std::string Version(uint32_t v) {
  return fmt::format("{}.{}.{}", VK_API_VERSION_MAJOR(v), VK_API_VERSION_MINOR(v),
                     VK_API_VERSION_PATCH(v));
}

const char* NombreDriver(VkDriverId id) {
  switch (id) {
    case VK_DRIVER_ID_MESA_TURNIP:
      return "Mesa Turnip";
    case VK_DRIVER_ID_QUALCOMM_PROPRIETARY:
      return "Qualcomm (propietario)";
    case VK_DRIVER_ID_ARM_PROPRIETARY:
      return "ARM Mali (propietario)";
    case VK_DRIVER_ID_SAMSUNG_PROPRIETARY:
      return "Samsung Xclipse (propietario)";
    case VK_DRIVER_ID_IMAGINATION_PROPRIETARY:
      return "PowerVR (propietario)";
    default:
      return "otro";
  }
}

// Mali de Arm: por el vendor (0x13B5) o por el driver, no solo por el nombre. Las nuevas se llaman
// "Mali-G715", "Immortalis-G715" o "Mali-G720-Immortalis MC12".
bool EsMali(const VkPhysicalDeviceProperties& props, VkDriverId driver) {
  return props.vendorID == 0x13B5 || driver == VK_DRIVER_ID_ARM_PROPRIETARY ||
         std::strstr(props.deviceName, "Mali") != nullptr ||
         std::strstr(props.deviceName, "Immortalis") != nullptr;
}

bool EsXclipse(const VkPhysicalDeviceProperties& props, VkDriverId driver) {
  return driver == VK_DRIVER_ID_SAMSUNG_PROPRIETARY || std::strstr(props.deviceName, "Xclipse") != nullptr;
}

// La generacion de un Mali por el numero tras la "G": G31, G51, G52, G71, G72 y G76 Bifrost; G57, G68, G77, G78 y G310-G715 Valhall,
// G715 Valhall con trazado de rayos, G720 en adelante la 5.a generacion.
const char* GeneracionMali(const char* nombre) {
  const char* g = std::strchr(nombre, 'G');
  while (g && !(g[1] >= '0' && g[1] <= '9')) {
    g = std::strchr(g + 1, 'G');
  }
  if (!g) {
    return "generacion desconocida";
  }
  const int numero = std::atoi(g + 1);
  if (numero >= 720) {
    return "5.a generacion (Arm Immortalis/Mali moderno)";
  }
  if (numero == 31 || numero == 51 || numero == 52 || numero == 71 || numero == 72 || numero == 76) {
    return "Bifrost";
  }
  return "Valhall";
}

class Informe {
 public:
  void Linea(const std::string& texto) {
    REXLOG_INFO("[sonda] {}", texto);
    texto_ += texto;
    texto_ += '\n';
  }
  void Guardar(const std::string& ruta) const {
    if (ruta.empty()) {
      return;
    }
    std::ofstream f(ruta, std::ios::binary | std::ios::trunc);
    f << texto_;
  }

 private:
  std::string texto_;
};

void SondearDispositivo(Informe& inf, const VulkanInstance& instancia, VkPhysicalDevice fisico) {
  const auto& ifn = instancia.functions();

  VkPhysicalDeviceProperties props{};
  ifn.vkGetPhysicalDeviceProperties(fisico, &props);
  inf.Linea(fmt::format("GPU: {}  (vendor 0x{:04X}, device 0x{:08X})", props.deviceName,
                        props.vendorID, props.deviceID));
  inf.Linea(fmt::format("Vulkan del dispositivo: {}", Version(props.apiVersion)));

  // Que driver es de verdad. Es la respuesta a "se ha cargado Turnip?".
  VkDriverId driver_id = static_cast<VkDriverId>(0);
  if (props.apiVersion >= VK_MAKE_API_VERSION(0, 1, 2, 0) && ifn.vkGetPhysicalDeviceProperties2) {
    VkPhysicalDeviceDriverProperties driver{};
    driver.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &driver;
    ifn.vkGetPhysicalDeviceProperties2(fisico, &props2);
    driver_id = driver.driverID;
    inf.Linea(fmt::format("Driver: {}  [{}]  {} {}", NombreDriver(driver.driverID),
                          static_cast<int>(driver.driverID), driver.driverName,
                          driver.driverInfo));
  } else {
    inf.Linea("Driver: sin VkPhysicalDeviceDriverProperties (Vulkan < 1.2)");
  }

  // Texturas BC: las usa el juego. Sin ellas se descomprimen en CPU. Lo que
  // pide el motor nativo para usarlas tal cual (nfsmw_nativo_dibujos.cpp).
  const std::pair<VkFormat, const char*> formatos[] = {
      {VK_FORMAT_BC1_RGBA_UNORM_BLOCK, "BC1"},
      {VK_FORMAT_BC2_UNORM_BLOCK, "BC2"},
      {VK_FORMAT_BC3_UNORM_BLOCK, "BC3"},
      {VK_FORMAT_BC4_UNORM_BLOCK, "BC4"},
      {VK_FORMAT_BC5_UNORM_BLOCK, "BC5"},
  };
  constexpr VkFormatFeatureFlags kBcRequerido = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                                VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                                                VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
  std::string bc;
  std::string bc_en_cpu;
  // Las Xclipse de Samsung dicen tener BC4 y BC5, pero su driver las convierte el mismo por un camino a
  // medias: el motor nativo las pasa por la CPU (parche_nativo.py, seccion 10).
  const bool xclipse = EsXclipse(props, driver_id);
  const bool mali = EsMali(props, driver_id);
  for (const auto& [formato, nombre] : formatos) {
    VkFormatProperties fp{};
    ifn.vkGetPhysicalDeviceFormatProperties(fisico, formato, &fp);
    const bool ok = (fp.optimalTilingFeatures & kBcRequerido) == kBcRequerido;
    bc += fmt::format("{}={} ", nombre, ok ? "si" : "NO");
    const bool en_cpu = !ok || (xclipse && (formato == VK_FORMAT_BC4_UNORM_BLOCK ||
                                            formato == VK_FORMAT_BC5_UNORM_BLOCK));
    if (en_cpu) {
      bc_en_cpu += bc_en_cpu.empty() ? nombre : fmt::format(" {}", nombre);
    }
  }
  inf.Linea("Texturas comprimidas: " + bc);
  VkPhysicalDeviceFeatures rasgos_base{};
  ifn.vkGetPhysicalDeviceFeatures(fisico, &rasgos_base);
  if (mali) {
    inf.Linea(fmt::format("Mali: {}, driver {}", GeneracionMali(props.deviceName),
                          Version(props.driverVersion)));
  }
  // Lo que decide si el motor nativo usa 4 conjuntos de descriptores (menos de 5) y si caben sus montones
  // de texturas (update-after-bind).
  inf.Linea(fmt::format("Conjuntos de descriptores (maxBoundDescriptorSets): {}{}",
                        props.limits.maxBoundDescriptorSets,
                        props.limits.maxBoundDescriptorSets < 5 ? "  -> el motor nativo usa 4 conjuntos" : ""));
  inf.Linea(fmt::format("Enteros de 64 bits en shaders (shaderInt64): {}",
                        rasgos_base.shaderInt64 ? "si" : "NO"));

#if NFSMW_MOTOR_NATIVO
  // El dispositivo como lo crea el motor nativo en Android
  // (nfsmw_nativo_sistema.cpp): sin emulacion de la Xenos y con lo que piden
  // sus shaders. Lo que exige su renderizador al empezar
  // (nfsmw_nativo_dibujos.cpp, Inicializar), y lo que adapta si falta.
  rex::cvar::SetFlagByName("vulkan_native_shader_features", "true");
  if (auto nativo = VulkanDevice::CreateIfSupported(&instancia, fisico,
                                                    /*with_gpu_emulation=*/false,
                                                    /*with_swapchain=*/true)) {
    const auto& p = nativo->properties();
    const std::pair<bool, const char*> requisitos[] = {
        {p.independentBlend, "independentBlend"},
        {p.runtimeDescriptorArray, "runtimeDescriptorArray"},
        {p.shaderSampledImageArrayDynamicIndexing, "shaderSampledImageArrayDynamicIndexing"},
        {p.descriptorBindingPartiallyBound, "descriptorBindingPartiallyBound"},
        {p.descriptorBindingSampledImageUpdateAfterBind,
         "descriptorBindingSampledImageUpdateAfterBind"},
        {p.descriptorBindingUpdateUnusedWhilePending, "descriptorBindingUpdateUnusedWhilePending"},
    };
    std::string faltan;
    for (const auto& [ok, nombre] : requisitos) {
      if (!ok) {
        faltan += faltan.empty() ? nombre : fmt::format(", {}", nombre);
      }
    }
    if (!faltan.empty()) {
      inf.Linea("MOTOR NATIVO: NO sirve, falta " + faltan);
    } else {
      inf.Linea("MOTOR NATIVO: sirve");
      inf.Linea(bc_en_cpu.empty()
                    ? "  - texturas BC: las de la GPU"
                    : "  - texturas " + bc_en_cpu +
                          ": se descomprimen en la CPU (mas memoria y mas carga al entrar en una zona)");
      inf.Linea(p.shaderInt64 && p.bufferDeviceAddress
                    ? "  - constantes de los shaders: por UBO (tiene enteros de 64 bits, no hacen falta)"
                    : "  - sin enteros de 64 bits: no hacen falta, las constantes van por UBO");
      if (props.apiVersion < VK_MAKE_API_VERSION(0, 1, 2, 0)) {
        inf.Linea("  - Vulkan 1.1: los shaders se pasan de SPIR-V 1.5 a 1.3 al crearlos");
      }
      if (mali) {
        inf.Linea("  - Mali: sin consultas de oclusion (sin el destello del sol), por los cuelgues "
                  "de su driver");
      }
    }
  } else {
    inf.Linea("MOTOR NATIVO: NO sirve, el SDK no crea el dispositivo (motivo en el log)");
  }
#endif

  // Crear el dispositivo como lo crea el juego: con emulacion de GPU. Si el SDK
  // lo rechaza, el motivo queda en el log. Con el motor nativo es su modo de
  // compatibilidad.
  auto dispositivo = VulkanDevice::CreateIfSupported(&instancia, fisico,
                                                     /*with_gpu_emulation=*/true,
                                                     /*with_swapchain=*/true);
#if NFSMW_MOTOR_NATIVO
  if (!dispositivo) {
    inf.Linea("MODO DE COMPATIBILIDAD (emular la Xenos): NO sirve (motivo en el log)");
    return;
  }
  inf.Linea("MODO DE COMPATIBILIDAD (emular la Xenos): sirve");
#else
  if (!dispositivo) {
    inf.Linea("RESULTADO: el SDK NO acepta esta GPU para emular la Xenos (motivo en el log)");
    return;
  }
  inf.Linea("RESULTADO: el SDK acepta esta GPU para emular la Xenos");
#endif

  const auto& p = dispositivo->properties();
  const std::pair<bool, const char*> rasgos[] = {
      {p.fragmentStoresAndAtomics, "fragmentStoresAndAtomics"},
      {p.vertexPipelineStoresAndAtomics, "vertexPipelineStoresAndAtomics"},
      {p.independentBlend, "independentBlend"},
      {p.geometryShader, "geometryShader"},
      {p.tessellationShader, "tessellationShader"},
      {p.sampleRateShading, "sampleRateShading"},
      {p.depthClamp, "depthClamp"},
      {p.fillModeNonSolid, "fillModeNonSolid"},
      {p.samplerAnisotropy, "samplerAnisotropy"},
      {p.shaderClipDistance, "shaderClipDistance"},
      {p.fullDrawIndexUint32, "fullDrawIndexUint32"},
      {p.fragmentShaderPixelInterlock, "fragmentShaderPixelInterlock"},
      {p.shaderDemoteToHelperInvocation, "shaderDemoteToHelperInvocation"},
      {p.dynamicRendering, "dynamicRendering"},
  };
  for (const auto& [ok, nombre] : rasgos) {
    inf.Linea(fmt::format("  {:3} {}", ok ? "si" : "NO", nombre));
  }
}

}  // namespace

int NfsmwSondaVulkan(int argc, char** argv) {
  rex::cvar::Init(argc, argv);
  rex::cvar::ApplyEnvironment();
  rex::InitLoggingEarly();

  Informe inf;
  inf.Linea("NFSMW Recompiled - sonda de Vulkan");
  // PanVK (Mesa para Mali) puede depender de la version de Android (libdrm, gralloc): se anota para
  // comparar informes. Android 16 = API 36.
  inf.Linea(fmt::format("Android: API {}", android_get_device_api_level()));
  const std::string& driver = REXCVAR_GET(android_gpu_driver_name);
  inf.Linea(driver.empty() ? "Driver pedido: el del sistema"
                           : fmt::format("Driver pedido: {} (en {})", driver,
                                         REXCVAR_GET(android_gpu_driver_dir)));

  auto instancia = VulkanInstance::Create(/*with_surface=*/true, /*try_enable_validation=*/false);
  if (!instancia) {
    inf.Linea("RESULTADO: no se pudo crear la instancia de Vulkan (ver log)");
    inf.Guardar(REXCVAR_GET(nfsmw_informe_sonda));
    return 1;
  }
  inf.Linea(fmt::format("Instancia Vulkan: {}", Version(instancia->api_version())));

  std::vector<VkPhysicalDevice> fisicos;
  instancia->EnumeratePhysicalDevices(fisicos);
  if (fisicos.empty()) {
    inf.Linea("RESULTADO: el driver no devuelve ninguna GPU. Con Turnip suele ser que los hooks "
              "de adrenotools no estan en nativeLibraryDir");
  }
  for (VkPhysicalDevice fisico : fisicos) {
    SondearDispositivo(inf, *instancia, fisico);
  }

  inf.Guardar(REXCVAR_GET(nfsmw_informe_sonda));
  return 0;
}
