#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <string>
#include <vector>

// Системные библиотеки Windows
#include <windows.h>
#include <commdlg.h>

// Функция получения пути к папке, где лежит сам .exe
std::wstring GetExeDirectory() {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::wstring exepath(buffer);
    size_t pos = exepath.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"" : exepath.substr(0, pos + 1);
}

// Функция вызова проводника с поддержкой кириллицы
std::wstring OpenFileDialogW() {
    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = { 0 };

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"Video Files (*.mp4;*.avi;*.mkv)\0*.mp4;*.avi;*.mkv\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileNameW(&ofn) == TRUE) {
        return std::wstring(ofn.lpstrFile);
    }
    return L"";
}

// Функция конвертации WideString (русский язык Windows) в UTF-8 (для OpenCV)
std::string WStringToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

int main() {
    // Включаем современную кодировку UTF-8 для CMD (полная поддержка русского языка)
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
    std::setlocale(LC_ALL, ".UTF8");

    std::cout << "Открытие проводника... Выберите исходное видео." << std::endl;
    std::wstring video_path = OpenFileDialogW();

    if (video_path.empty()) {
        std::cout << "Файл не выбран. Выход из программы." << std::endl;
        system("pause");
        return -1;
    }

    // Настраиваем пути к папкам проекта
    std::wstring exe_dir = GetExeDirectory();
    std::wstring ascii_folder = exe_dir + L"ascii";
    std::wstring video_folder = exe_dir + L"video";

    // Создаем папки ascii и video
    CreateDirectoryW(ascii_folder.c_str(), NULL);
    CreateDirectoryW(video_folder.c_str(), NULL);

    std::wstring dest_video_path = video_folder + L"\\ascii_render.avi";

    // Конвертируем пути в UTF-8 для OpenCV
    std::string cv_video_path = WStringToUTF8(video_path);
    std::string cv_out_path = WStringToUTF8(dest_video_path);

    cv::VideoCapture cap(cv_video_path);
    if (!cap.isOpened()) {
        std::cout << "Ошибка OpenCV: Не удалось открыть видеофайл." << std::endl;
        system("pause");
        return -1;
    }

    // Получаем параметры исходного видео
    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0) fps = 30.0;

    system("cls");
    std::cout << "Обработка видео в ASCII... Пожалуйста, подождите." << std::endl;

    std::string ascii_chars = " .:-=+*#%@";
    
    // Параметры ASCII-сетки
    int target_width = 120; 
    cv::Mat frame, resized_frame;
    int frame_count = 0;

    // Настройки шрифта OpenCV
    int font_face = cv::FONT_HERSHEY_PLAIN;
    double font_scale = 0.5;
    int thickness = 1;
    int char_w = 8;  
    int char_h = 12; 

    cv::VideoWriter video_writer;
    bool is_writer_initialized = false;

    while (true) {
        cap >> frame;
        if (frame.empty()) break;

        frame_count++;

        int target_height = static_cast<int>(frame.rows * target_width / frame.cols * 0.55);
        cv::resize(frame, resized_frame, cv::Size(target_width, target_height));

        // Инициализируем запись видео в первый раз
        if (!is_writer_initialized) {
            int out_w = target_width * char_w;
            int out_h = target_height * char_h;
            
            video_writer.open(cv_out_path, cv::VideoWriter::fourcc('X', 'V', 'I', 'D'), fps, cv::Size(out_w, out_h), true);
            is_writer_initialized = true;
        }

        // Создаем черную картинку для отрисовки ASCII-видеокадра
        cv::Mat ascii_video_frame = cv::Mat::zeros(target_height * char_h, target_width * char_w, CV_8UC3);
        std::string file_buffer = "";

        for (int y = 0; y < resized_frame.rows; ++y) {
            for (int x = 0; x < resized_frame.cols; ++x) {
                cv::Vec3b pixel = resized_frame.at<cv::Vec3b>(y, x);
                uchar b = pixel[0]; // Исправлено: правильный доступ к индексам каналов Vec3b
                uchar g = pixel[1];
                uchar r = pixel[2];

                int brightness = static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b);
                int char_index = brightness * (ascii_chars.length() - 1) / 255;
                char ascii_char = ascii_chars[char_index];

                file_buffer += ascii_char;

                // Отрисовываем цветной символ на картинку будущего видео
                std::string text_str(1, ascii_char);
                cv::putText(ascii_video_frame, text_str, cv::Point(x * char_w, y * char_h + char_h - 2), 
                            font_face, font_scale, cv::Scalar(b, g, r), thickness, cv::LINE_AA);
            }
            file_buffer += "\n";
        }

        // Записываем кадр в ASCII-видеофайл
        if (video_writer.isOpened()) {
            video_writer.write(ascii_video_frame);
        }

        // Сохраняем текстовый кадр в папку ascii/frame_XXXX.txt
        std::wstring txt_path = ascii_folder + L"\\frame_" + std::to_wstring(frame_count) + L".txt";
        std::ofstream out_file(txt_path.c_str());
        if (out_file.is_open()) {
            out_file << file_buffer;
            out_file.close();
        }
    }

    cap.release();
    if (video_writer.isOpened()) {
        video_writer.release();
    }

    std::cout << "\nУспех! Всё готово!" << std::endl;
    std::cout << "Текстовые кадры лежат в папке: 'ascii'" << std::endl;
    std::cout << "Готовое ASCII-видео сохранено в папку: 'video\\ascii_render.avi'" << std::endl;
    
    system("pause");
    return 0;
}
