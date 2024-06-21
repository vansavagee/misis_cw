#include <windows.h>
#include <commdlg.h>
#include <gdiplus.h>
#include <tchar.h>
#include <opencv2/opencv.hpp>

#pragma comment(lib, "gdiplus.lib")

// Глобальные переменные
HINSTANCE hInst;
cv::Mat image;

// Прототипы функций
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void LoadImageFromFile(HWND hwnd);
void FindLetters();
void OnPaint(HWND hwnd);
void OnResize(HWND hwnd);
void ConvertMatToHBITMAP(const cv::Mat& mat, HBITMAP& hBitmap);

// Инициализация GDI+
ULONG_PTR gdiplusToken;

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
   LPTSTR lpCmdLine, int nCmdShow)
{
   Gdiplus::GdiplusStartupInput gdiplusStartupInput;
   Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

   MSG msg;
   WNDCLASS wc = { 0 };
   HWND hwnd;
   hInst = hInstance;

   // Регистрация класса окна
   wc.lpfnWndProc = WndProc;
   wc.hInstance = hInstance;
   wc.lpszClassName = _T("MyWindowClass");
   wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
   wc.hCursor = LoadCursor(NULL, IDC_ARROW);

   if (!RegisterClass(&wc))
      return 1;

   // Создание главного окна
   hwnd = CreateWindow(wc.lpszClassName, _T("Распознование текста"),
      WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
      800, 600, NULL, NULL, hInstance, NULL);

   if (!hwnd)
      return 1;

   ShowWindow(hwnd, nCmdShow);
   UpdateWindow(hwnd);

   // Цикл обработки сообщений
   while (GetMessage(&msg, NULL, 0, 0))
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }

   Gdiplus::GdiplusShutdown(gdiplusToken);
   return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
   switch (msg)
   {
   case WM_CREATE:
   {
      // Создание меню
      HMENU hMenu = CreateMenu();
      HMENU hFileMenu = CreateMenu();
      AppendMenu(hFileMenu, MF_STRING, 1, _T("Открыть изображение"));
      AppendMenu(hFileMenu, MF_STRING, 2, _T("Найти буквы"));
      AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, _T("Файл"));
      SetMenu(hwnd, hMenu);
      break;
   }
   case WM_COMMAND:
      switch (LOWORD(wParam))
      {
      case 1:
         // Открытие изображения
         LoadImageFromFile(hwnd);
         break;
      case 2:
         // Найти буквы
         FindLetters();
         break;
      }
      break;
   case WM_PAINT:
      OnPaint(hwnd);
      break;
   case WM_SIZE:
      OnResize(hwnd);
      break;
   case WM_DESTROY:
      PostQuitMessage(0);
      if (!image.empty()) {
         image.release();
      }
      break;
   default:
      return DefWindowProc(hwnd, msg, wParam, lParam);
   }
   return 0;
}

std::string ConvertTCHARToString(const TCHAR* szFile) {
#ifdef _UNICODE
   std::wstring wstr(szFile);
   std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
   return conv.to_bytes(wstr);
#else
   return std::string(szFile);
#endif
}

void LoadImageFromFile(HWND hwnd)
{
   OPENFILENAME ofn;
   TCHAR szFile[MAX_PATH] = { 0 };

   ZeroMemory(&ofn, sizeof(ofn));
   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hwnd;
   ofn.lpstrFile = szFile;
   ofn.nMaxFile = sizeof(szFile);
   ofn.lpstrFilter = _T("Все файлы\0*.*\0Файлы изображений\0*.BMP;*.JPG;*.GIF;*.PNG\0");
   ofn.nFilterIndex = 1;
   ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

   if (GetOpenFileName(&ofn))
   {

      // Преобразуем TCHAR в std::string
      //std::wstring wstr(szFile);
      std::string filePath = ConvertTCHARToString(szFile);

      // Удаление предыдущего изображения, если оно есть
      if (!image.empty()) {
         image.release();
      }

      // Загружаем изображение с помощью OpenCV
      image = cv::imread(filePath);

      // Перерисовываем окно
      InvalidateRect(hwnd, NULL, TRUE);
   }
}

void FindLetters()
{
   if (image.empty()) {
      MessageBox(NULL, _T("Изображение не загружено."), _T("Ошибка"), MB_OK);
      return;
   }

   // Преобразование изображения в оттенки серого
   cv::Mat gray;
   cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

   // Применение размытия по Гауссу для уменьшения шума
   cv::Mat blurred;
   cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);

   // Применение адаптивного порогового значения для выделения букв
   cv::Mat thresh;
   cv::adaptiveThreshold(blurred, thresh, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY_INV, 11, 2);

   // Нахождение контуров на бинарном изображении
   std::vector<std::vector<cv::Point>> contours;
   cv::findContours(thresh, contours, cv::RETR_TREE, cv::CHAIN_APPROX_NONE);

   // Перебор всех найденных контуров
   for (const auto& contour : contours) {
      // Вычисление ограничивающего прямоугольника для каждого контура
      cv::Rect boundingBox = cv::boundingRect(contour);

      // Вычисление соотношения сторон прямоугольника
      float aspectRatio = (float)boundingBox.width / boundingBox.height;

      // Проверка, соответствует ли соотношение сторон и размер прямоугольника букве
      cv::rectangle(image, boundingBox, cv::Scalar(0, 255, 0), 1);
   }

   // Перерисовка окна для отображения обновленного изображения
   InvalidateRect(GetActiveWindow(), NULL, TRUE);
}

void OnPaint(HWND hwnd)
{
   PAINTSTRUCT ps;
   HDC hdc = BeginPaint(hwnd, &ps);

   if (!image.empty()) {
      HBITMAP hBitmap;
      ConvertMatToHBITMAP(image, hBitmap);
      if (hBitmap) {
         HDC hdcMem = CreateCompatibleDC(hdc);
         HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

         RECT rc;
         GetClientRect(hwnd, &rc);
         BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);

         SelectObject(hdcMem, hOldBitmap);
         DeleteDC(hdcMem);
         DeleteObject(hBitmap);
      }
   }

   EndPaint(hwnd, &ps);
}

void OnResize(HWND hwnd)
{
   InvalidateRect(hwnd, NULL, TRUE);
}

void ConvertMatToHBITMAP(const cv::Mat& mat, HBITMAP& hBitmap)
{
   if (mat.empty()) {
      hBitmap = NULL;
      return;
   }

   cv::Mat matTemp;
   if (mat.channels() == 1) {
      cv::cvtColor(mat, matTemp, cv::COLOR_GRAY2BGRA);
   }
   else if (mat.channels() == 3) {
      cv::cvtColor(mat, matTemp, cv::COLOR_BGR2BGRA);
   }
   else if (mat.channels() == 4) {
      matTemp = mat.clone();
   }
   else {
      hBitmap = NULL;
      return;
   }

   BITMAPINFOHEADER bi;
   bi.biSize = sizeof(BITMAPINFOHEADER);
   bi.biWidth = matTemp.cols;
   bi.biHeight = -matTemp.rows;
   bi.biPlanes = 1;
   bi.biBitCount = 32;
   bi.biCompression = BI_RGB;
   bi.biSizeImage = 0;
   bi.biXPelsPerMeter = 0;
   bi.biYPelsPerMeter = 0;
   bi.biClrUsed = 0;
   bi.biClrImportant = 0;

   HDC hdc = GetDC(NULL);
   hBitmap = CreateDIBitmap(hdc, &bi, CBM_INIT, matTemp.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
   ReleaseDC(NULL, hdc);
}
