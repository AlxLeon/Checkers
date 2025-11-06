#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game {
public:
  Game()
      : board(config("WindowSize", "Width"), config("WindowSize", "Hight")),
        hand(&board), logic(&board, &config) {
    ofstream fout(project_path + "log.txt", ios_base::trunc);
    fout.close();
  }

  // to start checkers
  int play() {
    // Запоминаем время начала игры для логирования
    auto start = chrono::steady_clock::now();

    // Если это повтор игры, перезагружаем логику и конфигурацию
    if (is_replay) {
      logic = Logic(&board, &config);
      config.reload();
      board.redraw();
    } else {
      // Иначе начинаем новую игру с отрисовкой доски
      board.start_draw();
    }
    is_replay = false;

    // Инициализация переменных для управления игровым циклом
    int turn_num = -1;
    bool is_quit = false;
    const int Max_turns = config("Game", "MaxNumTurns");

    // Основной игровой цикл по ходам
    while (++turn_num < Max_turns) {
      beat_series = 0; // Сброс серии захватов

      // Поиск возможных ходов для текущего игрока
      logic.find_turns(turn_num % 2);

      // Если ходы отсутствуют, игра завершается
      if (logic.turns.empty())
        break;

      // Установка глубины поиска для бота
      logic.Max_depth =
          config("Bot", string((turn_num % 2) ? "Black" : "White") +
                            string("BotLevel"));

      // Проверяем, является ли текущий игрок ботом
      if (!config("Bot", string("Is") +
                             string((turn_num % 2) ? "Black" : "White") +
                             string("Bot"))) {
        // Если игрок не бот - ждем его ход
        auto resp = player_turn(turn_num % 2);
        if (resp == Response::QUIT) {
          // Игрок вышел из игры
          is_quit = true;
          break;
        } else if (resp == Response::REPLAY) {
          // Игрок запросил повтор игры
          is_replay = true;
          break;
        } else if (resp == Response::BACK) {
          // Игрок хочет отменить ход
          // Проверяем возможность отката для бота и если нет серии захватов
          if (config("Bot", string("Is") +
                                string((1 - turn_num % 2) ? "Black" : "White") +
                                string("Bot")) &&
              !beat_series && board.history_mtx.size() > 2) {
            board.rollback();
            --turn_num;
          }
          // Откатываем ход текущего игрока
          if (!beat_series)
            --turn_num;
          board.rollback();
          --turn_num;
          beat_series = 0;
        }
      } else {
        // Если игрок бот - делаем ход бота
        bot_turn(turn_num % 2);
      }
    }

    // Логирование времени игры
    auto end = chrono::steady_clock::now();
    ofstream fout(project_path + "log.txt", ios_base::app);
    fout << "Game time: "
         << (int)chrono::duration<double, milli>(end - start).count()
         << " millisec\n";
    fout.close();

    // Обработка результатов игры
    if (is_replay)
      return play(); // Запуск повторной игры
    if (is_quit)
      return 0; // Выход из игры

    int res = 2; // По умолчанию - ничья
    if (turn_num == Max_turns) {
      res = 0; // Максимальное количество ходов достигнуто - ничья
    } else if (turn_num % 2) {
      res = 1; // Белые выиграли (нечетный номер хода)
    }

    // Отображение финального результата
    board.show_final(res);
    auto resp =
        hand.wait(); // Ожидание действия пользователя после окончания игры

    if (resp == Response::REPLAY) {
      is_replay = true;
      return play(); // Повтор игры
    }
    return res; // Возврат результата игры
  }

