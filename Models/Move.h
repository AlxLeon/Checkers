#pragma once
#include <stdlib.h>

typedef int8_t POS_T;

struct move_pos {
  POS_T x, y;   // начальные координаты фигуры
  POS_T x2, y2; // конечные координаты фигуры
  POS_T xb = -1, yb = -1; // координаты побитой фигуры (если есть)
  // Конструктор для создания объекта хода с двумя позициями (начальная и
  // конечная)
  move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2)
      : x(x), y(y), x2(x2), y2(y2) {}
  // Конструктор для создания объекта хода с тремя позициями (начальная,
  // конечная и позиция побитой фигуры)
  move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2,
           const POS_T xb, const POS_T yb)
      : x(x), y(y), x2(x2), y2(y2), xb(xb), yb(yb) {}
  // Оператор сравнения на равенство двух ходов
  bool operator==(const move_pos &other) const {
    return (x == other.x && y == other.y && x2 == other.x2 && y2 == other.y2);
  }
  // Оператор сравнения на неравенство двух ходов
  bool operator!=(const move_pos &other) const { return !(*this == other); }
};
