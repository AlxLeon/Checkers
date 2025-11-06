#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// methods for hands
class Hand {
public:
  Hand(Board *board) : board(board) {}
  tuple<Response, POS_T, POS_T> get_cell() const {
    SDL_Event windowEvent;
    Response resp = Response::OK;
    int x = -1, y = -1;
    int xc = -1, yc = -1;
    while (true) {
      // Проверяем наличие событий в очереди событий SDL
      if (SDL_PollEvent(&windowEvent)) {
        switch (windowEvent.type) {
        case SDL_QUIT:
          // Обработка закрытия окна приложения
          resp = Response::QUIT;
          break;
        case SDL_MOUSEBUTTONDOWN:
          // Обработка нажатия мыши
          x = windowEvent.motion.x;
          y = windowEvent.motion.y;
          // Вычисление координат клетки на доске
          xc = int(y / (board->H / 10) - 1);
          yc = int(x / (board->W / 10) - 1);
          // Проверка, если кликнули по кнопке "Назад"
          if (xc == -1 && yc == -1 && board->history_mtx.size() > 1) {
            resp = Response::BACK;
            // Проверка, если кликнули по кнопке "Повтор"
          } else if (xc == -1 && yc == 8) {
            resp = Response::REPLAY;
            // Проверка, если кликнули по игровой доске
          } else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8) {
            resp = Response::CELL;
          } else {
            // Сброс координат, если клик был вне допустимых областей
            xc = -1;
            yc = -1;
          }
          break;
        case SDL_WINDOWEVENT:
          // Обработка изменения размера окна
          if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            board->reset_window_size();
            break;
          }
        }
        // Прерываем цикл, если получено действие, отличное от OK
        if (resp != Response::OK)
          break;
      }
    }
    // Возвращаем результат: тип действия и координаты клетки
    return {resp, xc, yc};
  }

  Response wait() const {
    SDL_Event windowEvent;
    Response resp = Response::OK;
    while (true) {
      // Проверяем наличие событий в очереди событий SDL
      if (SDL_PollEvent(&windowEvent)) {
        switch (windowEvent.type) {
        case SDL_QUIT:
          // Обработка события закрытия окна
          resp = Response::QUIT;
          break;
        case SDL_WINDOWEVENT_SIZE_CHANGED:
          // Обработка изменения размера окна
          board->reset_window_size();
          break;
        case SDL_MOUSEBUTTONDOWN: {
          // Получение координат нажатия мыши
          int x = windowEvent.motion.x;
          int y = windowEvent.motion.y;
          // Преобразование координат в индексы клеток доски
          int xc = int(y / (board->H / 10) - 1);
          int yc = int(x / (board->W / 10) - 1);
          // Проверка нажатия на кнопку "Replay" (кнопка в правом нижнем углу)
          if (xc == -1 && yc == 8)
            resp = Response::REPLAY;
        } break;
        }
        // Прерываем цикл при получении завершающего ответа
        if (resp != Response::OK)
          break;
      }
    }
    return resp;
  }

private:
  Board *board;
};
