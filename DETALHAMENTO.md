# Detalhamento Técnico dos Mecanismos de Sincronização

Este documento detalha os mecanismos de sincronização implementados no Processador de Imagens Paralelo, explicando como o padrão Produtor/Consumidor e o padrão Future são utilizados para garantir o processamento correto e eficiente das imagens.

## 1. Mutex (Mutual Exclusion)

O mutex é utilizado para garantir que apenas uma thread por vez possa acessar recursos compartilhados. No projeto, implementamos mutex em vários pontos críticos:

```c
// Mutex para proteger a fila
pthread_mutex_t mutex;

// Mutex para métricas
pthread_mutex_t mutex_metricas = PTHREAD_MUTEX_INITIALIZER;

// Mutex para ordem de finalização
pthread_mutex_t mutex_ordem = PTHREAD_MUTEX_INITIALIZER;
```

### Uso do Mutex na Fila

Na função `inserir_imagem_na_fila`, o mutex é utilizado para proteger as operações críticas:

```c
pthread_mutex_lock(&fila->mutex);
// Operações críticas na fila
pthread_mutex_unlock(&fila->mutex);
```

Este mecanismo garante que:
- Apenas uma thread pode modificar a fila por vez
- As operações de inserção e remoção são atômicas
- Não há condições de corrida no acesso aos dados compartilhados

## 2. Semáforos

Os semáforos são utilizados para controlar o acesso à fila, garantindo que:
- Produtores não insiram em uma fila cheia
- Consumidores não removam de uma fila vazia

### Implementação dos Semáforos

```c
sem_t vazio;  // Controla slots vazios
sem_t cheio;  // Controla slots ocupados
```

### Inicialização

```c
sem_init(&fila->vazio, 0, capacidade);  // Inicializa com capacidade total
sem_init(&fila->cheio, 0, 0);           // Inicializa com 0 slots ocupados
```

### Uso nos Produtores

```c
sem_wait(&fila->vazio);  // Espera por slot vazio
// Insere imagem
sem_post(&fila->cheio);  // Sinaliza novo item
```

### Uso nos Consumidores

```c
sem_wait(&fila->cheio);  // Espera por item disponível
// Remove imagem
sem_post(&fila->vazio);  // Sinaliza slot livre
```

## 3. Padrão Produtor/Consumidor

O padrão Produtor/Consumidor é implementado através de uma fila thread-safe que coordena o trabalho entre threads produtoras e consumidoras.

### Estrutura da Fila

```c
typedef struct {
    Imagem* imagens;      // Array de imagens
    Future** futures;     // Array de futures
    int capacidade;       // Tamanho máximo
    int inicio;          // Índice do início
    int fim;             // Índice do fim
    int tamanho;         // Número atual de itens
} FilaImagens;
```

### Threads Produtoras

```c
void* produtor(void* arg) {
    // Lê diretório de entrada
    // Carrega imagens
    // Insere na fila
    // Atualiza métricas
}
```

### Threads Consumidoras

```c
void* consumidor(void* arg) {
    // Remove imagem da fila
    // Processa imagem
    // Salva resultado
    // Atualiza métricas
}
```

## 4. Padrão Future

O padrão Future é implementado para gerenciar resultados assíncronos, permitindo que as threads consumidoras processem as imagens de forma assíncrona enquanto as threads produtoras continuam carregando novas imagens.

### Estrutura do Future

```c
typedef struct {
    Imagem* resultado;
    int concluido;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} Future;
```

### Operações Principais

#### Criar Future
```c
Future* future = criar_future();
```

#### Definir Resultado
```c
void definir_resultado_future(Future* future, Imagem* resultado) {
    pthread_mutex_lock(&future->mutex);
    future->resultado = resultado;
    future->concluido = 1;
    pthread_cond_signal(&future->cond);
    pthread_mutex_unlock(&future->mutex);
}
```

#### Obter Resultado (Assíncrono)
```c
Imagem* obter_resultado_future(Future* future) {
    pthread_mutex_lock(&future->mutex);
    while (!future->concluido) {
        pthread_cond_wait(&future->cond, &future->mutex);
    }
    Imagem* resultado = future->resultado;
    pthread_mutex_unlock(&future->mutex);
    return resultado;
}
```

### Implementação Assíncrona com Monitor

Para demonstrar o uso assíncrono do Future, implementamos um monitor que verifica periodicamente o estado dos Futures:

```c
void* monitor(void* arg) {
    while (executando) {
        for (int i = 0; i < fila->capacidade; i++) {
            Future* future = fila->futures[i];
            if (future) {
                pthread_mutex_lock(&future->mutex);
                if (future->concluido) {
                    // Processamento assíncrono quando a imagem está pronta
                    printf("Monitor: Imagem processada na posição %d\n", i);
                }
                pthread_mutex_unlock(&future->mutex);
            }
        }
        usleep(100000); // Verifica a cada 100ms
    }
}
```

### Benefícios da Implementação Assíncrona

1. **Monitoramento em Tempo Real:**
   - O monitor verifica o estado dos Futures sem bloquear o processamento principal
   - Permite acompanhar o progresso do processamento das imagens
   - Facilita a implementação de callbacks e notificações

2. **Flexibilidade:**
   - O processamento principal continua síncrono para garantir a ordem
   - Operações adicionais podem ser realizadas de forma assíncrona
   - Fácil expansão para diferentes tipos de processamento pós-conclusão

3. **Escalabilidade:**
   - Múltiplos monitores podem ser adicionados para diferentes propósitos
   - Cada monitor pode implementar sua própria lógica de processamento
   - Sistema pode ser adaptado para diferentes necessidades sem alterar a lógica principal

> **Nota:** O Future é usado tanto como mecanismo de sincronização quanto para processamento assíncrono. A implementação do monitor demonstra como o Future pode ser utilizado para operações verdadeiramente assíncronas, permitindo que o sistema responda a eventos de processamento sem bloquear as threads principais.

## Benefícios da Implementação

1. **Concorrência Eficiente:**
   - Múltiplas imagens são processadas simultaneamente
   - Recursos do sistema são utilizados de forma otimizada

2. **Sincronização Robusta:**
   - Sem condições de corrida
   - Sem deadlocks
   - Sem starvation

3. **Escalabilidade:**
   - Fácil ajuste do número de produtores e consumidores
   - Adaptável a diferentes cargas de trabalho

4. **Monitoramento:**
   - Métricas detalhadas de desempenho
   - Rastreamento da ordem de finalização das threads 