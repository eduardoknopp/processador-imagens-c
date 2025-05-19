#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

// Definir CLOCK_MONOTONIC
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 0
#endif

// Incluir stb_image.h para carregamento de imagens
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Incluir stb_image_write.h para salvar imagens
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Declarações das estruturas
typedef struct {
    char nome[256];        // Nome do arquivo
    int largura;          // Largura da imagem
    int altura;           // Altura da imagem
    int canais;           // Número de canais
    unsigned char* dados; // Dados da imagem
    int produtor_id;      // ID do produtor que inseriu a imagem
} Imagem;

// Estrutura para Future
typedef struct {
    Imagem* resultado;
    int concluido;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} Future;

typedef struct {
    Imagem* imagens;      // Array de imagens
    Future** futures;     // Array de futures
    int capacidade;       // Tamanho máximo da fila
    int inicio;          // Índice do início da fila
    int fim;             // Índice do fim da fila
    int tamanho;         // Número atual de imagens na fila
    
    pthread_mutex_t mutex;    // Mutex para proteger a fila
    sem_t vazio;             // Semáforo para controlar slots vazios
    sem_t cheio;             // Semáforo para controlar slots ocupados
} FilaImagens;

typedef struct {
    FilaImagens* fila;
    char* diretorio_entrada;
    char* diretorio_saida;
    int thread_id;
} ThreadArgs;

// Adicionar após as declarações globais
#define NUM_PRODUTORES 5
#define NUM_CONSUMIDORES 5
#define NUM_MONITORES 1

// Estrutura para métricas
typedef struct {
    int imagens_processadas;
    double tempo_total;
    int ordem_finalizacao;  // Nova variável para rastrear a ordem de finalização
} Metricas;

// Variáveis globais para métricas
Metricas metricas_produtores[NUM_PRODUTORES];
Metricas metricas_consumidores[NUM_CONSUMIDORES];
pthread_mutex_t mutex_metricas = PTHREAD_MUTEX_INITIALIZER;

// Variáveis para rastrear a ordem de finalização
int ordem_finalizacao_produtores = 0;
int ordem_finalizacao_consumidores = 0;
pthread_mutex_t mutex_ordem = PTHREAD_MUTEX_INITIALIZER;

// Variáveis globais para controle
int executando = 1;  // Flag para controlar a execução das threads

// Estrutura para argumentos do monitor
typedef struct {
    FilaImagens* fila;
    int thread_id;
} MonitorArgs;

// Função do monitor
void* monitor(void* arg) {
    MonitorArgs* args = (MonitorArgs*)arg;
    printf("\033[1;36m[MONITOR %d] Iniciado\033[0m\n", args->thread_id);
    
    // Array para rastrear o estado anterior de cada posição
    int* estado_anterior = (int*)calloc(args->fila->capacidade, sizeof(int));
    
    while (executando) {
        // Verifica todas as posições da fila
        pthread_mutex_lock(&args->fila->mutex);
        printf("\033[1;36m[MONITOR %d] Estado atual da fila:\033[0m\n", args->thread_id);
        printf("\033[1;36m[MONITOR %d] - Tamanho: %d/%d\033[0m\n", 
               args->thread_id, args->fila->tamanho, args->fila->capacidade);
        
        for (int i = 0; i < args->fila->capacidade; i++) {
            Future* future = args->fila->futures[i];
            if (future) {
                pthread_mutex_lock(&future->mutex);
                // Extrai apenas o nome do arquivo do caminho completo
                const char* nome_arquivo = strrchr(args->fila->imagens[i].nome, '/');
                if (nome_arquivo) {
                    nome_arquivo++; // Pula a barra
                } else {
                    nome_arquivo = args->fila->imagens[i].nome;
                }
                
                int estado_atual = future->concluido;
                
                // Se o estado mudou de não processado para processado
                if (estado_atual && !estado_anterior[i]) {
                    printf("\033[1;32m[MONITOR %d] ✓ Imagem processada: %s [Produtor: %d]\033[0m\n",
                           args->thread_id, nome_arquivo, args->fila->imagens[i].produtor_id);
                }
                
                printf("\033[1;36m[MONITOR %d] Posição %d: %s (%s) [Produtor: %d]\033[0m\n", 
                       args->thread_id, 
                       i, 
                       estado_atual ? "Processada" : "Em processamento",
                       nome_arquivo,
                       args->fila->imagens[i].produtor_id);
                
                estado_anterior[i] = estado_atual;
                pthread_mutex_unlock(&future->mutex);
            } else {
                printf("\033[1;36m[MONITOR %d] Posição %d: Vazia\033[0m\n", 
                       args->thread_id, i);
                estado_anterior[i] = 0;
            }
        }
        pthread_mutex_unlock(&args->fila->mutex);
        
        printf("\033[1;36m[MONITOR %d] Aguardando processamento...\033[0m\n", args->thread_id);
        usleep(100000); // Dorme por 100ms
    }
    
    free(estado_anterior);
    printf("\033[1;36m[MONITOR %d] Finalizado\033[0m\n", args->thread_id);
    return NULL;
}

