# Notas de apresentação — TP1 de SO

## Decisões da equipe

- **Q1 v1 é a leitura literal do enunciado.** Na v2 o enunciado diz "leitores somente têm acesso
  quando não há nenhum escritor ativo; não ocorre leitura suja". Então, na v1, o leitor acessa mesmo
  com escritor ativo e pode pegar a conta no meio de uma escrita. Por isso só os escritores se excluem
  entre si. A solução de Courtois 1 (leitores juntos, escritor exclusivo) foi descartada porque é
  *preferência de leitores*, o contrário de "sem preferência", e nela a leitura suja nunca apareceria.
- **Q1 v2 é a solução clássica de Courtois, Heymans e Parnas (1971), problema 2** (preferência de
  escritores), com os 5 semáforos originais.
- **Q2 v1 e v2 usam o mesmo protocolo clássico do buffer limitado** (`vagas`, `itens`, `mutexBuffer`).
  O mutex único já serve para vários consumidores. Um mutex por ponta faria a exibição do buffer ler
  posições que a outra ponta está gravando. A v1 só aceita 1 consumidor.
- **Mecanismo:** semáforo POSIX (`sem_t`) para toda a sincronização. O único `pthread_mutex` é o do
  log, que protege só a saída e os contadores usados para detectar os problemas.
- **Versão sem controle** é o mesmo código das threads sem as chamadas de entrada e saída. O erro sai
  da falta de sincronização, não de código diferente.
- **Entrada por argumentos**, todos com valor padrão. Os valores dos escritores e os atrasos são
  sorteados a partir da semente, que é impressa no cabeçalho; com a mesma semente, os sorteios se
  repetem.
- **Um atraso por tipo de thread** (`-L`/`-E` na Q1, `-P`/`-C` na Q2). Ele vale fora da região
  crítica e em cada etapa dentro dela, sorteado entre 50% e 150% do valor dado.

## Como ler o log

- Toda linha começa com `[tempo ms]`, contado do início do programa. As linhas nunca se misturam,
  porque só uma thread escreve no log por vez.
- `BLOQUEADO: motivo` aparece quando a thread vai dormir num semáforo; `DESBLOQUEADO`, quando acorda.
  O `PLog` tenta antes com `sem_trywait`: se o semáforo estava livre, não há bloqueio e nada é
  registrado.
- Marcas com `<<<` são problemas que o próprio programa detectou no momento em que aconteceram. O
  resumo no fim conta todos eles e compara o resultado obtido com o esperado.
- Pergunta provável: *"o mutex do log não esconde o problema da v3?"* Não. Ele só protege a impressão
  e os contadores; o dado compartilhado (a conta, o buffer) nunca é alterado com o log travado, e
  nenhuma thread segura o log enquanto dorme ou espera um semáforo.

## Q1 v3 — sem controle de concorrência (`bin/le_v3`)

- **Semáforos:** nenhum. As threads são as das v1 e v2 sem a entrada e a saída; ninguém bloqueia.
- **Etapas:** a escrita lê o saldo, dorme, grava o saldo, dorme e grava o movimentado. A leitura lê o
  saldo, dorme e lê o movimentado.
- **O log prova:** `<<< DOIS ESCRITORES` com o mesmo `saldo lido` faz o último apagar o movimento do
  outro (`SALDO ERRADO` no resumo). `<<< LEITURA SUJA` é saldo e movimentado de momentos diferentes.
- **Pergunta:** *"por que o problema só aparece com escritores?"* Leitor não grava; sem escritor, a
  conta não muda.

## Q1 v1 — sem preferência de ordem de acesso (`bin/le_v1`)

- **Semáforos:** `acessoEscrita` = 1, só para escritores. O leitor não tem protocolo e nunca espera.
- **Quem bloqueia:** o escritor, quando já há outro escritor na conta. Ninguém tem prioridade.
- **O log prova:** o saldo final bate com o esperado e "escritor com outro escritor" dá 0. "Leitor
  com escritor" > 0 e `<<< LEITURA SUJA` são o que o enunciado permite. O TSan só acusa
  `consultarSaldo` × `movimentar`, nunca dois escritores.
