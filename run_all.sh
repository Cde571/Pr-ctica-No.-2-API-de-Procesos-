#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

echo "=== [WSL] ubicacion del proyecto ==="
pwd
ls

echo "=== [WSL] dependencias ==="
sudo apt update
sudo apt install -y build-essential

echo "=== [WSL] compilar ==="
make

echo "=== [WSL] verificar ejecutable ==="
ls -l wish

echo "=== [WSL] correr wish en modo batch (tests/*.txt) ==="
mkdir -p logs
rm -f logs/*.log || true

shopt -s nullglob
files=(tests/*.txt)

if (( ${#files[@]} == 0 )); then
  echo "No hay archivos tests/*.txt"
else
  for f in "${files[@]}"; do
    b="$(basename "$f")"
    tmp="$(mktemp)"

    # Quita comentarios (#...) y lineas en blanco para evitar errores extra
    sed -e '/^[[:space:]]*#/d' -e '/^[[:space:]]*$/d' "$f" > "$tmp"

    echo "----- RUNNING: $f -----" | tee -a logs/all_tests.log
    ./wish "$tmp" 2>&1 | tee -a "logs/${b}.log" | tee -a logs/all_tests.log
    echo "" | tee -a logs/all_tests.log

    rm -f "$tmp"
  done
fi

echo "=== [WSL] listo. logs en: logs/ ==="
ls -1 logs