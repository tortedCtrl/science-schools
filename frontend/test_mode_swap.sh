#!/usr/bin/env bash
# frontend/test_mode.sh
# Меняет местами содержимое backend.tsx и mock.tsx.
# Использование:  ./test_mode.sh
# Запускать из каталога frontend/ (или откуда угодно — путь считается от скрипта).

set -euo pipefail

# --- Каталог скрипта (он же frontend/) ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# --- Пути к файлам (поправь, если у тебя hooks/backend.ts, а не backend.tsx) ---
REAL_FILE="$SCRIPT_DIR/src/hooks/backend.tsx"
MOCK_FILE="$SCRIPT_DIR/src/hooks/mock.tsx"

# --- Проверки ---
if [[ ! -f "$REAL_FILE" ]]; then
  echo "Ошибка: не найден $REAL_FILE" >&2
  exit 1
fi
if [[ ! -f "$MOCK_FILE" ]]; then
  echo "Ошибка: не найден $MOCK_FILE" >&2
  exit 1
fi

# --- Предупреждение, если Vite уже запущен (свап на лету может привести к HMR-ошибкам) ---
if pgrep -af "vite" >/dev/null 2>&1; then
  echo "Внимание: обнаружен запущенный процесс vite. Рекомендуется остановить его перед свапом."
  read -r -p "Продолжить? [y/N] " ans
  case "$ans" in
    y|Y|yes|YES) ;;
    *) echo "Отменено."; exit 0 ;;
  esac
fi

# --- Свап содержимого через временный файл ---
TMP="$(mktemp "${TMPDIR:-/tmp}/test_mode.XXXXXX")"
trap 'rm -f "$TMP"' EXIT

cp "$REAL_FILE" "$TMP"
cp "$MOCK_FILE" "$REAL_FILE"
cp "$TMP" "$MOCK_FILE"

# --- Отчёт ---
echo "Содержимое файлов поменяно местами:"
echo "  $REAL_FILE"
echo "  $MOCK_FILE"
echo "Текущий режим: см. первый комментарий в $REAL_FILE (если он там есть)."

# --- Определяем «какой сейчас режим» по содержимому реального файла ---
if grep -q "USE_MOCK[[:space:]]*=[[:space:]]*true" "$REAL_FILE" 2>/dev/null; then
  echo "Похоже, сейчас активен МОК."
elif grep -q "from '../api'" "$REAL_FILE" 2>/dev/null; then
  echo "Похоже, сейчас активен РЕАЛЬНЫЙ API."
else
  echo "Не удалось определить режим по содержимому — проверь файлы вручную."
fi