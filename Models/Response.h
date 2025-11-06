#pragma once

enum class Response {
  // Успешное выполнение операции
  OK,

  // Отменить ход
  BACK,

  // Повтор игры
  REPLAY,

  // Выход
  QUIT,

  // Выбор ячейки (например, в игре)
  CELL
};
