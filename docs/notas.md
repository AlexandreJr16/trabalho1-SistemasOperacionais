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
