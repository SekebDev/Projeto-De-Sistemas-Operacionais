#!/usr/bin/env bash
# Roda o experimento completo no Linux nativo.
#
#   cd experimentos && ./run_linux.sh
#
# 12 configuracoes (2 versoes x 2 PxC x 3 buffers) x REPS repeticoes,
# em ordem embaralhada, com 1 execucao de aquecimento descartada por
# configuracao. Depois roda as medicoes de latencia (1x1, buffer 1).
set -euo pipefail
export LC_ALL=C   # saida dos comandos em ingles (free, date), facil de filtrar

ITENS=${ITENS:-400000}
REPS=${REPS:-30}
LAT_REPS=${LAT_REPS:-5}

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
BENCH="$RAIZ/code/bench"
DADOS="$RAIZ/dados/linux"
VAZAO="$DADOS/vazao.csv"
LAT="$DADOS/latencia.csv"

mkdir -p "$DADOS"
make -C "$RAIZ/code" bench >/dev/null

# ---- ambiente ----
{
    echo "data: $(date -Iseconds)"
    echo "kernel: $(uname -srmo)"
    [ -f /etc/os-release ] && grep PRETTY_NAME /etc/os-release
    echo "cpu: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 | sed 's/^ //')"
    echo "nucleos logicos: $(nproc)"
    echo "ram: $(free -h | awk '/Mem:/{print $2}')"
    echo "compilador: $(cc --version | head -1)"
    echo "flags: -O2 -Wall -Wextra -std=c99 -pthread"
    echo "governor: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo n/d)"
    echo "itens: $ITENS  reps: $REPS  lat_reps: $LAT_REPS"
} > "$DADOS/ambiente.txt"

# ---- vazao ----
[ -f "$VAZAO" ] || echo "so,versao,produtores,consumidores,buffer,itens,rep,tempo_s,vazao_itens_s" > "$VAZAO"

lista=$(mktemp)
for v in A B; do
  for pc in "1 1" "4 4"; do
    for n in 1 16 256; do
      for r in $(seq 1 "$REPS"); do
        echo "$v $pc $n $r" >> "$lista"
      done
    done
  done
done
total=$(wc -l < "$lista")

# aquecimento: 1 execucao descartada por configuracao
for v in A B; do
  for pc in "1 1" "4 4"; do
    for n in 1 16 256; do
      set -- $pc
      "$BENCH" --version "$v" --producers "$1" --consumers "$2" --buffer "$n" \
               --items "$ITENS" --rep 0 --os linux > /dev/null
    done
  done
done

i=0
while read -r v p c n r; do
    i=$((i + 1))
    "$BENCH" --version "$v" --producers "$p" --consumers "$c" --buffer "$n" \
             --items "$ITENS" --rep "$r" --os linux >> "$VAZAO"
    printf '\r[%d/%d] %s %sx%s buf %s rep %s   ' "$i" "$total" "$v" "$p" "$c" "$n" "$r" >&2
done < <(shuf "$lista")
rm -f "$lista"
echo >&2

# ---- latencia ----
[ -f "$LAT" ] || echo "so,versao,rep,seq,latencia_ns" > "$LAT"
for v in A B; do
    "$BENCH" --version "$v" --producers 1 --consumers 1 --buffer 1 \
             --items "$ITENS" --rep 0 --os linux > /dev/null   # aquecimento
done
for r in $(seq 1 "$LAT_REPS"); do
    for v in $(printf 'A\nB\n' | shuf); do
        "$BENCH" --version "$v" --producers 1 --consumers 1 --buffer 1 \
                 --items "$ITENS" --rep "$r" --os linux --latency "$LAT" > /dev/null
    done
done
gzip -f "$LAT"

echo "pronto: $VAZAO e $LAT.gz" >&2
