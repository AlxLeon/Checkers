#pragma once
#include <fstream>
#include <iostream>
#include <vector>

#include "../Models/Move.h"
#include "../Models/Project_path.h"

#ifdef __APPLE__
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#endif

using namespace std;

class Board {
public:
  Board() = default;
  Board(const unsigned int W, const unsigned int H) : W(W), H(H) {}

  // draws start board
  int start_draw() {
    // Инициализация SDL2
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
      print_exception("SDL_Init can't init SDL2 lib");
      return 1;
    }

    // Если ширина или высота не заданы, получаем размеры экрана
    if (W == 0 || H == 0) {
      SDL_DisplayMode dm;
      if (SDL_GetDesktopDisplayMode(0, &dm)) {
        print_exception(
            "SDL_GetDesktopDisplayMode can't get desctop display mode");
        return 1;
      }
      W = min(dm.w, dm.h);
      W -= W / 15; // Уменьшаем на 1/15 для отступов
      H = W;
    }

    // Создание окна с заданными параметрами
    win = SDL_CreateWindow("Checkers", 0, H / 30, W, H, SDL_WINDOW_RESIZABLE);
    if (win == nullptr) {
      print_exception("SDL_CreateWindow can't create window");
      return 1;
    }

    // Создание рендерера с ускорением и вертикальной синхронизацией
    ren = SDL_CreateRenderer(
        win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (ren == nullptr) {
      print_exception("SDL_CreateRenderer can't create renderer");
      return 1;
    }

    // Загрузка текстур для игровых элементов
    board = IMG_LoadTexture(ren, board_path.c_str());
    w_piece = IMG_LoadTexture(ren, piece_white_path.c_str());
    b_piece = IMG_LoadTexture(ren, piece_black_path.c_str());
    w_queen = IMG_LoadTexture(ren, queen_white_path.c_str());
    b_queen = IMG_LoadTexture(ren, queen_black_path.c_str());
    back = IMG_LoadTexture(ren, back_path.c_str());
    replay = IMG_LoadTexture(ren, replay_path.c_str());

    // Проверка успешности загрузки всех текстур
    if (!board || !w_piece || !b_piece || !w_queen || !b_queen || !back ||
        !replay) {
      print_exception("IMG_LoadTexture can't load main textures from " +
                      textures_path);
      return 1;
    }

    // Получение фактического размера рендерера
    SDL_GetRendererOutputSize(ren, &W, &H);

    // Создание начальной матрицы состояния доски
    make_start_mtx();

    // Первичная отрисовка доски
    rerender();

    return 0;
  }

  void redraw() {
    game_results = -1;
    history_mtx.clear();
    history_beat_series.clear();
    make_start_mtx();
    clear_active();
    clear_highlight();
  }

  void move_piece(move_pos turn, const int beat_series = 0) {
    if (turn.xb != -1) {
      mtx[turn.xb][turn.yb] = 0;
    }
    move_piece(turn.x, turn.y, turn.x2, turn.y2, beat_series);
  }

  void move_piece(const POS_T i, const POS_T j, const POS_T i2, const POS_T j2,
                  const int beat_series = 0) {
    if (mtx[i2][j2]) {
      throw runtime_error("final position is not empty, can't move");
    }
    if (!mtx[i][j]) {
      throw runtime_error("begin position is empty, can't move");
    }
    if ((mtx[i][j] == 1 && i2 == 0) || (mtx[i][j] == 2 && i2 == 7))
      mtx[i][j] += 2;
    mtx[i2][j2] = mtx[i][j];
    drop_piece(i, j);
    add_history(beat_series);
  }

  void drop_piece(const POS_T i, const POS_T j) {
    mtx[i][j] = 0;
    rerender();
  }

  void turn_into_queen(const POS_T i, const POS_T j) {
    if (mtx[i][j] == 0 || mtx[i][j] > 2) {
      throw runtime_error("can't turn into queen in this position");
    }
    mtx[i][j] += 2;
    rerender();
  }
  vector<vector<POS_T>> get_board() const { return mtx; }

  void highlight_cells(vector<pair<POS_T, POS_T>> cells) {
    for (auto pos : cells) {
      POS_T x = pos.first, y = pos.second;
      is_highlighted_[x][y] = 1;
    }
    rerender();
  }

  void clear_highlight() {
    for (POS_T i = 0; i < 8; ++i) {
      is_highlighted_[i].assign(8, 0);
    }
    rerender();
  }