// Declarações das funções
FilaImagens* criar_fila(int capacidade);
void destruir_fila(FilaImagens* fila);
int inserir_imagem_na_fila(FilaImagens* fila, Imagem* img);
int remover_imagem_da_fila(FilaImagens* fila, Imagem* img);
void liberar_imagem_da_memoria(Imagem* img);
void* produtor(void* arg);
void* consumidor(void* arg);

/**
 * @brief Cria um novo Future para acompanhar o processamento de uma imagem
 * @return Ponteiro para o Future criado, ou NULL em caso de erro
 * 
 * A função inicializa um Future com:
 * - Mutex para sincronização
 * - Variável de condição para espera
 * - Flag de conclusão
 * - Ponteiro para o resultado
 * 
 * É responsabilidade do chamador destruir o Future usando destruir_future().
 */
Future* criar_future() {
    Future* future = (Future*)malloc(sizeof(Future));
    if (!future) {
        perror("Erro ao alocar Future");
        return NULL;
    }
    
    future->resultado = NULL;
    future->concluido = 0;
    pthread_mutex_init(&future->mutex, NULL);
    pthread_cond_init(&future->cond, NULL);
    
    return future;
}

/**
 * @brief Destrói um Future e libera seus recursos
 * @param future Ponteiro para o Future a ser destruído
 * 
 * Libera todos os recursos associados ao Future, incluindo:
 * - Mutex
 * - Variável de condição
 * - Estrutura do Future
 */
void destruir_future(Future* future) {
    if (!future) return;
    
    pthread_mutex_destroy(&future->mutex);
    pthread_cond_destroy(&future->cond);
    free(future);
}

/**
 * @brief Define o resultado de um Future
 * @param future Ponteiro para o Future
 * @param resultado Ponteiro para a imagem processada
 * 
 * A função:
 * 1. Trava o mutex do Future
 * 2. Define o resultado
 * 3. Marca o Future como concluído
 * 4. Sinaliza a variável de condição
 * 5. Libera o mutex
 */
void definir_resultado_future(Future* future, Imagem* resultado) {
    if (!future) return;
    
    pthread_mutex_lock(&future->mutex);
    future->resultado = resultado;
    future->concluido = 1;
    pthread_cond_signal(&future->cond);
    pthread_mutex_unlock(&future->mutex);
}

/**
 * @brief Obtém o resultado de um Future (bloqueante)
 * @param future Ponteiro para o Future
 * @return Ponteiro para a imagem processada, ou NULL em caso de erro
 * 
 * A função bloqueia até que o Future seja concluído.
 * Quando o resultado estiver disponível, retorna a imagem processada.
 * Não libera a memória da imagem - isso é responsabilidade do chamador.
 */
Imagem* obter_resultado_future(Future* future) {
    if (!future) return NULL;
    
    pthread_mutex_lock(&future->mutex);
    while (!future->concluido) {
        pthread_cond_wait(&future->cond, &future->mutex);
    }
    Imagem* resultado = future->resultado;
    pthread_mutex_unlock(&future->mutex);
    
    return resultado;
}

/**
 * @brief Carrega uma imagem do disco para a memória
 * @param caminho Caminho do arquivo de imagem a ser carregado
 * @param produtor_id ID do produtor que está carregando a imagem (para logs)
 * @return Ponteiro para a estrutura Imagem carregada, ou NULL em caso de erro
 * 
 * Esta função utiliza a biblioteca stb_image para carregar imagens em vários formatos
 * (PNG, JPG, BMP, etc). A imagem é carregada em memória e suas dimensões e número
 * de canais são detectados automaticamente.
 * 
 * A função aloca memória para a estrutura Imagem e seus dados. É responsabilidade
 * do chamador liberar esta memória usando liberar_imagem_da_memoria().
 */