- **Pergunta:** *"se o leitor não espera, por que o saldo está certo?"* A leitura suja estraga só o
  que o leitor vê. Estragar a conta exige dois escritores juntos, e isso o `acessoEscrita` impede.

## Q1 v2 — escritores com preferência (`bin/le_v2`)

- **Semáforos (todos = 1):** `mutexLeitores` e `mutexEscritores` (protegem `leitoresAtivos` e
  `escritoresPendentes`); `catracaLeitores` (o PRIMEIRO escritor fecha, o ÚLTIMO reabre);
  `filaLeitores` (um leitor por vez na catraca); `acessoConta` (um escritor ou o grupo de leitores).
- **Quem bloqueia:** o leitor na catraca (`escritor esperando ou escrevendo`) ou na fila; o escritor
  em `acessoConta` (`conta em uso`) até os leitores que estão dentro saírem.
- **O log prova:** escritor que chega DEPOIS de um leitor bloqueado entra ANTES; resumo zerado.
- **Pergunta:** *"para que serve a `filaLeitores`?"* Com ela, o escritor que chega disputa a catraca
  com no máximo um leitor, e não com todos.

## Q2 — o buffer (`produtores_consumidores/buffer.c`)

- **Vetor circular:** o produtor grava em `posicaoEntrada` e o consumidor tira de `posicaoSaida`. As
  duas só avançam e voltam a 0, então sai na ordem em que entrou. A posição vazia vale 0 (`_`).
- **Item:** produtor × 100 + número (`203` é o 3º item do Produtor 2). Como é único, o resumo acha
  perdidos e duplicados. Os consumidores dividem o total produzido, então todos sabem quando parar.
- **Status:** `produzindo`, `inseriu ... | buffer [...]`, `retirou ...`, `consumindo`. O "dormindo" do
  enunciado é `BLOQUEADO: sem vaga, dormindo` (buffer cheio) e `sem item, dormindo` (vazio).

## Q2 v3 — sem controle de concorrência (`bin/pc_v3`)

- **Semáforos:** nenhum; ninguém dorme com o buffer cheio ou vazio. A inserção lê a posição, dorme,
  grava e avança. A retirada lê o item, dorme, esvazia e avança.
- **O log prova, na própria linha:** `SOBRESCREVEU` (dois produtores na mesma posição ou buffer
  cheio), `DUPLICADO` (dois consumidores com o mesmo item), `posicao estava VAZIA` (retirou do vazio)
  e `APAGOU` (esvaziou uma posição que já tinha item novo). O resumo conta perdidos e duplicados.
- **Pergunta:** *"todo item sobrescrito se perde?"* Não: se um consumidor já o tinha lido, ele sai,
  às vezes duplicado. Por isso a contagem real fica no resumo, item por item.

## Q2 v1 — vários produtores e um consumidor (`bin/pc_v1`)

- **Semáforos:** `vagas` = tamanho (posições livres), `itens` = 0 (itens prontos), `mutexBuffer` = 1.
- **Quem bloqueia:** o produtor em `vagas` (`sem vaga, dormindo`), o consumidor em `itens` (`sem item,
  dormindo`) e qualquer um em `mutexBuffer` (`buffer em uso`). Cada saída acorda o outro lado: o
  produtor faz `V(itens)` e o consumidor faz `V(vagas)`.
- **O log prova:** o consumidor dorme no início, os produtores dormem com o buffer cheio e a retirada
  segue a ordem de inserção. O resumo fica zerado, o buffer termina vazio e o TSan não acusa nada.
- **Pergunta:** *"e se pegar o `mutexBuffer` antes de `vagas`?"* Deadlock: o produtor dorme sem vaga
  segurando o mutex, e o consumidor nunca entra para abrir a vaga.