  void set_active(const POS_T x, const POS_T y) {
    active_x = x;
    active_y = y;
    rerender();
  }

  void clear_active() {
    active_x = -1;
    active_y = -1;
    rerender();
  }

  bool is_highlighted(const POS_T x, const POS_T y) {
    return is_highlighted_[x][y];
  }

  void rollback() {
    auto beat_series = max(1, *(history_beat_series.rbegin()));
    while (beat_series-- && history_mtx.size() > 1) {
      history_mtx.pop_back();
      history_beat_series.pop_back();
    }
    mtx = *(history_mtx.rbegin());
    clear_highlight();
    clear_active();
  }

  void show_final(const int res) {
    game_results = res;
    rerender();
  }

  // use if window size changed
  void reset_window_size() {
    SDL_GetRendererOutputSize(ren, &W, &H);
    rerender();
  }

  void quit() {
    SDL_DestroyTexture(board);
    SDL_DestroyTexture(w_piece);
    SDL_DestroyTexture(b_piece);
    SDL_DestroyTexture(w_queen);
    SDL_DestroyTexture(b_queen);
    SDL_DestroyTexture(back);
    SDL_DestroyTexture(replay);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
  }

  ~Board() {
    if (win)
      quit();
  }

private:
  void add_history(const int beat_series = 0) {
    history_mtx.push_back(mtx);
    history_beat_series.push_back(beat_series);
  }
  // function to make start matrix
  void make_start_mtx() {
    for (POS_T i = 0; i < 8; ++i) {
      for (POS_T j = 0; j < 8; ++j) {
        mtx[i][j] = 0;
        if (i < 3 && (i + j) % 2 == 1)
          mtx[i][j] = 2;
        if (i > 4 && (i + j) % 2 == 1)
          mtx[i][j] = 1;
      }
    }
    add_history();
  }

  // function that re-draw all the textures
  void rerender() {
    // draw board
    SDL_RenderClear(ren); // Очищаем экран перед отрисовкой нового кадра
    SDL_RenderCopy(ren, board, NULL, NULL); // Отображаем игровое поле

    // draw pieces - отрисовка фигур
    for (POS_T i = 0; i < 8; ++i) {
      for (POS_T j = 0; j < 8; ++j) {
        if (!mtx[i][j]) // Если ячейка пуста, пропускаем её
          continue;
        int wpos = W * (j + 1) / 10 + W / 120; // Вычисляем позицию по ширине
        int hpos = H * (i + 1) / 10 + H / 120; // Вычисляем позицию по высоте
        SDL_Rect rect{wpos, hpos, W / 12,
                      H / 12}; // Создаем прямоугольник для фигуры

        SDL_Texture *piece_texture; // Текстура для отображения фигуры
        if (mtx[i][j] == 1) // Белая пешка
          piece_texture = w_piece;
        else if (mtx[i][j] == 2) // Черная пешка
          piece_texture = b_piece;
        else if (mtx[i][j] == 3) // Белая королева
          piece_texture = w_queen;
        else // Черная королева
          piece_texture = b_queen;

        SDL_RenderCopy(ren, piece_texture, NULL, &rect); // Рисуем фигуру
      }
    }

    // draw hilight - отрисовка подсветки возможных ходов
    SDL_SetRenderDrawColor(ren, 0, 255, 0,
                           0); // Устанавливаем зеленый цвет для подсветки
    const double scale = 2.5; // Масштаб для корректного отображения подсветки
    SDL_RenderSetScale(ren, scale, scale); // Применяем масштаб
    for (POS_T i = 0; i < 8; ++i) {
      for (POS_T j = 0; j < 8; ++j) {
        if (!is_highlighted_[i][j]) // Если клетка не подсвечена, пропускаем её
          continue;
        SDL_Rect cell{
            int(W * (j + 1) / 10 / scale), // Позиция клетки по горизонтали
            int(H * (i + 1) / 10 / scale), // Позиция клетки по вертикали
            int(W / 10 / scale),  // Ширина клетки
            int(H / 10 / scale)}; // Высота клетки
        SDL_RenderDrawRect(ren, &cell); // Рисуем контур подсвеченной клетки
      }
    }

    // draw active - отрисовка активной клетки (выбранной фигуры)
    if (active_x != -1) { // Если есть выбранная фигура
      SDL_SetRenderDrawColor(
          ren, 255, 0, 0, 0); // Устанавливаем красный цвет для активной клетки
      SDL_Rect active_cell{int(W * (active_y + 1) / 10 /
                               scale), // Позиция активной клетки по горизонтали
                           int(H * (active_x + 1) / 10 /
                               scale), // Позиция активной клетки по вертикали
                           int(W / 10 / scale), // Ширина активной клетки
                           int(H / 10 / scale)}; // Высота активной клетки
      SDL_RenderDrawRect(ren, &active_cell); // Рисуем контур активной клетки
    }
    SDL_RenderSetScale(ren, 1, 1); // Сбрасываем масштаб обратно к 1:1

    // draw arrows - отрисовка стрелок управления
    SDL_Rect rect_left{W / 40, H / 40, W / 15,
                       H / 15}; // Позиция и размер кнопки "назад"
    SDL_RenderCopy(ren, back, NULL, &rect_left); // Рисуем кнопку "назад"
    SDL_Rect replay_rect{W * 109 / 120, H / 40, W / 15,
                         H / 15}; // Позиция и размер кнопки "повтор"
    SDL_RenderCopy(ren, replay, NULL, &replay_rect); // Рисуем кнопку "повтор"

    // draw result - отрисовка результата игры
    if (game_results != -1) { // Если игра завершена
      string result_path = draw_path; // Путь к изображению ничьи по умолчанию
      if (game_results == 1) // Белые выиграли
        result_path = white_path;
      else if (game_results == 2) // Черные выиграли
        result_path = black_path;
      SDL_Texture *result_texture = IMG_LoadTexture(
          ren, result_path.c_str()); // Загружаем текстуру результата
      if (result_texture == nullptr) { // Проверяем успешность загрузки
        print_exception("IMG_LoadTexture can't load game result picture from " +
                        result_path);
        return;
      }
      SDL_Rect res_rect{W / 5, H * 3 / 10, W * 3 / 5,
                        H * 2 / 5}; // Позиция и размер окна результата
      SDL_RenderCopy(ren, result_texture, NULL,
                     &res_rect); // Отображаем результат
      SDL_DestroyTexture(
          result_texture); // Освобождаем память от текстуры результата
    }

    SDL_RenderPresent(ren); // Отображаем всё на экране
    // next rows for mac os - задержка для macOS
    SDL_Delay(10); // Небольшая задержка для стабильной работы на macOS
    SDL_Event windowEvent; // Переменная для обработки событий окна
    SDL_PollEvent(&windowEvent); // Опрос событий окна
  }