Imagem* carregar_imagem_do_disco(const char* caminho, int produtor_id) {
    Imagem* img = (Imagem*)malloc(sizeof(Imagem));
    if (!img) {
        perror("Erro ao alocar estrutura de imagem");
        return NULL;
    }

    // Copia o nome do arquivo
    strncpy(img->nome, caminho, sizeof(img->nome) - 1);
    img->nome[sizeof(img->nome) - 1] = '\0';

    // Carrega a imagem usando stb_image, forçando 3 canais (RGB)
    img->dados = stbi_load(caminho, &img->largura, &img->altura, &img->canais, 3);
    
    if (!img->dados) {
        printf("Erro ao carregar imagem %s: %s\n", caminho, stbi_failure_reason());
        free(img);
        return NULL;
    }

    // Força 3 canais
    img->canais = 3;

    printf("Produtor %d: Carregou imagem %s (%dx%d, %d canais)\n", 
           produtor_id, caminho, img->largura, img->altura, img->canais);

    return img;
}

/**
 * @brief Libera a memória alocada para uma imagem
 * @param img Ponteiro para a estrutura Imagem a ser liberada
 * 
 * Esta função libera tanto a estrutura Imagem quanto os dados da imagem em si.
 * É segura para chamar com NULL.
 */
void liberar_imagem_da_memoria(Imagem* img) {
    if (img) {
        if (img->dados) {
            stbi_image_free(img->dados);
        }
        free(img);
    }
}

/**
 * @brief Salva uma imagem processada no disco
 * @param img Ponteiro para a estrutura Imagem a ser salva
 * @param diretorio_saida Diretório onde a imagem será salva
 * @param consumidor_id ID do consumidor que processou a imagem
 * 
 * A função detecta automaticamente o formato original da imagem e salva no mesmo formato.
 * Se o formato não for reconhecido, salva como PNG.
 * Utiliza a biblioteca stb_image_write para salvar a imagem.
 * O nome do arquivo de saída inclui o ID do consumidor para evitar conflitos.
 */
void salvar_imagem_no_disco(Imagem* img, const char* diretorio_saida, int consumidor_id) {
    if (!img || !img->dados) {
        printf("Erro: Imagem inválida para salvar.\n");
        return;
    }

    // Garante que o diretório de saída existe
    struct stat st = {0};
    if (stat(diretorio_saida, &st) == -1) {
        mkdir(diretorio_saida, 0700);
    }

    // Extrair apenas o nome do arquivo do caminho completo
    const char* nome_arquivo = strrchr(img->nome, '/');
    if (nome_arquivo) {
        nome_arquivo++; // Pula a barra
    } else {
        nome_arquivo = img->nome;
    }

    // Construir o caminho de saída com o ID do consumidor
    char caminho_saida[512];
    char nome_base[256];
    char extensao[32] = "";
    
    // Separar nome base e extensão
    const char* ponto = strrchr(nome_arquivo, '.');
    if (ponto) {
        int pos = ponto - nome_arquivo;
        strncpy(nome_base, nome_arquivo, pos);
        nome_base[pos] = '\0';
        strcpy(extensao, ponto);
    } else {
        strcpy(nome_base, nome_arquivo);
    }
    
    // Construir novo nome com ID do consumidor
    snprintf(caminho_saida, sizeof(caminho_saida), "%s/cons-%d-%s%s", 
             diretorio_saida, consumidor_id, nome_base, extensao);

    printf("Tentando salvar imagem em: %s\n", caminho_saida);
    printf("Dimensões: %dx%d, Canais: %d\n", img->largura, img->altura, img->canais);

    // Detectar extensão do arquivo original
    const char* extensao_original = strrchr(nome_arquivo, '.');
    int sucesso = 0;
    if (extensao_original) {
        if (strcasecmp(extensao_original, ".png") == 0) {
            sucesso = stbi_write_png(caminho_saida, img->largura, img->altura, img->canais, img->dados, img->largura * img->canais);
        } else if (strcasecmp(extensao_original, ".jpg") == 0 || strcasecmp(extensao_original, ".jpeg") == 0) {
            sucesso = stbi_write_jpg(caminho_saida, img->largura, img->altura, img->canais, img->dados, 90); // Qualidade 90
        } else if (strcasecmp(extensao_original, ".bmp") == 0) {
            sucesso = stbi_write_bmp(caminho_saida, img->largura, img->altura, img->canais, img->dados);
        } else if (strcasecmp(extensao_original, ".tga") == 0) {
            sucesso = stbi_write_tga(caminho_saida, img->largura, img->altura, img->canais, img->dados);
        } else {
            // Se extensão não reconhecida, salva como PNG
            sucesso = stbi_write_png(caminho_saida, img->largura, img->altura, img->canais, img->dados, img->largura * img->canais);
        }
    } else {
        // Se não tem extensão, salva como PNG
        sucesso = stbi_write_png(caminho_saida, img->largura, img->altura, img->canais, img->dados, img->largura * img->canais);
    }

    if (!sucesso) {
        printf("Erro ao salvar imagem: %s\n", caminho_saida);
    } else {
        printf("Imagem salva com sucesso: %s\n", caminho_saida);
    }
}