## Q2 v2 — vários produtores e vários consumidores (`bin/pc_v2`)

- **Semáforos:** os mesmos da v1, com os mesmos valores iniciais. O protocolo é idêntico.
- **Quem bloqueia:** como na v1, e agora consumidor também espera consumidor: dois acordam em `itens`
  e o segundo fica `BLOQUEADO: buffer em uso` até o primeiro sair.
- **O log prova:** com 3 consumidores, cada item é retirado por exatamente um deles. O resumo fica
  zerado e o TSan não acusa nada.
- **Pergunta:** *"qual a diferença da v1?"* O `mutexBuffer` já exclui qualquer par de threads,
  inclusive dois consumidores disputando a `posicaoSaida`. Sem ele, dá o `DUPLICADO` da v3.

## Perguntas extras

- *Q1 v2: "o leitor pode passar fome?"* Pode. Se sempre chegar escritor, a catraca não abre; no log da
  v2, os escritores se revezam enquanto os leitores esperam.
- *Q2 v1: "por que o consumidor usa mutex se é o único?"* Porque ele divide o vetor com os produtores,
  e a exibição do buffer lê todas as posições.
- *Q2 v1: "por que um produtor dormiu sem vaga com uma posição livre na tela?"* A última vaga já estava
  reservada (`P(vagas)`) por outro produtor que ainda ia gravar.

## Como rodar

```
make            # compila os 6 programas em bin/, sem warning
make demo       # roda cada versão com semente fixa e salva o log em docs/execucoes/
make tsan       # recompila com ThreadSanitizer e conta os avisos de corrida de cada programa
```

- **Q1:** `bin/le_vN [-l leitores] [-e escritores] [-n operacoes] [-L atraso_leitor_ms]
  [-E atraso_escritor_ms] [-s semente]`
- **Q2:** `bin/pc_vN [-p produtores] [-c consumidores] [-n itens_por_produtor] [-t tamanho_buffer]
  [-P atraso_produtor_ms] [-C atraso_consumidor_ms] [-s semente]`
- Sem argumentos, tudo usa o valor padrão. Sem `-s`, a semente vem do relógio e aparece no cabeçalho.
- O `make demo` usa a **mesma entrada nas três versões** de cada questão: na Q1,
  `-l 3 -e 3 -n 4 -L 200 -E 300 -s 4`; na Q2, `-p 3 -n 4 -t 3 -P 150 -C 200 -s 5`, com 1 consumidor
  na v1 e 3 na v2 e na v3. Com a mesma semente, os escritores sorteiam os mesmos valores, então o
  saldo esperado é igual nas três versões (R$ 1644).

## Resultado do `make demo` (o que cada log mostra)

| | Q1 v1 | Q1 v2 | Q1 v3 |
|---|---|---|---|
| Saldo final × esperado | 1644 = 1644 | 1644 = 1644 | **1560 ≠ 1644** |
| Escritor com outro escritor | 0 | 0 | **10** |
| Leitor com escritor | **14** (permitido) | 0 | **18** |
| Leituras sujas | **6** (permitido) | 0 | **9** |
| TSan | só leitor×escritor | 0 avisos | corridas |

| | Q2 v1 (1 consumidor) | Q2 v2 (3 consumidores) | Q2 v3 (3 consumidores) |
|---|---|---|---|
| Retirados uma vez só / produzidos | 12 / 12 | 12 / 12 | **1 / 12** |
| Perdidos / duplicados / vazias | 0 / 0 / 0 | 0 / 0 / 0 | **10 / 1 / 8** |
| Buffer no fim | vazio | vazio | sobrou item |
| TSan | 0 avisos | 0 avisos | corridas |

Os tempos dos logs podem variar alguns milissegundos entre execuções, porque o escalonador não é
determinístico. Os logs de `docs/execucoes/` são os que estão no relatório; não editar.
