# Processador de Imagens Paralelo (Docker)

Este projeto implementa um processador de imagens paralelo usando threads em C, com suporte a containerização via Docker.

## Requisitos

- Docker
- Docker Compose

## Estrutura de Diretórios

```
.
├── Dockerfile
├── docker-compose.yml
├── .dockerignore
├── processador_imagens_paralelo.c
└── imagens/
    ├── entrada/    # Coloque suas imagens aqui
    └── saida/      # Imagens processadas serão salvas aqui
```

## Como Usar

1. Coloque suas imagens no diretório `imagens/entrada/`

2. Construa e inicie o container:
```bash
docker-compose up --build
```

3. Para executar em modo interativo (para debug):
```bash
docker-compose run --rm processador-imagens bash
```

   Dentro do container, você pode:
   ```bash
   # Listar arquivos no diretório atual
   ls

   # Executar o processador de imagens
   ./processador_imagens

   # Verificar as imagens processadas
   ls imagens/saida/

   # Para sair do container, digite:
   exit
   ```

4. Para parar o container:
```bash
docker-compose down
```

## Formatos de Imagem Suportados

- PNG
- JPG/JPEG
- BMP
- TGA

## Operações Realizadas

1. Conversão para escala de cinza
2. Inversão de cores
3. Ajuste de brilho (+20%)
4. Ajuste de contraste (+30%)

## Métricas

O programa exibe métricas detalhadas sobre:
- Tempo total de execução
- Número de imagens processadas por thread
- Tempo médio de processamento
- Ordem de finalização das threads

## Arquitetura do Sistema

### Padrões de Projeto Utilizados

#### 1. Produtor/Consumidor
- **Produtores**: Threads que recebem e adicionam imagens à fila de processamento
- **Consumidores**: Threads que retiram imagens da fila e realizam o processamento
- **Fila Compartilhada**: Estrutura que armazena as imagens pendentes de processamento

#### 2. Future
- Permite obter resultados de forma assíncrona
- Cada imagem processada retorna um Future com o resultado
- Facilita o gerenciamento de múltiplas operações assíncronas

### Estruturas de Sincronização

#### 1. Mutex
- Protege o acesso à fila de imagens
- Garante que apenas uma thread por vez possa modificar a fila

#### 2. Semáforo
- Controla o número máximo de threads de processamento
- Gerencia o acesso aos recursos do sistema

## Configuração do Docker

O ambiente Docker inclui:
- Ubuntu 22.04 como sistema base
- GCC e ferramentas de compilação
- Bibliotecas necessárias (stb_image)
- Diretórios montados para entrada/saída de imagens

Para mais detalhes sobre a instalação nativa (sem Docker), consulte o arquivo `README_NATIVE.md`.