/**
 * @brief Converte uma imagem colorida para escala de cinza
 * @param img Ponteiro para a estrutura Imagem a ser convertida
 * 
 * A função aplica a fórmula padrão de conversão para escala de cinza:
 * cinza = 0.299R + 0.587G + 0.114B
 * 
 * A conversão é feita in-place, modificando os dados originais da imagem.
 * Requer que a imagem tenha pelo menos 3 canais (RGB).
 */
void converter_para_cinza(Imagem* img) {
    if (!img || !img->dados || img->canais < 3) return;
    
    for (int i = 0; i < img->largura * img->altura; i++) {
        unsigned char* pixel = &img->dados[i * img->canais];
        unsigned char cinza = (unsigned char)(0.299f * pixel[0] + 0.587f * pixel[1] + 0.114f * pixel[2]);
        pixel[0] = pixel[1] = pixel[2] = cinza;
    }
}

/**
 * @brief Inverte as cores de uma imagem
 * @param img Ponteiro para a estrutura Imagem a ser invertida
 * 
 * A função inverte cada canal da imagem subtraindo seu valor de 255.
 * A inversão é feita in-place, modificando os dados originais da imagem.
 */
void inverter_cores(Imagem* img) {
    if (!img || !img->dados) return;
    
    for (int i = 0; i < img->largura * img->altura * img->canais; i++) {
        img->dados[i] = 255 - img->dados[i];
    }
}

/**
 * @brief Ajusta o brilho de uma imagem
 * @param img Ponteiro para a estrutura Imagem a ser ajustada
 * @param fator Fator de multiplicação do brilho (1.0 = brilho original)
 * 
 * A função multiplica cada canal da imagem pelo fator especificado.
 * Valores são limitados ao intervalo [0, 255].
 * O ajuste é feito in-place, modificando os dados originais da imagem.
 */
void ajustar_brilho(Imagem* img, float fator) {
    if (!img || !img->dados) return;
    
    for (int i = 0; i < img->largura * img->altura * img->canais; i++) {
        int novo_valor = (int)(img->dados[i] * fator);
        img->dados[i] = (unsigned char)(novo_valor > 255 ? 255 : (novo_valor < 0 ? 0 : novo_valor));
    }
}

/**
 * @brief Ajusta o contraste de uma imagem
 * @param img Ponteiro para a estrutura Imagem a ser ajustada
 * @param fator Fator de multiplicação do contraste (1.0 = contraste original)
 * 
 * A função ajusta o contraste subtraindo 128 de cada valor,
 * multiplicando pelo fator e adicionando 128 novamente.
 * Valores são limitados ao intervalo [0, 255].
 * O ajuste é feito in-place, modificando os dados originais da imagem.
 */
void ajustar_contraste(Imagem* img, float fator) {
    if (!img || !img->dados) return;
    
    for (int i = 0; i < img->largura * img->altura * img->canais; i++) {
        int novo_valor = (int)((img->dados[i] - 128) * fator + 128);
        img->dados[i] = (unsigned char)(novo_valor > 255 ? 255 : (novo_valor < 0 ? 0 : novo_valor));
    }
}

/**
 * @brief Atualiza as métricas de desempenho de uma thread
 * @param thread_id ID da thread
 * @param tipo_thread 0 para produtor, 1 para consumidor
 * @param tempo_processamento Tempo gasto no processamento atual
 * 
 * Atualiza o número de imagens processadas e o tempo total
 * de processamento para a thread especificada.
 */
void atualizar_metricas(int thread_id, int tipo_thread, double tempo_processamento) {
    pthread_mutex_lock(&mutex_metricas);
    if (tipo_thread == 0) { // Produtor
        metricas_produtores[thread_id].imagens_processadas++;
        metricas_produtores[thread_id].tempo_total += tempo_processamento;
    } else { // Consumidor
        metricas_consumidores[thread_id].imagens_processadas++;
        metricas_consumidores[thread_id].tempo_total += tempo_processamento;
    }
    pthread_mutex_unlock(&mutex_metricas);
}

/**
 * @brief Registra a ordem de finalização de uma thread
 * @param thread_id ID da thread
 * @param tipo_thread 0 para produtor, 1 para consumidor
 * 
 * Atualiza a ordem de finalização da thread no array de métricas.
 * A ordem é mantida separadamente para produtores e consumidores.
 */
void registrar_finalizacao(int thread_id, int tipo_thread) {
    pthread_mutex_lock(&mutex_ordem);
    if (tipo_thread == 0) { // Produtor
        metricas_produtores[thread_id].ordem_finalizacao = ++ordem_finalizacao_produtores;
    } else { // Consumidor
        metricas_consumidores[thread_id].ordem_finalizacao = ++ordem_finalizacao_consumidores;
    }
    pthread_mutex_unlock(&mutex_ordem);
}