private:
  void bot_turn(const bool color) {
    auto start = chrono::steady_clock::now(); // Записываем начальное время для измерения времени хода бота

    auto delay_ms = config("Bot", "BotDelayMS"); // Получаем задержку в миллисекундах из конфигурации для бота
    // new thread for equal delay for each turn
    thread th(SDL_Delay, delay_ms); // Создаем отдельный поток для задержки, чтобы она не блокировала основной поток
    auto turns = logic.find_best_turns(color); // Находим лучшие ходы для текущего цвета с помощью логики игры
    th.join(); // Ожидаем завершения потока с задержкой перед выполнением ходов
    bool is_first = true; // Флаг для отслеживания первого хода в серии
    
    // making moves - выполнение всех найденных ходов
    for (auto turn : turns) {
      if (!is_first) {
        SDL_Delay(delay_ms); // Добавляем задержку между последовательными ходами (кроме первого)
      }
      is_first = false;
      beat_series += (turn.xb != -1); // Увеличиваем счетчик побитых фигур при необходимости
      board.move_piece(turn, beat_series); // Выполняем ход на доске
    }

    auto end = chrono::steady_clock::now(); // Записываем конечное время после выполнения ходов
    ofstream fout(project_path + "log.txt", ios_base::app); // Открываем файл лога для добавления информации
    fout << "Bot turn time: "
         << (int)chrono::duration<double, milli>(end - start).count()
         << " millisec\n"; // Записываем время хода бота в лог
    fout.close(); // Закрываем файл лога
  }

  Response player_turn(const bool color) {
    // Подготовка списка возможных начальных позиций для подсветки
    vector<pair<POS_T, POS_T>> cells;
    for (auto turn : logic.turns) {
      cells.emplace_back(turn.x, turn.y);
    }
    board.highlight_cells(cells);

    move_pos pos = {-1, -1, -1, -1}; // Позиция для хода
    POS_T x = -1, y = -1; // Координаты выбранной фигуры

    // Основной цикл выбора начальной позиции
    while (true) {
      auto resp = hand.get_cell(); // Получаем выбор игрока

      // Если игрок хочет выйти из игры
      if (get<0>(resp) != Response::CELL)
        return get<0>(resp);

      pair<POS_T, POS_T> cell{get<1>(resp),
                              get<2>(resp)}; // Получаем координаты

      bool is_correct = false;

      // Проверяем, является ли выбранный ход допустимым
      for (auto turn : logic.turns) {
        if (turn.x == cell.first && turn.y == cell.second) {
          is_correct = true;
          break;
        }
        if (turn == move_pos{x, y, cell.first, cell.second}) {
          pos = turn;
          break;
        }
      }

      // Если ход уже выбран, выходим из цикла
      if (pos.x != -1)
        break;

      // Если ход недопустимый
      if (!is_correct) {
        if (x != -1) {
          board.clear_active(); // Снимаем активное выделение
          board.clear_highlight(); // Снимаем подсветку
          board.highlight_cells(cells); // Подсвечиваем возможные ходы снова
        }
        x = -1;
        y = -1;
        continue;
      }

      // Устанавливаем выбранную фигуру как активную
      x = cell.first;
      y = cell.second;
      board.clear_highlight(); // Снимаем подсветку
      board.set_active(x, y); // Активируем выбранную фигуру

      // Подготавливаем список возможных конечных позиций
      vector<pair<POS_T, POS_T>> cells2;
      for (auto turn : logic.turns) {
        if (turn.x == x && turn.y == y) {
          cells2.emplace_back(turn.x2, turn.y2);
        }
      }
      board.highlight_cells(cells2); // Подсвечиваем возможные конечные позиции
    }

    // Завершаем ход и выполняем перемещение фигуры
    board.clear_highlight();
    board.clear_active();
    board.move_piece(pos, pos.xb != -1); // Выполняем ход

    // Если это обычный ход (не побитие), возвращаем OK
    if (pos.xb == -1)
      return Response::OK;

    // Обработка серии побитий
    beat_series = 1;
    while (true) {
      logic.find_turns(pos.x2, pos.y2); // Ищем возможные побития

      // Если побитий больше нет, завершаем серию
      if (!logic.have_beats)
        break;

      // Подготавливаем список возможных побитий
      vector<pair<POS_T, POS_T>> cells;
      for (auto turn : logic.turns) {
        cells.emplace_back(turn.x2, turn.y2);
      }
      board.highlight_cells(cells); // Подсвечиваем возможные побития
      board.set_active(pos.x2, pos.y2); // Активируем последнюю фигуру

      // Цикл выбора следующего побития
      while (true) {
        auto resp = hand.get_cell(); // Получаем выбор игрока

        // Если игрок хочет выйти
        if (get<0>(resp) != Response::CELL)
          return get<0>(resp);

        pair<POS_T, POS_T> cell{get<1>(resp),
                                get<2>(resp)}; // Получаем координаты

        bool is_correct = false;

        // Проверяем корректность выбранного побития
        for (auto turn : logic.turns) {
          if (turn.x2 == cell.first && turn.y2 == cell.second) {
            is_correct = true;
            pos = turn;
            break;
          }
        }

        // Если выбранное побитие некорректно, продолжаем ожидание
        if (!is_correct)
          continue;

        board.clear_highlight(); // Снимаем подсветку
        board.clear_active(); // Снимаем активное выделение
        beat_series += 1; // Увеличиваем счетчик побитий
        board.move_piece(pos, beat_series); // Выполняем побитие
        break; // Переходим к следующему шагу
      }
    }

    return Response::OK;
  }

private:
  Config config;
  Board board;
  Hand hand;
  Logic logic;
  int beat_series;
  bool is_replay = false;
};
