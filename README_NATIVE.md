# Processador de Imagens Paralelo (Instalação Nativa)

Este projeto implementa um processador de imagens paralelo usando threads em C, com suporte para instalação nativa no sistema.

## Requisitos

- GCC (GNU Compiler Collection)
- Make
- pthread (biblioteca de threads POSIX)
- curl (para download da biblioteca stb_image.h)

## Instalação

### Ubuntu/Debian
```bash
# Atualizar lista de pacotes
sudo apt-get update

# Instalar compilador e ferramentas de desenvolvimento
sudo apt-get install build-essential curl

# Baixar as bibliotecas stb_image.h e stb_image_write.h
curl -o stb_image.h https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
curl -o stb_image_write.h https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h
```

## Estrutura de Diretórios

```
.
├── processador_imagens_paralelo.c  # Código fonte principal
├── stb_image.h       # Biblioteca para carregamento de imagens
├── stb_image_write.h # Biblioteca para salvar imagens
├── imagens/          # Diretório com as imagens para processamento
│   ├── entrada/      # Imagens originais
│   └── saida/        # Imagens processadas
└── README_NATIVE.md  # Este arquivo
```

## Compilação

Para compilar o projeto, execute:

```bash
gcc -o processador_imagens processador_imagens_paralelo.c -pthread -lm
```

## Execução

1. Coloque suas imagens no diretório `imagens/entrada/`

2. Execute o programa:
```bash
./processador_imagens
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

## Solução de Problemas

### Erros Comuns

1. **Erro de compilação com pthread**
   - Verifique se a biblioteca pthread está instalada
   - Certifique-se de usar a flag `-pthread` na compilação

2. **Erro ao carregar imagens**
   - Verifique se as bibliotecas stb_image.h e stb_image_write.h estão presentes
   - Confirme se os formatos de imagem são suportados

3. **Erro de permissão**
   - Verifique as permissões dos diretórios de entrada e saída
   - Certifique-se de que o programa tem permissão de escrita

Para mais detalhes sobre a instalação com Docker, consulte o arquivo `README.md`. 