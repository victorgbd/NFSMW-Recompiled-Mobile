// NFSMW Recompiled - hilos fijados a nucleos en Android
//
// POR QUE ESTA AQUI Y NO EN EL SDK
//
// El SDK tiene set_affinity_mask() por hilo, pero nada la llama: su
// EnableAffinityConfiguration() en POSIX esta vacia, y no hay ningun cvar que
// reparta hilos por nombre. Un "thread_affinity" anterior en android_main.cpp
// escribia "Afinidad de hilos configurada" sin fijar ninguno.
//
// Tampoco serviria engancharse a donde el SDK nombra cada hilo: los del juego
// se llaman todos "XThreadXXXX", los crea tambien el driver de Qualcomm, SDL y
// binder, y al pasar a segundo plano Android cambia el cpuset y el kernel
// reajusta la afinidad. Asi que un hilo recorre /proc/self/task cada 2 s, lee
// el nombre de cada hilo y le pone su mascara si no la tiene. Son ~45 lecturas
// de ficheros diminutos cada 2 s: no se nota.
//
// LAS REGLAS (cvar thread_affinity)
//
//   ""        no tocar nada (por defecto)
//   "auto"    los nucleos de mas capacidad (/sys/.../cpu_capacity) son los
//             prime: "GPU Commands" (o "GPU anillo nativo" con el motor
//             nativo) y "Main XThread" van ahi, y TODO lo demas a
//             los otros, para que nadie les quite el nucleo. En el Snapdragon 8
//             Elite: cpu6-7 (4,32 GHz) frente a cpu0-5 (3,53 GHz).
//   reglas    "GPU Commands=6-7;Main XThread=6+7;*=0-5". Casan por el PRINCIPIO
//             del nombre (el kernel lo corta a 15 caracteres: "GPU Commands
//             (F"), gana la primera que case, y "*" es el resto.

#include "afinidad.h"

#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_STRING(thread_affinity, "", "System",
                      "Fijar hilos a nucleos: 'auto', o reglas 'Nombre=6-7;Otro=0+1;*=0-5'. "
                      "Vacio = no tocar");

