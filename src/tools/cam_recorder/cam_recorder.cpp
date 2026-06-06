/**
 * @file cam_recorder.cpp
 * @brief Aplicación de línea de comandos para grabar video desde la cámara web.
 * @details Utiliza OpenCV para la captura y escritura de video, argparse.hpp
 * para la gestión de argumentos, e indicators.hpp para mostrar una barra de
 * progreso la grabación.
 */

#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <optional>
#include <clocale>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cerrno>
#endif

#include <opencv2/opencv.hpp>
#include "argparse.hpp"
#include "indicators.hpp"

namespace cam_recorder {

#ifndef _WIN32
// Puntero global para acceder a la barra desde los manejadores de señales
indicators::ProgressBar* global_bar = nullptr;

// Indicador atómico para saber si se interrumpió el programa
volatile std::sig_atomic_t g_interrupted = 0;

// Manejador del cambio de tamaño de la terminal
void handle_resize(int signal) {
  if (global_bar) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    
    // Ajustar el ancho de la barra dinámicamente para evitar saltos de línea
    // Dejamos aprox. 65 caracteres para los textos del postfix, porcentaje, corchetes, etc.
    if (w.ws_col > 75) {
      global_bar->set_option(indicators::option::BarWidth{static_cast<size_t>(w.ws_col - 65)});
    } else {
      global_bar->set_option(indicators::option::BarWidth{10}); // Ancho mínimo de seguridad
    }
  }
}

// Manejador para Ctrl+C o señales de terminación
void handle_exit(int signal) {
  g_interrupted = signal;
}
#endif

/**
 * @brief Interfaz base para los comandos del grabador de cámara.
 * @details Esta interfaz permite cumplir con los principios de Responsabilidad Única (SRP)
 * y Abierto/Cerrado (OCP), facilitando la adición de nuevos comandos en el futuro.
 */
class Command {
 public:
  virtual ~Command() = default;

  /**
   * @brief Ejecuta la acción del comando.
   * @return Código de salida (0 para éxito, diferente de 0 para error).
   */
  virtual int Execute() = 0;
};

/**
 * @brief Comando para listar los dispositivos de cámara disponibles.
 */
class ListCommand : public Command {
 public:
  ListCommand() = default;
  ~ListCommand() override = default;

  /**
   * @brief Ejecuta el listado de cámaras probando índices del 0 al 9.
   */
  int Execute() override {
    std::cout << "Buscando dispositivos de cámara disponibles..." << std::endl;
    int dispositivos_encontrados = 0;

    // Elegir backend de video según plataforma
#ifdef _WIN32
    int backend = cv::CAP_DSHOW;
#else
    int backend = cv::CAP_V4L2;
#endif

    for (int i = 0; i < 10; ++i) {
      cv::VideoCapture cap;
      // Intentamos abrir la cámara con la API recomendada para la plataforma
      if (cap.open(i, backend)) {
        int ancho = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        int alto = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        double fps = cap.get(cv::CAP_PROP_FPS);

        std::cout << "  - Cámara ID: " << i 
                  << " (Resolución: " << ancho << "x" << alto;
        if (fps > 0.0) {
          std::cout << " @ " << fps << " FPS";
        }
        std::cout << ")" << std::endl;

        cap.release();
        dispositivos_encontrados++;
      }
    }

    if (dispositivos_encontrados == 0) {
      std::cout << "No se detectaron dispositivos de cámara web." << std::endl;
    } else {
      std::cout << "Se encontraron " << dispositivos_encontrados 
                << " dispositivo(s) activo(s)." << std::endl;
    }

    return 0;
  }
};

/**
 * @brief Comando para grabar video de la cámara web.
 */
class RecordCommand : public Command {
 public:
  RecordCommand(int device_id, int duration_seconds, const std::string& output_file,
                std::optional<double> custom_fps, std::optional<int> custom_width,
                std::optional<int> custom_height)
      : device_id_(device_id),
        duration_seconds_(duration_seconds),
        output_file_(output_file),
        custom_fps_(custom_fps),
        custom_width_(custom_width),
        custom_height_(custom_height) {}

  ~RecordCommand() override = default;