/**
 * @brief Função executada por cada thread produtora
 * @param arg Argumentos da thread (ThreadArgs*)
 * @return NULL
 * 
 * A thread produtora:
 * 1. Lê arquivos do diretório de entrada
 * 2. Carrega cada imagem encontrada
 * 3. Insere a imagem na fila
 * 4. Registra métricas de desempenho
 * 5. Registra sua ordem de finalização
 */
void* produtor(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    DIR* dir;
    struct dirent* ent;
    struct timespec inicio, fim;
    
    printf("Produtor %d iniciado\n", args->thread_id);
    
    dir = opendir(args->diretorio_entrada);
    if (!dir) {
        perror("Erro ao abrir diretório");
        return NULL;
    }
    
    while (executando && (ent = readdir(dir)) != NULL) {
        char caminho[512];
        snprintf(caminho, sizeof(caminho), "%s/%s", args->diretorio_entrada, ent->d_name);

        struct stat st;
        if (stat(caminho, &st) == 0 && S_ISREG(st.st_mode)) {
            clock_gettime(CLOCK_MONOTONIC, &inicio);
            
            Imagem* img = carregar_imagem_do_disco(caminho, args->thread_id);
            if (img) {
                img->produtor_id = args->thread_id;  // Define o ID do produtor
                printf("Produtor %d: inserindo imagem %s na fila\n", 
                       args->thread_id, ent->d_name);
                
                if (inserir_imagem_na_fila(args->fila, img)) {
                    printf("Produtor %d: Imagem %s inserida na fila\n", 
                           args->thread_id, ent->d_name);
                }
                
                liberar_imagem_da_memoria(img);
            }
            
            clock_gettime(CLOCK_MONOTONIC, &fim);
            double tempo = (fim.tv_sec - inicio.tv_sec) + 
                          (fim.tv_nsec - inicio.tv_nsec) / 1e9;
            atualizar_metricas(args->thread_id, 0, tempo);
        }
    }
    
    closedir(dir);
    registrar_finalizacao(args->thread_id, 0);
    printf("Produtor %d finalizado\n", args->thread_id);
    return NULL;
}

/**
 * @brief Função executada por cada thread consumidora
 * @param arg Argumentos da thread (ThreadArgs*)
 * @return NULL
 * 
 * A thread consumidora:
 * 1. Remove imagens da fila
 * 2. Aplica transformações (cinza, inversão, brilho, contraste)
 * 3. Salva a imagem processada
 * 4. Registra métricas de desempenho
 * 5. Registra sua ordem de finalização
 */
void* consumidor(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    struct timespec inicio, fim;
    
    printf("Consumidor %d iniciado\n", args->thread_id);
    
    while (1) {
        Imagem img;
        Future* future = NULL;
        
        // Tenta remover imagem da fila com timeout
        struct timespec timeout;
        clock_gettime(CLOCK_MONOTONIC, &timeout);
        timeout.tv_sec += 2; // 2 segundos de timeout
        
        int sem_result = sem_timedwait(&args->fila->cheio, &timeout);
        if (sem_result == 0) {
            pthread_mutex_lock(&args->fila->mutex);
            if (args->fila->tamanho > 0) {
                // Obtém o future da imagem
                future = args->fila->futures[args->fila->inicio];

                // Copia a imagem da fila
                strncpy(img.nome, args->fila->imagens[args->fila->inicio].nome, sizeof(img.nome) - 1);
                img.largura = args->fila->imagens[args->fila->inicio].largura;
                img.altura = args->fila->imagens[args->fila->inicio].altura;
                img.canais = args->fila->imagens[args->fila->inicio].canais;
                img.produtor_id = args->fila->imagens[args->fila->inicio].produtor_id;
                
                // Aloca e copia os dados da imagem
                size_t tamanho_dados = img.largura * img.altura * img.canais;
                img.dados = (unsigned char*)malloc(tamanho_dados);
                if (img.dados) {
                    memcpy(img.dados, args->fila->imagens[args->fila->inicio].dados, tamanho_dados);
                }

                // Libera os dados da imagem na fila
                free(args->fila->imagens[args->fila->inicio].dados);
                args->fila->imagens[args->fila->inicio].dados = NULL;

                // Limpa o future
                args->fila->futures[args->fila->inicio] = NULL;

                // Atualiza índices da fila
                args->fila->inicio = (args->fila->inicio + 1) % args->fila->capacidade;
                args->fila->tamanho--;

                pthread_mutex_unlock(&args->fila->mutex);
                sem_post(&args->fila->vazio);

                clock_gettime(CLOCK_MONOTONIC, &inicio);
                
                printf("Consumidor %d: Processando imagem %s\n", 
                       args->thread_id, img.nome);
                
                // Processa a imagem
                converter_para_cinza(&img);
                inverter_cores(&img);
                ajustar_brilho(&img, 1.2f);
                ajustar_contraste(&img, 1.3f);
                
                // Salva a imagem processada
                salvar_imagem_no_disco(&img, args->diretorio_saida, args->thread_id);
                
                // Define o resultado no future
                if (future) {
                    printf("Consumidor %d: Definindo resultado no Future para imagem %s\n",
                           args->thread_id, img.nome);
                    definir_resultado_future(future, &img);
                }
                
                free(img.dados);
                
                clock_gettime(CLOCK_MONOTONIC, &fim);
                double tempo = (fim.tv_sec - inicio.tv_sec) + 
                              (fim.tv_nsec - inicio.tv_nsec) / 1e9;
                atualizar_metricas(args->thread_id, 1, tempo);
            } else {
                pthread_mutex_unlock(&args->fila->mutex);
                // Não faz break aqui, apenas continua tentando
                usleep(10000); // Pequeno delay para evitar busy-wait
                continue;
            }
        } else {
            // Timeout ocorreu
            pthread_mutex_lock(&args->fila->mutex);
            int fila_vazia = (args->fila->tamanho == 0);
            pthread_mutex_unlock(&args->fila->mutex);
            if (!executando && fila_vazia) {
                break; // Só sai se não estiver executando e a fila estiver vazia
            }
            // Caso contrário, continua tentando
            usleep(10000);
        }
    }
    
    registrar_finalizacao(args->thread_id, 1);
    printf("Consumidor %d finalizado\n", args->thread_id);
    return NULL;
}

