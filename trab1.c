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
    int canais;           // Número de canais (RGB = 3, RGBA = 4)
    unsigned char* dados; // Dados da imagem
} Imagem;

typedef struct {
    Imagem* imagens;      // Array de imagens
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
#define NUM_PRODUTORES 2
#define NUM_CONSUMIDORES 2

// Estrutura para métricas
typedef struct {
    int imagens_processadas;
    double tempo_total;
} Metricas;

// Variáveis globais para métricas
Metricas metricas_produtores[NUM_PRODUTORES];
Metricas metricas_consumidores[NUM_CONSUMIDORES];
pthread_mutex_t mutex_metricas = PTHREAD_MUTEX_INITIALIZER;

// Declarações das funções
FilaImagens* criar_fila(int capacidade);
void destruir_fila(FilaImagens* fila);
int inserir_imagem(FilaImagens* fila, Imagem* img);
int remover_imagem(FilaImagens* fila, Imagem* img);
Imagem* carregar_imagem(const char* caminho);
void liberar_imagem(Imagem* img);
void* produtor(void* arg);
void* consumidor(void* arg);

// Variáveis globais para controle
int executando = 1;  // Flag para controlar a execução das threads

// Função para carregar uma imagem do disco
Imagem* carregar_imagem(const char* caminho) {
    Imagem* img = (Imagem*)malloc(sizeof(Imagem));
    if (!img) {
        perror("Erro ao alocar estrutura de imagem");
        return NULL;
    }

    // Copia o nome do arquivo
    strncpy(img->nome, caminho, sizeof(img->nome) - 1);
    img->nome[sizeof(img->nome) - 1] = '\0';

    // Carrega a imagem usando stb_image
    img->dados = stbi_load(caminho, &img->largura, &img->altura, &img->canais, 0);
    
    if (!img->dados) {
        printf("Erro ao carregar imagem %s: %s\n", caminho, stbi_failure_reason());
        free(img);
        return NULL;
    }

    printf("Imagem carregada: %s (%dx%d, %d canais)\n", 
           caminho, img->largura, img->altura, img->canais);

    return img;
}

// Função para liberar uma imagem
void liberar_imagem(Imagem* img) {
    if (img) {
        if (img->dados) {
            stbi_image_free(img->dados);
        }
        free(img);
    }
}

// Função para salvar uma imagem processada
void salvar_imagem(Imagem* img, const char* diretorio_saida) {
    if (!img || !img->dados) {
        printf("Erro: Imagem inválida para salvar.\n");
        return;
    }

    // Extrair apenas o nome do arquivo do caminho completo
    const char* nome_arquivo = strrchr(img->nome, '/');
    if (nome_arquivo) {
        nome_arquivo++; // Pula a barra
    } else {
        nome_arquivo = img->nome;
    }

    // Construir o caminho de saída
    char caminho_saida[512];
    snprintf(caminho_saida, sizeof(caminho_saida), "%s/%s", diretorio_saida, nome_arquivo);

    // Salvar a imagem no formato PNG
    if (!stbi_write_png(caminho_saida, img->largura, img->altura, img->canais, img->dados, img->largura * img->canais)) {
        printf("Erro ao salvar imagem: %s\n", caminho_saida);
    } else {
        printf("Imagem salva com sucesso: %s\n", caminho_saida);
    }
}

// Funções de processamento de imagem
void converter_para_cinza(Imagem* img) {
    if (!img || !img->dados || img->canais < 3) return;
    
    for (int i = 0; i < img->largura * img->altura; i++) {
        unsigned char* pixel = &img->dados[i * img->canais];
        unsigned char cinza = (unsigned char)(0.299f * pixel[0] + 0.587f * pixel[1] + 0.114f * pixel[2]);
        pixel[0] = pixel[1] = pixel[2] = cinza;
    }
}

void inverter_cores(Imagem* img) {
    if (!img || !img->dados) return;
    
    for (int i = 0; i < img->largura * img->altura * img->canais; i++) {
        img->dados[i] = 255 - img->dados[i];
    }
}

void ajustar_brilho(Imagem* img, float fator) {
    if (!img || !img->dados) return;
    
    for (int i = 0; i < img->largura * img->altura * img->canais; i++) {
        int novo_valor = (int)(img->dados[i] * fator);
        img->dados[i] = (unsigned char)(novo_valor > 255 ? 255 : (novo_valor < 0 ? 0 : novo_valor));
    }
}

void ajustar_contraste(Imagem* img, float fator) {
    if (!img || !img->dados) return;
    
    for (int i = 0; i < img->largura * img->altura * img->canais; i++) {
        int novo_valor = (int)((img->dados[i] - 128) * fator + 128);
        img->dados[i] = (unsigned char)(novo_valor > 255 ? 255 : (novo_valor < 0 ? 0 : novo_valor));
    }
}

// Função para atualizar métricas
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

// Função do produtor modificada
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
        if (ent->d_type == DT_REG) {
            clock_gettime(CLOCK_MONOTONIC, &inicio);
            
            char caminho[512];
            snprintf(caminho, sizeof(caminho), "%s/%s", 
                    args->diretorio_entrada, ent->d_name);
            
            Imagem* img = carregar_imagem(caminho);
            if (img) {
                printf("Produtor %d: Carregando imagem %s\n", 
                       args->thread_id, ent->d_name);
                
                if (inserir_imagem(args->fila, img)) {
                    printf("Produtor %d: Imagem %s inserida na fila\n", 
                           args->thread_id, ent->d_name);
                }
                
                liberar_imagem(img);
            }
            
            clock_gettime(CLOCK_MONOTONIC, &fim);
            double tempo = (fim.tv_sec - inicio.tv_sec) + 
                          (fim.tv_nsec - inicio.tv_nsec) / 1e9;
            atualizar_metricas(args->thread_id, 0, tempo);
        }
    }
    
    closedir(dir);
    printf("Produtor %d finalizado\n", args->thread_id);
    return NULL;
}

