# Projeto de SO — Tema 2: Variável de Condição e Semáforo

GBC045 — Sistemas Operacionais — UFU/FACOM — 2026/2

## Integrantes

| Nome | Matrícula |
|---|---|
| _(preencher)_ | _(preencher)_ |
| _(preencher)_ | _(preencher)_ |
| _(preencher)_ | _(preencher)_ |

## O que tem aqui

Uma camada de threads (`platform_thread.h/.c`) com uma API única que por baixo usa
pthreads + `sem_t` no Linux e a API Win32 no Windows (`CreateThread`, `CRITICAL_SECTION`,
`CONDITION_VARIABLE`, `CreateSemaphore`). Não usamos winpthreads.

Em cima dela, um buffer circular limitado em duas versões:

- **A** (`bbuffer_cond.c`): 1 mutex + 2 variáveis de condição (`not_full`, `not_empty`).
- **B** (`bbuffer_sem.c`): 2 semáforos contadores (`slots`, `items`) + 1 mutex só para os índices.

```
code/          camada de threads, buffer A/B, bench, Makefile, build_windows.bat
testes/        test_platform.c (cada operação da camada), test_bbuffer.c (nenhum item perdido/duplicado)
dados/         CSVs brutos (linux/, windows/) + ambiente.txt de cada máquina
experimentos/  scripts de execução, analisar.py, resumos por SO e graficos/
relatorio/     relatório técnico (Relatorio_SO.pdf)
video/         vídeo da apresentação (Apresentacao_SO_Tema2.mp4)
```

## Como reproduzir

### Linux (nativo)

```sh
cd code
make test                       # compila e roda os testes
cd ../experimentos
./run_linux.sh                  # gera dados/linux/vazao.csv e latencia.csv.gz
```

### Windows (nativo, MSVC)

Precisa do Visual Studio Build Tools com "Desenvolvimento para desktop com C++". Pode ser
qualquer terminal (PowerShell ou cmd): o script acha o MSVC sozinho.

```bat
cd code
.\build_windows.bat test
cd ..\experimentos
powershell -ExecutionPolicy Bypass -File .\run_windows.ps1
```

O `run_windows.ps1` também compila sozinho se o `bench.exe` ainda não existir.

### Análise

Precisa de Python 3 com `pandas` e `matplotlib`:

```sh
python experimentos/analisar.py
```

Gera `experimentos/resumo_<so>.csv`, `experimentos/resumo_latencia_<so>.csv` e, em
`experimentos/graficos/`, `vazao_<so>.png`, `latencia_<so>.png` e `latencia_comparacao.png`
(esta só quando há dados dos dois SOs). Como cada arquivo leva o nome do SO, rodar a análise
numa máquina com os dados de um SO só não apaga os do outro. Os gráficos são gerados só a partir dos CSVs brutos, que nunca
são editados à mão.

### Rodando o bench na mão

```
bench --version A|B --producers P --consumers C --buffer N --items M --rep R --os linux|windows [--latency arq.csv]
```

Saída (uma linha):

```
so,versao,produtores,consumidores,buffer,itens,rep,tempo_s,vazao_itens_s
```

## Metodologia

- Total fixo de **400000 itens** em todas as configurações (divisível por 1 e por 4).
- Configurações: versão {A, B} × P×C {1×1, 4×4} × buffer {1, 16, 256} = 12.
- **30 repetições** por configuração, em ordem embaralhada, com 1 execução de aquecimento
  descartada por configuração.
- O tempo é medido de antes de criar as threads até depois do último `join`.
- **Término:** cada produtor gera exatamente `M/P` itens e cada consumidor retira `M/C`.
  Como o total produzido é igual ao consumido, nenhuma thread fica bloqueada no fim.
- **Latência:** com P=1, C=1, buffer 1, o produtor grava o instante no item e o consumidor
  calcula a diferença. Os valores vão para um vetor pré-alocado e só são gravados no disco
  depois da medição. Cada execução gera 400000 amostras; rodamos 5 execuções por versão e
  o arquivo é salvo compactado (`latencia.csv.gz`) por causa do tamanho.
- Compilação com `-O2` (gcc) e `/O2` (MSVC).