  /**
   * @brief Ejecuta la grabación y muestra la barra de progreso azul.
   */
  int Execute() override {
    std::cout << "Abriendo dispositivo de cámara (ID: " << device_id_ << ")..." << std::endl;
    cv::VideoCapture cap;

    // Elegir backend de video según plataforma
#ifdef _WIN32
    int backend = cv::CAP_DSHOW;
#else
    int backend = cv::CAP_V4L2;
#endif

    if (!cap.open(device_id_, backend)) {
      std::cerr << "Error: No se pudo acceder a la cámara con ID " << device_id_ << "." << std::endl;
      return 1;
    }

    // Configurar resolución personalizada si se solicita
    if (custom_width_.has_value()) {
      cap.set(cv::CAP_PROP_FRAME_WIDTH, custom_width_.value());
    }
    if (custom_height_.has_value()) {
      cap.set(cv::CAP_PROP_FRAME_HEIGHT, custom_height_.value());
    }
    if (custom_fps_.has_value()) {
      cap.set(cv::CAP_PROP_FPS, custom_fps_.value());
    }

    // Obtener propiedades definitivas del flujo de video
    int width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    double fps = cap.get(cv::CAP_PROP_FPS);

    // Ajustar si el FPS devuelto no es válido
    if (fps <= 0.0) {
      fps = custom_fps_.value_or(30.0);
    }

    std::cout << "Configuración de grabación inicializada:" << std::endl;
    std::cout << "  - Resolución real: " << width << "x" << height << std::endl;
    std::cout << "  - FPS real: " << fps << std::endl;
    std::cout << "  - Archivo de salida: " << output_file_ << std::endl;
    std::cout << "  - Duración: " << duration_seconds_ << " segundos" << std::endl;

    // Configurar el escritor de video utilizando el códec MP4V
    cv::VideoWriter writer;
    int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');

    if (!writer.open(output_file_, fourcc, fps, cv::Size(width, height), true)) {
      std::cerr << "Error: No se pudo abrir o crear el archivo de salida '" << output_file_ << "'." << std::endl;
      return 1;
    }

    std::cout << "\nGrabación iniciada..." << std::endl;

    // Crear y configurar la barra de progreso de indicators
    using namespace indicators;
    ProgressBar bar{
        option::BarWidth{45},
        option::Start{"["},
        option::Fill{"█"},
        option::Lead{""},
        option::Remainder{"░"},
        option::End{"]"},
        option::ForegroundColor{Color::blue},
        option::ShowPercentage{true},
        option::PostfixText{"| Iniciando..."}
    };

    // Ocultar el cursor en la terminal para mejorar la visualización de la barra
    show_console_cursor(false);

#ifndef _WIN32
    global_bar = &bar;
    g_interrupted = 0;

    // Configurar manejadores con sigaction para tener control sobre SA_RESTART
    struct sigaction sa_resize;
    sa_resize.sa_handler = handle_resize;
    sigemptyset(&sa_resize.sa_mask);
    sa_resize.sa_flags = SA_RESTART;
    struct sigaction prev_sigwinch;
    sigaction(SIGWINCH, &sa_resize, &prev_sigwinch);

    struct sigaction sa_exit;
    sa_exit.sa_handler = handle_exit;
    sigemptyset(&sa_exit.sa_mask);
    sa_exit.sa_flags = 0; // No SA_RESTART para que interrumpa I/O de inmediato
    struct sigaction prev_sigint;
    struct sigaction prev_sigterm;
    sigaction(SIGINT, &sa_exit, &prev_sigint);
    sigaction(SIGTERM, &sa_exit, &prev_sigterm);

    // Chequeo inicial del tamaño de la terminal
    handle_resize(0);
#endif

    auto start_time = std::chrono::steady_clock::now();
    auto end_time = start_time + std::chrono::seconds(duration_seconds_);
    long long frames_grabados = 0;

    cv::Mat frame;

    // Bucle de grabación basado en tiempo real
    while (true) {
#ifndef _WIN32
      if (g_interrupted) {
        break;
      }
#endif

      auto current_time = std::chrono::steady_clock::now();
      if (current_time >= end_time) {
        break;
      }

      // Capturar cuadro de video
      if (!cap.read(frame)) {
#ifndef _WIN32
        if (g_interrupted) {
          break;
        }
        if (errno == EINTR) {
          errno = 0;
          continue;
        }
#endif
        std::cerr << "\nError: Se interrumpió la captura de video." << std::endl;
        break;
      }

      // Escribir en el archivo
      writer.write(frame);
      frames_grabados++;

      // Calcular tiempo transcurrido, restante y porcentaje
      auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
      double total_ms = duration_seconds_ * 1000.0;
      double progress_pct = (elapsed_ms / total_ms) * 100.0;
      if (progress_pct > 100.0) progress_pct = 100.0;

      double elapsed_sec = elapsed_ms / 1000.0;
      double remaining_sec = duration_seconds_ - elapsed_sec;
      if (remaining_sec < 0.0) remaining_sec = 0.0;

      // Actualizar el texto que acompaña a la barra
      std::stringstream ss;
      ss << std::fixed << std::setprecision(1)
         << " | Transcurrido: " << elapsed_sec << "s / Restante: " << remaining_sec << "s"
         << " (Total: " << duration_seconds_ << "s)";

      bar.set_option(option::PostfixText{ss.str()});
      bar.set_progress(static_cast<size_t>(progress_pct));
    }

#ifndef _WIN32
    if (g_interrupted) {
      if (!bar.is_completed()) {
        bar.mark_as_completed();
      }
      show_console_cursor(true);

      // Restaurar manejadores anteriores
      sigaction(SIGWINCH, &prev_sigwinch, nullptr);
      sigaction(SIGINT, &prev_sigint, nullptr);
      sigaction(SIGTERM, &prev_sigterm, nullptr);
      global_bar = nullptr;

      cap.release();
      writer.release();

      std::cout << "\n[!] Program interrupted. Terminal restored.\n";
      return g_interrupted;
    }
#endif

    // Asegurar barra al 100%
    std::stringstream ss;
    ss << " | Transcurrido: " << static_cast<double>(duration_seconds_) << ".0s / Restante: 0.0s"
       << " (Total: " << duration_seconds_ << "s)";
    bar.set_option(option::PostfixText{ss.str()});
    bar.set_progress(100);

#ifndef _WIN32
    // Restaurar manejadores anteriores
    sigaction(SIGWINCH, &prev_sigwinch, nullptr);
    sigaction(SIGINT, &prev_sigint, nullptr);
    sigaction(SIGTERM, &prev_sigterm, nullptr);
    global_bar = nullptr;
#endif

    // Restaurar cursor en terminal
    show_console_cursor(true);

    // Liberar recursos de OpenCV
    cap.release();
    writer.release();

    std::cout << "\nProceso finalizado con éxito." << std::endl;
    std::cout << "  - Fotogramas escritos: " << frames_grabados << std::endl;
    std::cout << "  - Archivo guardado: " << output_file_ << std::endl;

    return 0;
  }