namespace nfsmw::afinidad {

namespace {

constexpr int kMaxNucleos = 64;
constexpr auto kIntervalo = std::chrono::seconds(2);

struct Regla {
  std::string prefijo;  // vacio = comodin
  cpu_set_t nucleos;
  std::string texto;  // para el log
};

std::string Recortar(std::string s) {
  const auto a = s.find_first_not_of(" \t");
  const auto b = s.find_last_not_of(" \t\r\n");
  return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

// Un fichero pequeño de /proc o /sys, sin ifstream: con ~45 cada 2 s, lo
// barato importa algo.
bool LeerFichero(const char* ruta, char* buf, size_t tam) {
  const int fd = open(ruta, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return false;
  }
  const ssize_t n = read(fd, buf, tam - 1);
  close(fd);
  if (n <= 0) {
    return false;
  }
  buf[n] = '\0';
  // Sin el salto de linea final.
  for (ssize_t i = n - 1; i >= 0 && (buf[i] == '\n' || buf[i] == '\r'); --i) {
    buf[i] = '\0';
  }
  return true;
}

// "6-7", "0+1", "0-3+6". Devuelve false si no queda ningun nucleo.
bool LeerNucleos(const std::string& lista, cpu_set_t& salida) {
  CPU_ZERO(&salida);
  size_t pos = 0;
  while (pos <= lista.size()) {
    size_t fin = lista.find('+', pos);
    if (fin == std::string::npos) {
      fin = lista.size();
    }
    const std::string trozo = Recortar(lista.substr(pos, fin - pos));
    if (!trozo.empty()) {
      const size_t guion = trozo.find('-');
      const int desde = std::atoi(trozo.substr(0, guion).c_str());
      const int hasta = guion == std::string::npos ? desde : std::atoi(trozo.substr(guion + 1).c_str());
      for (int c = desde; c <= hasta && c < kMaxNucleos; ++c) {
        if (c >= 0) {
          CPU_SET(c, &salida);
        }
      }
    }
    pos = fin + 1;
  }
  return CPU_COUNT(&salida) > 0;
}

std::string TextoNucleos(const cpu_set_t& nucleos) {
  std::string s;
  for (int c = 0; c < kMaxNucleos; ++c) {
    if (CPU_ISSET(c, &nucleos)) {
      if (!s.empty()) {
        s += '+';
      }
      s += std::to_string(c);
    }
  }
  return s;
}

std::vector<Regla> LeerReglas(const std::string& texto) {
  std::vector<Regla> reglas;
  size_t pos = 0;
  while (pos < texto.size()) {
    size_t fin = texto.find(';', pos);
    if (fin == std::string::npos) {
      fin = texto.size();
    }
    const std::string trozo = texto.substr(pos, fin - pos);
    pos = fin + 1;
    const size_t igual = trozo.find('=');
    if (igual == std::string::npos) {
      continue;
    }
    Regla r;
    r.prefijo = Recortar(trozo.substr(0, igual));
    if (r.prefijo == "*") {
      r.prefijo.clear();
    }
    if (!LeerNucleos(trozo.substr(igual + 1), r.nucleos)) {
      REXLOG_WARN("[afinidad] regla sin nucleos, se ignora: '{}'", trozo);
      continue;
    }
    r.texto = (r.prefijo.empty() ? "*" : r.prefijo) + "=" + TextoNucleos(r.nucleos);
    reglas.push_back(std::move(r));
  }
  return reglas;
}

// Capacidad relativa de un nucleo: cpu_capacity si el kernel la da y, si no
// (kernels 3.18 / 4.4 de Snapdragon 650, 660...), su frecuencia maxima en MHz.
// 0 = no se sabe (nucleo apagado).
int CapacidadNucleo(int c) {
  char ruta[96];
  char buf[32];
  std::snprintf(ruta, sizeof(ruta), "/sys/devices/system/cpu/cpu%d/cpu_capacity", c);
  if (LeerFichero(ruta, buf, sizeof(buf))) {
    return std::atoi(buf);
  }
  std::snprintf(ruta, sizeof(ruta),
                "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", c);
  if (LeerFichero(ruta, buf, sizeof(buf))) {
    return std::atoi(buf) / 1000;
  }
  return 0;
}

bool ExisteNucleo(int c) {
  char ruta[64];
  std::snprintf(ruta, sizeof(ruta), "/sys/devices/system/cpu/cpu%d", c);
  return access(ruta, F_OK) == 0;
}

// Los prime son los de mas capacidad. Si todos valen lo mismo no hay nada que
// repartir y no se toca nada.
//
// Con pocos nucleos grandes y muchos pequenos (2+6 del Snapdragon 695, 2+4 del
// 650 y el 660), mandar TODO lo demas a los pequenos es peor que no tocar: el
// audio, la compilacion de shaders y los hilos del driver quedarian en nucleos
// que rinden menos de la mitad. Ahi solo se fijan los hilos criticos.
std::vector<Regla> ReglasAuto() {
  int capacidad[kMaxNucleos];
  int mayor = 0;
  int nucleos = 0;
  for (int c = 0; c < kMaxNucleos && ExisteNucleo(c); ++c) {
    capacidad[c] = CapacidadNucleo(c);
    mayor = std::max(mayor, capacidad[c]);
    ++nucleos;
  }
  int menor = mayor;
  for (int c = 0; c < nucleos; ++c) {
    if (capacidad[c] > 0) {
      menor = std::min(menor, capacidad[c]);
    }
  }
  if (nucleos == 0 || mayor == menor) {
    REXLOG_INFO("[afinidad] auto: todos los nucleos valen lo mismo, no se toca nada");
    return {};
  }

  cpu_set_t prime;
  cpu_set_t resto;
  CPU_ZERO(&prime);
  CPU_ZERO(&resto);
  int numPrime = 0;
  int mejorResto = 0;
  // Con if y no con ?: dentro de CPU_SET: es una macro que hace (set)->__bits,
  // y el ?: sin parentesis se asociaria mal.
  for (int c = 0; c < nucleos; ++c) {
    if (capacidad[c] == mayor) {
      CPU_SET(c, &prime);
      ++numPrime;
    } else {
      CPU_SET(c, &resto);
      mejorResto = std::max(mejorResto, capacidad[c]);
    }
  }
  const bool pocosGrandes = numPrime <= 2 && mejorResto * 10 < mayor * 6;

  std::vector<Regla> reglas;
  // "GPU Commands" es el procesador de comandos del motor de Xenos; "GPU anillo"
  // ("GPU anillo nativo", cortado a 15 caracteres), el hilo del anillo del
  // motor nativo, que hace su mismo papel.
  for (const char* nombre : {"GPU Commands", "GPU anillo", "Main XThread"}) {
    Regla r;
    r.prefijo = nombre;
    r.nucleos = prime;
    r.texto = std::string(nombre) + "=" + TextoNucleos(prime);
    reglas.push_back(r);
  }
  if (pocosGrandes) {
    REXLOG_INFO("[afinidad] auto: {} nucleos grandes y el resto pequenos; solo se fijan "
                "los hilos del anillo y principal",
                numPrime);
    return reglas;
  }
  Regla comodin;
  comodin.nucleos = resto;
  comodin.texto = "*=" + TextoNucleos(resto);
  reglas.push_back(comodin);
  return reglas;
}

}  // namespace

class Vigilante {
 public:
  explicit Vigilante(std::vector<Regla> reglas) : reglas_(std::move(reglas)) {
    hilo_ = std::thread([this]() { Bucle(); });
  }