  void print_exception(const string &text) {
    ofstream fout(project_path + "log.txt", ios_base::app);
    fout << "Error: " << text << ". " << SDL_GetError() << endl;
    fout.close();
  }

public:
  int W = 0;
  int H = 0;
  // history of boards
  vector<vector<vector<POS_T>>> history_mtx;

private:
  SDL_Window *win = nullptr;
  SDL_Renderer *ren = nullptr;
  // textures
  SDL_Texture *board = nullptr;
  SDL_Texture *w_piece = nullptr;
  SDL_Texture *b_piece = nullptr;
  SDL_Texture *w_queen = nullptr;
  SDL_Texture *b_queen = nullptr;
  SDL_Texture *back = nullptr;
  SDL_Texture *replay = nullptr;
  // texture files names
  const string textures_path = project_path + "Textures/";
  const string board_path = textures_path + "board.png";
  const string piece_white_path = textures_path + "piece_white.png";
  const string piece_black_path = textures_path + "piece_black.png";
  const string queen_white_path = textures_path + "queen_white.png";
  const string queen_black_path = textures_path + "queen_black.png";
  const string white_path = textures_path + "white_wins.png";
  const string black_path = textures_path + "black_wins.png";
  const string draw_path = textures_path + "draw.png";
  const string back_path = textures_path + "back.png";
  const string replay_path = textures_path + "replay.png";
  // coordinates of chosen cell
  int active_x = -1, active_y = -1;
  // game result if exist
  int game_results = -1;
  // matrix of possible moves
  vector<vector<bool>> is_highlighted_ =
      vector<vector<bool>>(8, vector<bool>(8, 0));
  // matrix of possible moves
  // 1 - white, 2 - black, 3 - white queen, 4 - black queen
  vector<vector<POS_T>> mtx = vector<vector<POS_T>>(8, vector<POS_T>(8, 0));
  // series of beats for each move
  vector<int> history_beat_series;
};
