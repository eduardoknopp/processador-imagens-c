# Processador de Imagens Paralelo

Este projeto implementa um processador de imagens que utiliza programação paralela para melhorar o desempenho do processamento de múltiplas imagens simultaneamente.

## Descrição

O sistema implementa um processador de imagens que:
- Recebe múltiplas imagens para processamento
- Aplica diferentes filtros e transformações nas imagens
- Processa as imagens em paralelo usando threads
- Utiliza padrões de projeto para melhor organização e escalabilidade

## Padrões de Projeto Utilizados

### 1. Produtor/Consumidor
- **Produtores**: Threads que recebem e adicionam imagens à fila de processamento
- **Consumidores**: Threads que retiram imagens da fila e realizam o processamento
- **Fila Compartilhada**: Estrutura que armazena as imagens pendentes de processamento

### 2. Future
- Permite obter resultados de forma assíncrona
- Cada imagem processada retorna um Future com o resultado
- Facilita o gerenciamento de múltiplas operações assíncronas

## Estruturas de Sincronização

### 1. Mutex
- Protege o acesso à fila de imagens
- Garante que apenas uma thread por vez possa modificar a fila

### 2. Semáforo
- Controla o número máximo de threads de processamento
- Gerencia o acesso aos recursos do sistema

## Requisitos e Instalação

### Dependências
- GCC (GNU Compiler Collection)
- Make
- pthread (biblioteca de threads POSIX)
- curl (para download da biblioteca stb_image.h)

### Instalação no Ubuntu/Debian
```bash
# Atualizar lista de pacotes
sudo apt-get update

# Instalar compilador e ferramentas de desenvolvimento
sudo apt-get install build-essential curl

# Baixar as bibliotecas stb_image.h e stb_image_write.h
curl -o stb_image.h https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
curl -o stb_image_write.h https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h
```

## Compilação

Para compilar o projeto, execute:

```bash
gcc -o processador_imagens trab1.c -pthread
```

## Execução

```bash
./processador_imagens
```

## Estrutura do Projeto

```
.
├── trab1.c           # Código fonte principal
├── stb_image.h       # Biblioteca para carregamento de imagens
├── stb_image_write.h # Biblioteca para salvar imagens
├── imagens/          # Diretório com as imagens para processamento
│   ├── entrada/      # Imagens originais
│   └── saida/        # Imagens processadas
└── README.md         # Este arquivo
```

## Funcionalidades Planejadas

1. Carregamento de imagens em lote do diretório imagens/entrada
2. Processamento paralelo de múltiplas imagens
3. Aplicação de diferentes filtros:
   - Escala de cinza
   - Blur
   - Detecção de bordas
4. Salvamento das imagens processadas em imagens/saida
5. Relatório de desempenho

## Próximos Passos

1. Implementar a estrutura básica do projeto
2. Desenvolver o sistema de fila com mutex
3. Implementar o padrão produtor/consumidor
4. Adicionar suporte a futures
5. Implementar os processadores de imagem
6. Adicionar testes e documentação