/**
 * @brief Cria uma nova fila de imagens
 * @param capacidade Número máximo de imagens que a fila pode armazenar
 * @return Ponteiro para a fila criada, ou NULL em caso de erro
 * 
 * A função inicializa uma fila circular com semáforos para controle de acesso.
 * A fila é thread-safe e pode ser usada por múltiplos produtores e consumidores.
 * 
 * É responsabilidade do chamador destruir a fila usando destruir_fila().
 */
FilaImagens* criar_fila(int capacidade) {
    FilaImagens* fila = (FilaImagens*)malloc(sizeof(FilaImagens));
    if (!fila) {
        perror("Erro ao alocar fila");
        return NULL;
    }

    fila->imagens = (Imagem*)malloc(capacidade * sizeof(Imagem));
    if (!fila->imagens) {
        perror("Erro ao alocar array de imagens");
        free(fila);
        return NULL;
    }

    fila->futures = (Future**)malloc(capacidade * sizeof(Future*));
    if (!fila->futures) {
        perror("Erro ao alocar array de futures");
        free(fila->imagens);
        free(fila);
        return NULL;
    }

    fila->capacidade = capacidade;
    fila->inicio = 0;
    fila->fim = 0;
    fila->tamanho = 0;

    // Inicializa mutex e semáforos
    pthread_mutex_init(&fila->mutex, NULL);
    sem_init(&fila->vazio, 0, capacidade);
    sem_init(&fila->cheio, 0, 0);

    return fila;
}

/**
 * @brief Destrói uma fila de imagens e libera seus recursos
 * @param fila Ponteiro para a fila a ser destruída
 * 
 * Libera todos os recursos associados à fila, incluindo:
 * - Mutex e semáforos
 * - Array de imagens
 * - Array de futures
 * - Estrutura da fila
 */
void destruir_fila(FilaImagens* fila) {
    if (!fila) return;

    // Libera recursos de sincronização
    pthread_mutex_destroy(&fila->mutex);
    sem_destroy(&fila->vazio);
    sem_destroy(&fila->cheio);

    // Libera memória
    free(fila->imagens);
    free(fila->futures);
    free(fila);
}

/**
 * @brief Insere uma imagem na fila
 * @param fila Ponteiro para a fila
 * @param img Ponteiro para a imagem a ser inserida
 * @return 1 se a inserção foi bem-sucedida, 0 caso contrário
 * 
 * A função é thread-safe e bloqueia se a fila estiver cheia.
 * Cria um novo Future para a imagem inserida.
 */
