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

- **Semáforos:** nenhum. As threads são as mesmas das v1 e v2, sem as chamadas de entrada e saída.
- **Quem bloqueia:** ninguém. Todo mundo entra na conta na hora em que quer.
- **A escrita é feita em etapas:** lê o saldo, dorme, grava o saldo novo, dorme, grava o
  movimentado. A leitura também: lê o saldo, dorme, lê o movimentado.
- **O que o log prova:**
  - `<<< DOIS ESCRITORES`: dois escritores estão na conta ao mesmo tempo. Quando os dois leem o
    mesmo `saldo lido`, o último a gravar apaga o movimento do outro. É a atualização perdida, e o
    resumo mostra `SALDO ERRADO` (final ≠ inicial + o que os escritores aplicaram).
  - `<<< LEITURA SUJA`: o leitor viu o saldo e o movimentado de momentos diferentes.
- **Pergunta provável:** *"por que o problema só aparece com escritores?"* Leitor com leitor nunca
  estraga nada, porque ninguém grava. Sem escritor, a conta não muda.

## Q1 v1 — sem preferência de ordem de acesso (`bin/le_v1`)

- **Semáforos:** `acessoEscrita` = 1. Só os escritores passam por ele.
- **Quem bloqueia:** só o escritor, em `acessoEscrita`, quando outro escritor está na conta
  (`BLOQUEADO: outro escritor na conta`). O leitor não tem protocolo e nunca espera.
- **Sem preferência:** ninguém tem prioridade. Os escritores fazem fila no semáforo, e o leitor
  entra a qualquer momento.
- **O que o log prova:**
  - O saldo final bate com o esperado, e "escritor com outro escritor" dá 0: os escritores se
    excluem.
  - "Leitor e escritor ao mesmo tempo" > 0 e `<<< LEITURA SUJA`: o leitor pegou a conta no meio de
    uma escrita, que é exatamente o que o enunciado diz que pode ocorrer.
  - O TSan acusa corrida só entre `consultarSaldo` e `movimentar`, nunca entre dois escritores.
- **Pergunta provável:** *"se o leitor não espera, por que o saldo final está certo?"* A leitura suja
  não estraga a conta, só o que o leitor vê. Quem poderia estragar são dois escritores juntos, e
  isso o `acessoEscrita` impede.

## Q1 v2 — escritores com preferência (`bin/le_v2`)

- **Semáforos (todos começam em 1):**
  - `mutexLeitores` protege `leitoresAtivos`; `mutexEscritores` protege `escritoresPendentes`
    (escritores esperando ou escrevendo).
  - `catracaLeitores`: o PRIMEIRO escritor que chega fecha, o ÚLTIMO a sair reabre.
  - `filaLeitores`: só um leitor por vez espera na catraca.
  - `acessoConta`: o escritor usa sozinho; os leitores ocupam como grupo (o primeiro tranca, o último
    libera).
- **Quem bloqueia:**
  - O leitor fica em `catracaLeitores` ("escritor esperando ou escrevendo") ou em `filaLeitores`
    ("outro leitor esperando na catraca").
  - O escritor fica em `acessoConta` ("conta em uso") enquanto os leitores que já estavam dentro
    terminam.
- **O que o log prova:**
  - Um escritor chega com leitores lendo e fica `BLOQUEADO: conta em uso`.
  - O leitor que chega depois fica `BLOQUEADO: escritor esperando`, mesmo com a conta ainda ocupada
    por leitores, e só entra depois que **todos** os escritores saem.
  - No resumo, todos os contadores ficam em 0, e o TSan não acusa nada.
- **Perguntas prováveis:**
  - *"Para que serve a `filaLeitores`?"* Sem ela, vários leitores esperariam na catraca, e o
    escritor que chega teria que disputar com todos eles. Com ela, disputa com no máximo um.
  - *"O leitor pode passar fome?"* Pode. Se sempre houver escritor chegando, a catraca nunca abre: é
    o preço da preferência, e o log mostra os escritores se revezando enquanto os leitores esperam.
