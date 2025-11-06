#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic {
public:
  Logic(Board *board, Config *config) : board(board), config(config) {
    rand_eng = std::default_random_engine(
        !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
    scoring_mode = (*config)("Bot", "BotScoringType");
    optimization = (*config)("Bot", "Optimization");
  }

  vector<move_pos> find_best_turns(const bool color) {
    next_best_state.clear();
    next_move.clear();

    find_first_best_turn(board->get_board(), color, -1, -1, 0);

    int cur_state = 0;
    vector<move_pos> res;
    do {
      res.push_back(next_move[cur_state]);
      cur_state = next_best_state[cur_state];
    } while (cur_state != -1 && next_move[cur_state].x != -1);
    return res;
  }

private:
  vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx,
                                  move_pos turn) const {
    // Если есть начальная позиция фигуры, убираем её с доски
    if (turn.xb != -1)
      mtx[turn.xb][turn.yb] = 0;

    // Проверяем условие превращения пешки: если фигура - пешка (1 или 2)
    // и достигла конца доски (x2 == 0 или x2 == 7), то превращаем её в дамку
    if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) ||
        (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
      mtx[turn.x][turn.y] += 2;

    // Перемещаем фигуру на новую позицию
    mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
    // Очищаем старую позицию
    mtx[turn.x][turn.y] = 0;
    return mtx;
  }

  // Рассчитывает оценку позиции на доске
  double calc_score(const vector<vector<POS_T>> &mtx,
                    const bool first_bot_color) const {
    // color - кто является максимизирующим игроком
    double w = 0, wq = 0, b = 0, bq = 0;
    for (POS_T i = 0; i < 8; ++i) {
      for (POS_T j = 0; j < 8; ++j) {
        // Подсчет белых пешек (1) и белых ферзей (3)
        w += (mtx[i][j] == 1);
        wq += (mtx[i][j] == 3);
        // Подсчет черных пешек (2) и черных ферзей (4)
        b += (mtx[i][j] == 2);
        bq += (mtx[i][j] == 4);
        // Если режим оценки "NumberAndPotential", добавляем потенциал позиции
        if (scoring_mode == "NumberAndPotential") {
          w += 0.05 * (mtx[i][j] == 1) * (7 - i);
          b += 0.05 * (mtx[i][j] == 2) * (i);
        }
      }
    }
    // Если первый бот - черные, меняем значения местами
    if (!first_bot_color) {
      swap(b, w);
      swap(bq, wq);
    }
    // Если у белых нет фигур, позиция выигрышная для черных
    if (w + wq == 0)
      return INF;
    // Если у черных нет фигур, позиция выигрышная для белых
    if (b + bq == 0)
      return 0;
    int q_coef = 4;
    // В режиме "NumberAndPotential" коэффициент для ферзей увеличен
    if (scoring_mode == "NumberAndPotential") {
      q_coef = 5;
    }
    // Возвращаем соотношение силы черных фигур к белым с учетом коэффициента
    // ферзей
    return (b + bq * q_coef) / (w + wq * q_coef);
  }

  double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color,
                              const POS_T x, const POS_T y, size_t state,
                              double alpha = -1) {
    next_best_state.push_back(-1);
    next_move.emplace_back(-1, -1, -1, -1);
    double best_score = -1;
    if (state != 0)
      find_turns(x, y, mtx);
    auto turns_now = turns;
    bool have_beats_now = have_beats;

    if (!have_beats_now && state != 0) {
      return find_best_turns_rec(mtx, 1 - color, 0, alpha);
    }

    vector<move_pos> best_moves;
    vector<int> best_states;

    for (auto turn : turns_now) {
      size_t next_state = next_move.size();
      double score;
      if (have_beats_now) {
        score = find_first_best_turn(make_turn(mtx, turn), color, turn.x2,
                                     turn.y2, next_state, best_score);
      } else {
        score =
            find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score);
      }
      if (score > best_score) {
        best_score = score;
        next_best_state[state] = (have_beats_now ? int(next_state) : -1);
        next_move[state] = turn;
      }
    }
    return best_score;
  }

  double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color,
                             const size_t depth, double alpha = -1,
                             double beta = INF + 1, const POS_T x = -1,
                             const POS_T y = -1) {
    if (depth == Max_depth) {
      return calc_score(mtx, (depth % 2 == color));
    }
    if (x != -1) {
      find_turns(x, y, mtx);
    } else
      find_turns(color, mtx);
    auto turns_now = turns;
    bool have_beats_now = have_beats;

    if (!have_beats_now && x != -1) {
      return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta);
    }

    if (turns.empty())
      return (depth % 2 ? 0 : INF);

    double min_score = INF + 1;
    double max_score = -1;
    for (auto turn : turns_now) {
      double score = 0.0;
      if (!have_beats_now && x == -1) {
        score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1,
                                    alpha, beta);
      } else {
        score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha,
                                    beta, turn.x2, turn.y2);
      }
      min_score = min(min_score, score);
      max_score = max(max_score, score);
      // alpha-beta pruning
      if (depth % 2)
        alpha = max(alpha, max_score);
      else
        beta = min(beta, min_score);
      if (optimization != "O0" && alpha >= beta)
        return (depth % 2 ? max_score + 1 : min_score - 1);
    }
    return (depth % 2 ? max_score : min_score);
  }

  // Перегрузка функции: находит все возможные ходы для заданного цвета
  void find_turns(const bool color) { find_turns(color, board->get_board()); }

  // Перегрузка функции: находит все возможные ходы для фигуры в указанной
  // позиции
  void find_turns(const POS_T x, const POS_T y) {
    find_turns(x, y, board->get_board());
  }

  // Перегрузка функции: находит все возможные ходы для заданного цвета на
  // заданной доске
  void find_turns(const bool color, const vector<vector<POS_T>> &mtx) {
    vector<move_pos> res_turns;
    bool have_beats_before = false;
    for (POS_T i = 0; i < 8; ++i) {
      for (POS_T j = 0; j < 8; ++j) {
        if (mtx[i][j] && mtx[i][j] % 2 != color) {
          find_turns(i, j, mtx);
          if (have_beats && !have_beats_before) {
            have_beats_before = true;
            res_turns.clear();
          }
          if ((have_beats_before && have_beats) || !have_beats_before) {
            res_turns.insert(res_turns.end(), turns.begin(), turns.end());
          }
        }
      }
    }
    turns = res_turns;
    shuffle(turns.begin(), turns.end(), rand_eng);
    have_beats = have_beats_before;
  }

  // Перегрузка функции: находит все возможные ходы для фигуры в указанной
  // позиции на заданной доске
  void find_turns(const POS_T x, const POS_T y,
                  const vector<vector<POS_T>> &mtx) {
    turns.clear();
    have_beats = false;
    POS_T type = mtx[x][y];
    // проверяем возможность взятия фигур
    switch (type) {
    case 1:
    case 2:
      // проверяем обычные фигуры
      for (POS_T i = x - 2; i <= x + 2; i += 4) {
        for (POS_T j = y - 2; j <= y + 2; j += 4) {
          if (i < 0 || i > 7 || j < 0 || j > 7)
            continue;
          POS_T xb = (x + i) / 2, yb = (y + j) / 2;
          if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
            continue;
          turns.emplace_back(x, y, i, j, xb, yb);
        }
      }
      break;
    default:
      // проверяем королев (дамки)
      for (POS_T i = -1; i <= 1; i += 2) {
        for (POS_T j = -1; j <= 1; j += 2) {
          POS_T xb = -1, yb = -1;
          for (POS_T i2 = x + i, j2 = y + j;
               i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j) {
            if (mtx[i2][j2]) {
              if (mtx[i2][j2] % 2 == type % 2 ||
                  (mtx[i2][j2] % 2 != type % 2 && xb != -1)) {
                break;
              }
              xb = i2;
              yb = j2;
            }
            if (xb != -1 && xb != i2) {
              turns.emplace_back(x, y, i2, j2, xb, yb);
            }
          }
        }
      }
      break;
    }
    // если есть возможности взятия, то это обязательные ходы
    if (!turns.empty()) {
      have_beats = true;
      return;
    }
    switch (type) {
    case 1:
    case 2:
      // проверяем обычные фигуры
      {
        POS_T i = ((type % 2) ? x - 1 : x + 1);
        for (POS_T j = y - 1; j <= y + 1; j += 2) {
          if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
            continue;
          turns.emplace_back(x, y, i, j);
        }
        break;
      }
    default:
      // проверяем королев (дамки)
      for (POS_T i = -1; i <= 1; i += 2) {
        for (POS_T j = -1; j <= 1; j += 2) {
          for (POS_T i2 = x + i, j2 = y + j;
               i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j) {
            if (mtx[i2][j2])
              break;
            turns.emplace_back(x, y, i2, j2);
          }
        }
      }
      break;
    }
  }

public:
  /// Вектор ходов в текущей игре
  vector<move_pos> turns;
  
  /// Флаг, указывающий наличие обязательных ходов (захватов)
  bool have_beats;
  
  /// Максимальная глубина поиска при использовании алгоритмов
  int Max_depth;

private:
  /// Генератор случайных чисел для случайных элементов игры
  default_random_engine rand_eng;
  
  /// Поведение бота "`NumberOnly`" (бот учитывает только количество шашек)  or "`NumberAndPotential`" (бот учитывает количество и расположение шашек). 
  string scoring_mode;
  
  /// Оптимизация веток поиска, чем выше уровень сложности, тем больше требуется оптимизация.
  string optimization;
  
  /// Следующий ход, который будет выполнен
  vector<move_pos> next_move;
  
  /// Лучшее состояние после хода (для оценки)
  vector<int> next_best_state;
  
  /// Указатель на игровую доску
  Board *board;
  
  /// Указатель на конфигурационные параметры
  Config *config;
};