// Função do consumidor modificada
void* consumidor(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    struct timespec inicio, fim;
    
    printf("Consumidor %d iniciado\n", args->thread_id);
    
    while (executando) {
        Imagem img;
        
        if (remover_imagem(args->fila, &img)) {
            clock_gettime(CLOCK_MONOTONIC, &inicio);
            
            printf("Consumidor %d: Processando imagem %s\n", 
                   args->thread_id, img.nome);
            
            converter_para_cinza(&img);
            inverter_cores(&img);
            ajustar_brilho(&img, 1.2f);
            ajustar_contraste(&img, 1.3f);
            
            salvar_imagem(&img, args->diretorio_saida);
            
            free(img.dados);
            
            clock_gettime(CLOCK_MONOTONIC, &fim);
            double tempo = (fim.tv_sec - inicio.tv_sec) + 
                          (fim.tv_nsec - inicio.tv_nsec) / 1e9;
            atualizar_metricas(args->thread_id, 1, tempo);
        }
    }
    
    printf("Consumidor %d finalizado\n", args->thread_id);
    return NULL;
}

// Função para inicializar a fila
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

// Função para destruir a fila
void destruir_fila(FilaImagens* fila) {
    if (!fila) return;

    // Libera recursos de sincronização
    pthread_mutex_destroy(&fila->mutex);
    sem_destroy(&fila->vazio);
    sem_destroy(&fila->cheio);

    // Libera memória
    free(fila->imagens);
    free(fila);
}

// Função para inserir uma imagem na fila
int inserir_imagem(FilaImagens* fila, Imagem* img) {
    if (!fila || !img) return 0;

    // Espera por um slot vazio
    sem_wait(&fila->vazio);
    
    // Trava o mutex para modificar a fila
    pthread_mutex_lock(&fila->mutex);

    // Copia a imagem para a fila
    strncpy(fila->imagens[fila->fim].nome, img->nome, sizeof(fila->imagens[fila->fim].nome) - 1);
    fila->imagens[fila->fim].largura = img->largura;
    fila->imagens[fila->fim].altura = img->altura;
    fila->imagens[fila->fim].canais = img->canais;
    
    // Aloca e copia os dados da imagem
    size_t tamanho_dados = img->largura * img->altura * img->canais;
    fila->imagens[fila->fim].dados = (unsigned char*)malloc(tamanho_dados);
    if (fila->imagens[fila->fim].dados) {
        memcpy(fila->imagens[fila->fim].dados, img->dados, tamanho_dados);
    }

    // Atualiza índices da fila
    fila->fim = (fila->fim + 1) % fila->capacidade;
    fila->tamanho++;

    // Libera o mutex
    pthread_mutex_unlock(&fila->mutex);
    
    // Sinaliza que há um novo item na fila
    sem_post(&fila->cheio);

    return 1;
}

// Função para remover uma imagem da fila
int remover_imagem(FilaImagens* fila, Imagem* img) {
    if (!fila || !img) return 0;

    // Espera por um item na fila
    sem_wait(&fila->cheio);
    
    // Trava o mutex para modificar a fila
    pthread_mutex_lock(&fila->mutex);

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

    // Atualiza índices da fila
    fila->inicio = (fila->inicio + 1) % fila->capacidade;
    fila->tamanho--;

    // Libera o mutex
    pthread_mutex_unlock(&fila->mutex);
    
    // Sinaliza que há um novo slot vazio
    sem_post(&fila->vazio);

    return 1;
}

// Função main modificada
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
    
    // Calcular e exibir métricas
    clock_gettime(CLOCK_MONOTONIC, &fim_total);
    double tempo_total = (fim_total.tv_sec - inicio_total.tv_sec) + 
                        (fim_total.tv_nsec - inicio_total.tv_nsec) / 1e9;
    
    printf("\nMétricas de Desempenho:\n");
    printf("Tempo total de execução: %.2f segundos\n", tempo_total);
    
    printf("\nProdutores:\n");
    for (int i = 0; i < NUM_PRODUTORES; i++) {
        printf("Produtor %d: %d imagens, tempo médio: %.3f segundos\n",
               i, metricas_produtores[i].imagens_processadas,
               metricas_produtores[i].tempo_total / metricas_produtores[i].imagens_processadas);
    }
    
    printf("\nConsumidores:\n");
    for (int i = 0; i < NUM_CONSUMIDORES; i++) {
        printf("Consumidor %d: %d imagens, tempo médio: %.3f segundos\n",
               i, metricas_consumidores[i].imagens_processadas,
               metricas_consumidores[i].tempo_total / metricas_consumidores[i].imagens_processadas);
    }
    
    // Limpeza
    pthread_mutex_destroy(&mutex_metricas);
    destruir_fila(fila);
    
    return 0;
}