int inserir_imagem_na_fila(FilaImagens* fila, Imagem* img) {
    if (!fila || !img) return 0;

    // Espera por um slot vazio
    sem_wait(&fila->vazio);
    
    // Trava o mutex para modificar a fila
    pthread_mutex_lock(&fila->mutex);

    // Cria um novo future para a imagem
    Future* future = criar_future();
    if (!future) {
        pthread_mutex_unlock(&fila->mutex);
        sem_post(&fila->vazio);
        return 0;
    }

    printf("Future criado para imagem: %s\n", img->nome);

    // Copia a imagem para a fila
    strncpy(fila->imagens[fila->fim].nome, img->nome, sizeof(fila->imagens[fila->fim].nome) - 1);
    fila->imagens[fila->fim].largura = img->largura;
    fila->imagens[fila->fim].altura = img->altura;
    fila->imagens[fila->fim].canais = img->canais;
    fila->imagens[fila->fim].produtor_id = img->produtor_id;  // Copia o ID do produtor
    
    // Aloca e copia os dados da imagem
    size_t tamanho_dados = img->largura * img->altura * img->canais;
    fila->imagens[fila->fim].dados = (unsigned char*)malloc(tamanho_dados);
    if (fila->imagens[fila->fim].dados) {
        memcpy(fila->imagens[fila->fim].dados, img->dados, tamanho_dados);
    }

    // Armazena o future
    fila->futures[fila->fim] = future;
    printf("Future armazenado na posição %d da fila\n", fila->fim);

    // Atualiza índices da fila
    fila->fim = (fila->fim + 1) % fila->capacidade;
    fila->tamanho++;

    // Libera o mutex
    pthread_mutex_unlock(&fila->mutex);
    
    // Sinaliza que há um novo item na fila
    sem_post(&fila->cheio);

    return 1;
}

/**
 * @brief Remove uma imagem da fila
 * @param fila Ponteiro para a fila
 * @param img Ponteiro para onde a imagem será copiada
 * @return 1 se a remoção foi bem-sucedida, 0 caso contrário
 * 
 * A função é thread-safe e bloqueia se a fila estiver vazia.
 * Copia a imagem e seu Future associado.
 */
int remover_imagem_da_fila(FilaImagens* fila, Imagem* img) {
    if (!fila || !img) return 0;

    // Espera por um item na fila
    sem_wait(&fila->cheio);
    
    // Trava o mutex para modificar a fila
    pthread_mutex_lock(&fila->mutex);

    // Obtém o future da imagem
    Future* future = fila->futures[fila->inicio];

    // Copia a imagem da fila
    strncpy(img->nome, fila->imagens[fila->inicio].nome, sizeof(img->nome) - 1);
    img->largura = fila->imagens[fila->inicio].largura;
    img->altura = fila->imagens[fila->inicio].altura;
    img->canais = fila->imagens[fila->inicio].canais;
    
    // Aloca e copia os dados da imagem
    size_t tamanho_dados = img->largura * img->altura * img->canais;
    img->dados = (unsigned char*)malloc(tamanho_dados);
    if (img->dados) {
        memcpy(img->dados, fila->imagens[fila->inicio].dados, tamanho_dados);
    }

    // Libera os dados da imagem na fila
    free(fila->imagens[fila->inicio].dados);
    fila->imagens[fila->inicio].dados = NULL;

    // Limpa o future
    fila->futures[fila->inicio] = NULL;

    // Atualiza índices da fila
    fila->inicio = (fila->inicio + 1) % fila->capacidade;
    fila->tamanho--;

    // Libera o mutex
    pthread_mutex_unlock(&fila->mutex);
    
    // Sinaliza que há um novo slot vazio
    sem_post(&fila->vazio);

    return 1;
}

/**
 * @brief Função principal do programa
 * @return 0 em caso de sucesso, 1 em caso de erro
 * 
 * A função:
 * 1. Inicializa a fila de imagens
 * 2. Cria threads produtoras e consumidoras
 * 3. Aguarda o processamento de todas as imagens
 * 4. Coleta e exibe métricas de desempenho
 * 5. Libera recursos
 * 
 * Diretórios padrão:
 * - Entrada: "imagens/entrada"
 * - Saída: "imagens/saida"
 */
