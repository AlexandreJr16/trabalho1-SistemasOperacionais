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

## Q2 — o buffer (`produtores_consumidores/buffer.c`)

- **Como funciona:** vetor circular com `posicaoEntrada` (onde o produtor grava) e `posicaoSaida` (de
  onde o consumidor tira). As duas só andam para frente e voltam a 0 no fim do vetor, então a ordem
  de saída é a mesma de entrada. A posição vazia vale 0 e aparece como `_`.
- **Valor do item:** produtor × 100 + número do item (`203` é o 3º item do Produtor 2). Como cada
  valor é único, o resumo consegue contar itens perdidos e duplicados.
- **Divisão do trabalho:** os consumidores dividem o total produzido (cota). Assim toda thread sabe
  quando parar e termina sozinha.
- **Status no log:**
  - `produzindo`, `vai inserir`, `inseriu ... | buffer [...]`;
  - `vai retirar`, `retirou ... | buffer [...]`, `consumindo`;
  - `BLOQUEADO: sem vaga, dormindo` (buffer cheio) e `BLOQUEADO: sem item, dormindo` (buffer
    vazio), que são o "dormindo" do enunciado.

## Q2 v3 — sem controle de concorrência (`bin/pc_v3`)

- **Semáforos:** nenhum. As threads são as mesmas das v1 e v2, sem as chamadas de entrada e saída.
- **Quem bloqueia:** ninguém. Ninguém dorme com o buffer cheio ou vazio, nem espera outra thread
  sair do buffer.
- **A inserção é feita em etapas:** lê a posição de entrada, dorme, grava o item, avança. A retirada
  também: lê o item da posição de saída, dorme, esvazia a posição, avança.
- **O que o log prova (onde e quando, na própria linha):**
  - `SOBRESCREVEU`: dois produtores pegaram a mesma posição, ou o buffer estava cheio.
  - `DUPLICADO`: dois consumidores leram o mesmo item.
  - `posicao estava VAZIA`: o consumidor retirou com o buffer vazio.
  - `APAGOU`: o consumidor esvaziou a posição depois que outro item já tinha sido gravado nela.
  - O resumo conta os itens perdidos (nunca retirados) e os duplicados.
- **Pergunta provável:** *"todo item sobrescrito se perde?"* Não. Se um consumidor já tinha lido o
  item antes de ele ser sobrescrito, ele sai, e às vezes até duplicado. Por isso a contagem real de
  perdidos fica no resumo, que confere item por item.

## Q2 v1 — vários produtores e um consumidor (`bin/pc_v1`)

- **Semáforos:**
  - `vagas` = tamanho do buffer (posições livres);
  - `itens` = 0 (itens prontos para retirar);
  - `mutexBuffer` = 1 (uma thread por vez no vetor e nas posições).
- **Quem bloqueia:**
  - O produtor em `vagas` (`sem vaga, dormindo`, buffer cheio).
  - O consumidor em `itens` (`sem item, dormindo`, buffer vazio).
  - Qualquer um em `mutexBuffer` (`buffer em uso`).
  - A saída sempre acorda o outro lado: o produtor faz `V(itens)` e o consumidor faz `V(vagas)`.
- **O que o log prova:**
  - O consumidor dorme no início (buffer vazio) e os produtores dormem quando o buffer enche.
  - A retirada segue a ordem de inserção.
  - O resumo fica zerado, o buffer termina vazio e o TSan não acusa nada.
- **Perguntas prováveis:**
  - *"E se trocar a ordem e pegar o `mutexBuffer` antes de `vagas`?"* Deadlock: o produtor dorme
    sem vaga segurando o mutex, e o consumidor nunca entra para abrir a vaga.
  - *"Por que o consumidor usa mutex se é o único?"* Porque ele divide o vetor com os produtores e
    a exibição do buffer lê todas as posições.
  - *"Por que um produtor dormiu sem vaga com uma posição livre na tela?"* A última vaga já estava
    reservada (`P(vagas)`) por outro produtor que ainda ia gravar.

## Q2 v2 — vários produtores e vários consumidores (`bin/pc_v2`)

- **Semáforos:** os mesmos da v1, com os mesmos valores iniciais: `vagas` = tamanho, `itens` = 0,
  `mutexBuffer` = 1. O protocolo é idêntico.
- **Quem bloqueia:**
  - Como na v1, o produtor espera em `vagas` e o consumidor em `itens`.
  - Agora também consumidor espera consumidor: dois consumidores acordam em `itens` e o segundo fica
    `BLOQUEADO: buffer em uso` até o primeiro sair do buffer.
- **O que o log prova:** com 3 consumidores, cada item é retirado por exatamente um deles, sem
  duplicado nem posição vazia; o resumo fica zerado e o TSan não acusa nada.
- **Pergunta provável:** *"então qual a diferença da v1?"* O protocolo é o mesmo porque o
  `mutexBuffer` já exclui qualquer par de threads, inclusive dois consumidores. O que a v2 mostra é
  que ele resolve a disputa pela `posicaoSaida`. Sem ele, dois consumidores leem a mesma posição, que
  é o `DUPLICADO` da v3.

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