 private:
  int device_id_;
  int duration_seconds_;
  std::string output_file_;
  std::optional<double> custom_fps_;
  std::optional<int> custom_width_;
  std::optional<int> custom_height_;
};

}  // namespace cam_recorder

int main(int argc, char* argv[]) {
#ifdef _WIN32
  // Configurar la consola de Windows para usar UTF-8 y mostrar acentos correctamente
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
#endif

  // Asegurar correcto formateo de caracteres UTF-8 en Windows/Linux
  std::setlocale(LC_ALL, "");

  argparse::ArgumentParser program("cam_recorder", "1.0.0");

  program.add_description("Aplicación CLI premium para grabar video desde la cámara web usando OpenCV.");

  // Configuración de los argumentos
  program.add_argument("-d", "--device")
         .help("ID del dispositivo de la cámara a utilizar (default: 0)")
         .scan<'i', int>()
         .default_value(0);

  program.add_argument("-t", "--duration")
         .help("Tiempo de grabación en segundos (default: 10)")
         .scan<'i', int>()
         .default_value(10);

  program.add_argument("-o", "--output")
         .help("Nombre del archivo de video de salida (default: out.mp4)")
         .default_value(std::string("out.mp4"));

  program.add_argument("-f", "--fps")
         .help("Configurar tasa de fotogramas por segundo (FPS) personalizada")
         .scan<'g', double>();

  program.add_argument("-w", "--width")
         .help("Configurar ancho de resolución personalizado de la cámara")
         .scan<'i', int>();

  program.add_argument("-h", "--height")
         .help("Configurar alto de resolución personalizado de la cámara")
         .scan<'i', int>();

  program.add_argument("-l", "--list")
         .help("Listar todos los dispositivos de cámara disponibles en el sistema")
         .flag();

  // Procesar argumentos
  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << "Error al procesar argumentos: " << err.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  std::unique_ptr<cam_recorder::Command> cmd;

  // Evaluar qué comando ejecutar según las banderas indicadas
  if (program.get<bool>("--list")) {
    cmd = std::make_unique<cam_recorder::ListCommand>();
  } else {
    // Obtener valores con o sin default
    int device_id = program.get<int>("--device");
    int duration = program.get<int>("--duration");
    std::string output = program.get<std::string>("--output");

    std::optional<double> custom_fps;
    if (auto fps_val = program.present<double>("--fps")) {
      custom_fps = *fps_val;
    }

    std::optional<int> custom_width;
    if (auto w_val = program.present<int>("--width")) {
      custom_width = *w_val;
    }

    std::optional<int> custom_height;
    if (auto h_val = program.present<int>("--height")) {
      custom_height = *h_val;
    }

    cmd = std::make_unique<cam_recorder::RecordCommand>(
        device_id, duration, output, custom_fps, custom_width, custom_height);
  }

  // Ejecución polimórfica (Patrón Command)
  return cmd->Execute();
}