int main() {
    printf("Iniciando Processador de Imagens Paralelo\n");
    printf("Número de produtores: %d\n", NUM_PRODUTORES);
    printf("Número de consumidores: %d\n", NUM_CONSUMIDORES);
    
    struct timespec inicio_total, fim_total;
    clock_gettime(CLOCK_MONOTONIC, &inicio_total);
    
    FilaImagens* fila = criar_fila(10);
    if (!fila) {
        printf("Erro ao criar fila\n");
        return 1;
    }
    
    printf("Fila criada com sucesso!\n");
    
    // Criar arrays de threads e argumentos
    pthread_t prod_threads[NUM_PRODUTORES];
    pthread_t cons_threads[NUM_CONSUMIDORES];
    ThreadArgs args_prod[NUM_PRODUTORES];
    ThreadArgs args_cons[NUM_CONSUMIDORES];
    
    // Inicializar argumentos e criar threads dos produtores
    for (int i = 0; i < NUM_PRODUTORES; i++) {
        args_prod[i].fila = fila;
        args_prod[i].diretorio_entrada = "imagens/entrada";
        args_prod[i].diretorio_saida = "imagens/saida";
        args_prod[i].thread_id = i;
        
        if (pthread_create(&prod_threads[i], NULL, produtor, &args_prod[i]) != 0) {
            perror("Erro ao criar thread do produtor");
            executando = 0;
            break;
        }
    }
    
    // Inicializar argumentos e criar threads dos consumidores
    for (int i = 0; i < NUM_CONSUMIDORES; i++) {
        args_cons[i].fila = fila;
        args_cons[i].diretorio_entrada = "imagens/entrada";
        args_cons[i].diretorio_saida = "imagens/saida";
        args_cons[i].thread_id = i;
        
        if (pthread_create(&cons_threads[i], NULL, consumidor, &args_cons[i]) != 0) {
            perror("Erro ao criar thread do consumidor");
            executando = 0;
            break;
        }
    }
    
    // Criar thread do monitor
    pthread_t monitor_thread;
    MonitorArgs args_monitor;
    args_monitor.fila = fila;
    args_monitor.thread_id = 0;
    
    if (pthread_create(&monitor_thread, NULL, monitor, &args_monitor) != 0) {
        perror("Erro ao criar thread do monitor");
        executando = 0;
    }
    
    // Aguardar todas as threads dos produtores terminarem
    for (int i = 0; i < NUM_PRODUTORES; i++) {
        pthread_join(prod_threads[i], NULL);
    }
    
    // Aguardar um pouco para garantir que todas as imagens sejam processadas
    sleep(2);
    
    // Sinalizar para os consumidores pararem
    executando = 0;
    
    // Aguardar todas as threads dos consumidores terminarem
    for (int i = 0; i < NUM_CONSUMIDORES; i++) {
        pthread_join(cons_threads[i], NULL);
    }
    
    // Aguardar thread do monitor
    pthread_join(monitor_thread, NULL);
    
    // Calcular e exibir métricas
    clock_gettime(CLOCK_MONOTONIC, &fim_total);
    double tempo_total = (fim_total.tv_sec - inicio_total.tv_sec) + 
                        (fim_total.tv_nsec - inicio_total.tv_nsec) / 1e9;
    
    printf("\n=== Métricas de Desempenho ===\n");
    printf("Tempo total de execução: %.2f segundos\n", tempo_total);
    
    printf("\n=== Produtores (%d threads) ===\n", NUM_PRODUTORES);
    for (int i = 0; i < NUM_PRODUTORES; i++) {
        printf("Produtor %d:\n", i);
        printf("  - Imagens carregadas do disco: %d\n", metricas_produtores[i].imagens_processadas);
        printf("  - Tempo médio por imagem: %.3f segundos\n", 
               metricas_produtores[i].tempo_total / metricas_produtores[i].imagens_processadas);
        printf("  - Tempo total de carregamento: %.3f segundos\n", 
               metricas_produtores[i].tempo_total);
        printf("  - \033[1;33mOrdem de finalização: %dº\033[0m\n", 
               metricas_produtores[i].ordem_finalizacao);
    }
    
    printf("\n=== Consumidores (%d threads) ===\n", NUM_CONSUMIDORES);
    for (int i = 0; i < NUM_CONSUMIDORES; i++) {
        printf("Consumidor %d:\n", i);
        printf("  - Imagens processadas: %d\n", metricas_consumidores[i].imagens_processadas);
        printf("  - Operações por imagem:\n");
        printf("    * Conversão para escala de cinza\n");
        printf("    * Inversão de cores\n");
        printf("    * Ajuste de brilho (+20%%)\n");
        printf("    * Ajuste de contraste (+30%%)\n");
        printf("    * Salvamento no disco\n");
        printf("  - Tempo médio por imagem: %.3f segundos\n", 
               metricas_consumidores[i].tempo_total / metricas_consumidores[i].imagens_processadas);
        printf("  - Tempo total de processamento: %.3f segundos\n", 
               metricas_consumidores[i].tempo_total);
        printf("  - \033[1;33mOrdem de finalização: %dº\033[0m\n", 
               metricas_consumidores[i].ordem_finalizacao);
    }
    
    // Limpeza
    pthread_mutex_destroy(&mutex_metricas);
    pthread_mutex_destroy(&mutex_ordem);
    destruir_fila(fila);
    
    return 0;
}
