/**
 * @file cam_calibrator.cpp
 * @brief Herramienta de calibración de cámara utilizando tableros ChArUco.
 * @details Captura imágenes automáticamente con intervalos definidos una vez detectado el patrón,
 * realiza la calibración intrínseca de la cámara y guarda el modelo en JSON y Markdown.
 */

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <clocale>
#include <memory>
#include <optional>

#ifdef _WIN32
#include <windows.h>
#endif

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/aruco/aruco_calib.hpp>
#include <opencv2/core/utils/logger.hpp>

#include "argparse.hpp"
#include "json.hpp"

using json = nlohmann::json;

// Clase para silenciar temporalmente el log de OpenCV
class ScopedLogLevel {
 public:
  explicit ScopedLogLevel(cv::utils::logging::LogLevel level)
      : previous_level_(cv::utils::logging::getLogLevel()) {
    cv::utils::logging::setLogLevel(level);
  }

  ~ScopedLogLevel() {
    cv::utils::logging::setLogLevel(previous_level_);
  }

 private:
  cv::utils::logging::LogLevel previous_level_;
};

// Obtiene la fecha y hora actual en formato ISO8601
std::string GetCurrentISO8601Time() {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
#ifdef _WIN32
  struct tm buf;
  if (localtime_s(&buf, &in_time_t) == 0) {
    ss << std::put_time(&buf, "%Y-%m-%dT%H:%M:%S");
  } else {
    ss << "unknown";
  }
#else
  struct tm buf;
  if (localtime_r(&in_time_t, &buf) != nullptr) {
    ss << std::put_time(&buf, "%Y-%m-%dT%H:%M:%S");
  } else {
    ss << "unknown";
  }
#endif
  return ss.str();
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
#endif
  std::setlocale(LC_ALL, "");

  argparse::ArgumentParser program("cam_calibrator", "1.0.0");
  program.add_description("Calibrador de cámara CLI utilizando tableros ChArUco y OpenCV.");

  program.add_argument("-d", "--device")
         .help("ID del dispositivo de cámara (default: 0)")
         .scan<'i', int>()
         .default_value(0);

  program.add_argument("-n", "--num-images")
         .help("Número de imágenes a capturar para calibración (default: 15)")
         .scan<'i', int>()
         .default_value(15);

  program.add_argument("-i", "--interval")
         .help("Intervalo entre capturas automáticas en segundos (default: 2.0)")
         .scan<'g', double>()
         .default_value(2.0);

  program.add_argument("-c", "--cols")
         .help("Número de columnas del tablero ChArUco (cuadrados negros/blancos en X) (default: 8)")
         .scan<'i', int>()
         .default_value(8);

  program.add_argument("-r", "--rows")
         .help("Número de filas del tablero ChArUco (cuadrados negros/blancos en Y) (default: 11)")
         .scan<'i', int>()
         .default_value(11);

  program.add_argument("-s", "--square-size")
         .help("Tamaño del lado de un cuadrado en metros (default: 0.030)")
         .scan<'g', double>()
         .default_value(0.030);

  program.add_argument("-m", "--marker-size")
         .help("Tamaño del lado del marcador ArUco en metros (default: 0.015)")
         .scan<'g', double>()
         .default_value(0.015);

  program.add_argument("-o", "--output")
         .help("Nombre base para los archivos de salida del modelo (default: camera_model)")
         .default_value(std::string("camera_model"));

  program.add_argument("-w", "--width")
         .help("Ancho de resolución personalizado para la cámara")
         .scan<'i', int>();

  program.add_argument("-h", "--height")
         .help("Alto de resolución personalizado para la cámara")
         .scan<'i', int>();

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << "Error de argumentos: " << err.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  int device_id = program.get<int>("--device");
  int num_images = program.get<int>("--num-images");
  double interval_sec = program.get<double>("--interval");
  int cols = program.get<int>("--cols");
  int rows = program.get<int>("--rows");
  double square_size = program.get<double>("--square-size");
  double marker_size = program.get<double>("--marker-size");
  std::string output_base = program.get<std::string>("--output");

  std::optional<int> custom_width = program.present<int>("--width");
  std::optional<int> custom_height = program.present<int>("--height");

  std::cout << "Iniciando calibrador con los siguientes parámetros:" << std::endl;
  std::cout << "  - Dispositivo Cámara ID: " << device_id << std::endl;
  std::cout << "  - Cantidad de capturas: " << num_images << std::endl;
  std::cout << "  - Intervalo de disparo: " << interval_sec << " segundos" << std::endl;
  std::cout << "  - Configuración Tablero: " << cols << "x" << rows << std::endl;
  std::cout << "  - Tamaño cuadrado: " << square_size << " m / Tamaño marcador: " << marker_size << " m" << std::endl;
  std::cout << "  - Salida base: " << output_base << std::endl;

  // Inicializar dispositivo de video
  cv::VideoCapture cap;
#ifdef _WIN32
  int backend = cv::CAP_DSHOW;
#else
  int backend = cv::CAP_V4L2;
#endif

  if (!cap.open(device_id, backend)) {
    std::cerr << "Error: No se pudo abrir la cámara con ID " << device_id << std::endl;
    return 1;
  }

  if (custom_width.has_value()) {
    cap.set(cv::CAP_PROP_FRAME_WIDTH, custom_width.value());
  }
  if (custom_height.has_value()) {
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, custom_height.value());
  }

  int width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
  int height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
  std::cout << "Resolución activa de la cámara: " << width << "x" << height << std::endl;

  // Configurar tablero ChArUco
  cv::aruco::Dictionary dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_5X5_100);
  cv::Ptr<cv::aruco::CharucoBoard> board = cv::makePtr<cv::aruco::CharucoBoard>(
      cv::Size(cols, rows), static_cast<float>(square_size), static_cast<float>(marker_size), dictionary);

  cv::aruco::CharucoDetector detector(*board);

  // Vectores para almacenar detecciones de calibración
  std::vector<std::vector<cv::Point2f>> all_charuco_corners;
  std::vector<std::vector<int>> all_charuco_ids;
  cv::Size image_size(width, height);

  bool capture_started = false;
  int captured_count = 0;
  auto last_capture_time = std::chrono::steady_clock::now();

  cv::Mat frame, display_frame;
  std::cout << "\n[INFO] Apunte a la cámara con el tablero ChArUco. La captura iniciará automáticamente cuando se detecte el tablero por primera vez." << std::endl;
  std::cout << "[INFO] Presione ESC en la ventana de video para cancelar." << std::endl;

  while (true) {
    if (!cap.read(frame)) {
      std::cerr << "Error al leer cuadro de la cámara." << std::endl;
      break;
    }

    frame.copyTo(display_frame);

    std::vector<cv::Point2f> charuco_corners;
    std::vector<int> charuco_ids;
    
    // Detectar marcadores y esquinas del tablero ChArUco en el fotograma
    detector.detectBoard(frame, charuco_corners, charuco_ids);

    // Dibujar esquinas si son detectadas
    if (!charuco_ids.empty()) {
      cv::aruco::drawDetectedCornersCharuco(display_frame, charuco_corners, charuco_ids);
    }

    auto now = std::chrono::steady_clock::now();

    if (!capture_started) {
      // Comenzar capturas automáticamente la primera vez que se detectan al menos 4 esquinas del patrón
      if (charuco_ids.size() >= 4) {
        capture_started = true;
        last_capture_time = now;
        std::cout << "\n[!] ¡Tablero ChArUco detectado! Iniciando capturas automáticas..." << std::endl;
      } else {
        cv::putText(display_frame, "Esperando tablero ChArUco...", cv::Point(20, 40),
                    cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2);
      }
    } else {
      double elapsed_sec = std::chrono::duration<double>(now - last_capture_time).count();
      double time_left = interval_sec - elapsed_sec;
      if (time_left < 0.0) time_left = 0.0;

      // Mostrar barra de progreso y conteo de capturas en la ventana de video
      std::stringstream ss_info;
      ss_info << "Capturadas: " << captured_count << " / " << num_images;
      cv::putText(display_frame, ss_info.str(), cv::Point(20, 40),
                  cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2);

      std::stringstream ss_timer;
      ss_timer << std::fixed << std::setprecision(1) << "Siguiente disparo en: " << time_left << "s";
      cv::putText(display_frame, ss_timer.str(), cv::Point(20, 80),
                  cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 0), 2);

      // Disparar captura al completarse el intervalo
      if (elapsed_sec >= interval_sec) {
        if (charuco_ids.size() >= 4) {
          all_charuco_corners.push_back(charuco_corners);
          all_charuco_ids.push_back(charuco_ids);
          captured_count++;
          std::cout << ">> Capturada imagen " << captured_count << "/" << num_images 
                    << " (" << charuco_ids.size() << " esquinas detectadas)" << std::endl;
          
          // Efecto visual de flash blanco para simular una foto
          cv::Mat flash = cv::Mat::ones(display_frame.size(), display_frame.type()) * 255;
          cv::addWeighted(display_frame, 0.5, flash, 0.5, 0, display_frame);
          
          last_capture_time = now;
        } else {
          // Si no se detectan suficientes marcadores en el instante del intervalo, esperar hasta tener buena visibilidad
          cv::putText(display_frame, "Deteccion fallida - Reposicione el tablero!", cv::Point(20, 120),
                      cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2);
        }
      }
    }

    cv::imshow("Calibrador ChArUco", display_frame);
    
    // Salir del ciclo si se cumple la cantidad de capturas o se presiona ESC (código 27)
    if (captured_count >= num_images) {
      break;
    }
    if (cv::waitKey(1) == 27) {
      std::cout << "\n[!] Calibracion cancelada por el usuario." << std::endl;
      cv::destroyAllWindows();
      cap.release();
      return 0;
    }
  }

  cv::destroyAllWindows();
  cap.release();

  if (all_charuco_corners.size() < 3) {
    std::cerr << "\nError: Se recopilaron muy pocas imágenes válidas (" << all_charuco_corners.size() 
              << "). Se necesitan al menos 3 para calibrar." << std::endl;
    return 1;
  }

  std::cout << "\nCalculando calibracion de camara. Por favor espere..." << std::endl;

  cv::Mat camera_matrix = cv::Mat::eye(3, 3, CV_64F);
  cv::Mat dist_coeffs = cv::Mat::zeros(8, 1, CV_64F);
  std::vector<cv::Mat> rvecs, tvecs;

  // Realizar la calibración usando esquinas interpoladas de ChArUco
  double reproj_error = cv::aruco::calibrateCameraCharuco(
      all_charuco_corners,
      all_charuco_ids,
      board,
      image_size,
      camera_matrix,
      dist_coeffs,
      rvecs,
      tvecs
  );

  std::cout << "Calibracion finalizada con exito." << std::endl;
  std::cout << "  - Error de reproyeccion: " << reproj_error << " px" << std::endl;

  // Obtener fecha de calibración
  std::string calib_date = GetCurrentISO8601Time();

  // Guardar en formato JSON
  std::string json_filename = output_base + ".json";
  std::ofstream json_file(json_filename);
  if (json_file.is_open()) {
    json j;
    j["fecha_calibracion"] = calib_date;
    j["resolucion"] = { {"ancho", image_size.width}, {"alto", image_size.height} };
    j["error_reproyeccion"] = reproj_error;
    
    j["matriz_camara"] = json::array();
    for (int i = 0; i < 3; ++i) {
      json row = json::array();
      for (int j_idx = 0; j_idx < 3; ++j_idx) {
        row.push_back(camera_matrix.at<double>(i, j_idx));
      }
      j["matriz_camara"].push_back(row);
    }

    j["coeficientes_distorsion"] = json::array();
    for (int i = 0; i < dist_coeffs.rows * dist_coeffs.cols; ++i) {
      j["coeficientes_distorsion"].push_back(dist_coeffs.at<double>(i));
    }

    json_file << j.dump(4);
    json_file.close();
    std::cout << "  - Modelo guardado en: " << json_filename << std::endl;
  } else {
    std::cerr << "Error: No se pudo escribir en el archivo JSON: " << json_filename << std::endl;
  }

  // Guardar en formato Markdown
  std::string md_filename = output_base + ".md";
  std::ofstream md_file(md_filename);
  if (md_file.is_open()) {
    md_file << "# Modelo de Calibración de Cámara\n\n";
    md_file << "- **Fecha de Calibración:** " << calib_date << "\n";
    md_file << "- **Resolución de Imagen:** " << image_size.width << "x" << image_size.height << "\n";
    md_file << "- **Error de Reproyección:** " << std::fixed << std::setprecision(5) << reproj_error << " px\n\n";
    
    md_file << "## Matriz de la Cámara (K)\n";
    md_file << "| | | |\n";
    md_file << "|---|---|---|\n";
    for (int i = 0; i < 3; ++i) {
      md_file << "| " << std::fixed << std::setprecision(6) 
              << camera_matrix.at<double>(i, 0) << " | "
              << camera_matrix.at<double>(i, 1) << " | "
              << camera_matrix.at<double>(i, 2) << " |\n";
    }
    md_file << "\n";

    md_file << "## Coeficientes de Distorsión\n";
    int dist_count = dist_coeffs.rows * dist_coeffs.cols;
    if (dist_count >= 5) {
      md_file << "- **k1:** " << dist_coeffs.at<double>(0) << "\n";
      md_file << "- **k2:** " << dist_coeffs.at<double>(1) << "\n";
      md_file << "- **p1:** " << dist_coeffs.at<double>(2) << "\n";
      md_file << "- **p2:** " << dist_coeffs.at<double>(3) << "\n";
      md_file << "- **k3:** " << dist_coeffs.at<double>(4) << "\n";
    }
    for (int i = 5; i < dist_count; ++i) {
      md_file << "- **k" << (i - 1) << ":** " << dist_coeffs.at<double>(i) << "\n";
    }

    md_file.close();
    std::cout << "  - Modelo guardado en: " << md_filename << std::endl;
  } else {
    std::cerr << "Error: No se pudo escribir en el archivo Markdown: " << md_filename << std::endl;
  }

  std::cout << "\nCalibracion finalizada." << std::endl;
  return 0;
}