  ~Vigilante() {
    {
      std::lock_guard<std::mutex> lock(m_);
      parar_ = true;
    }
    cv_.notify_all();
    if (hilo_.joinable()) {
      hilo_.join();
    }
  }

 private:
  void Bucle() {
    // Si no, heredaria el nombre de quien lo crea ("SDLThread") y se
    // confundiria con el al medir.
    pthread_setname_np(pthread_self(), "NFS afinidad");
    while (true) {
      Pasada();
      std::unique_lock<std::mutex> lock(m_);
      if (cv_.wait_for(lock, kIntervalo, [this]() { return parar_; })) {
        return;
      }
    }
  }

  const Regla* ReglaPara(const char* nombre) const {
    for (const Regla& r : reglas_) {
      if (r.prefijo.empty() || std::strncmp(nombre, r.prefijo.c_str(), r.prefijo.size()) == 0) {
        return &r;
      }
    }
    return nullptr;
  }

  void Pasada() {
    DIR* dir = opendir("/proc/self/task");
    if (!dir) {
      return;
    }
    while (const dirent* e = readdir(dir)) {
      if (e->d_name[0] < '0' || e->d_name[0] > '9') {
        continue;
      }
      const pid_t tid = static_cast<pid_t>(std::atoi(e->d_name));
      char ruta[64];
      char nombre[32];
      std::snprintf(ruta, sizeof(ruta), "/proc/self/task/%d/comm", tid);
      if (!LeerFichero(ruta, nombre, sizeof(nombre))) {
        continue;  // el hilo acaba de terminar
      }
      const Regla* r = ReglaPara(nombre);
      if (!r) {
        continue;
      }

      // Solo se toca si no la tiene ya: tambien repone la mascara cuando
      // Android la reajusta al volver de segundo plano.
      cpu_set_t actual;
      if (sched_getaffinity(tid, sizeof(actual), &actual) == 0 &&
          CPU_EQUAL(&actual, &r->nucleos)) {
        continue;
      }
      if (sched_setaffinity(tid, sizeof(r->nucleos), &r->nucleos) != 0) {
        // EINVAL: en segundo plano el cpuset no incluye esos nucleos. Es
        // normal y se repondra al volver; no se avisa cada 2 s.
        if (errno != EINVAL && fallidos_.insert(tid).second) {
          REXLOG_WARN("[afinidad] no se pudo fijar '{}' (tid {}): {}", nombre, tid,
                      std::strerror(errno));
        }
        continue;
      }
      if (anunciados_.insert(std::to_string(tid) + '|' + nombre).second) {
        REXLOG_INFO("[afinidad] {} (tid {}) -> {}", nombre, tid, TextoNucleos(r->nucleos));
      }
    }
    closedir(dir);
  }

  const std::vector<Regla> reglas_;
  std::thread hilo_;
  std::mutex m_;
  std::condition_variable cv_;
  bool parar_ = false;
  // Solo para no repetir las lineas del log. Los toca unicamente el vigilante.
  std::set<std::string> anunciados_;
  std::set<pid_t> fallidos_;
};

void BorrarVigilante::operator()(Vigilante* v) const {
  delete v;
}

VigilantePtr Arrancar() {
  const std::string texto = Recortar(REXCVAR_GET(thread_affinity));
  if (texto.empty()) {
    return nullptr;
  }
  std::vector<Regla> reglas = texto == "auto" ? ReglasAuto() : LeerReglas(texto);
  if (reglas.empty()) {
    return nullptr;
  }
  std::string resumen;
  for (const Regla& r : reglas) {
    if (!resumen.empty()) {
      resumen += "; ";
    }
    resumen += r.texto;
  }
  REXLOG_INFO("[afinidad] '{}' -> {}", texto, resumen);
  return VigilantePtr(new Vigilante(std::move(reglas)));
}

}  // namespace nfsmw::afinidad
