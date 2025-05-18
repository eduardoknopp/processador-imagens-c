# Script de Apresentação - Processador de Imagens Paralelo

## 1. Introdução (1 minuto)
**Eduardo Knopp:**
"Hoje vamos apresentar nosso Processador de Imagens Paralelo, um projeto desenvolvido em C que utiliza programação concorrente para processar múltiplas imagens simultaneamente."

## 2. Arquitetura e Padrões de Projeto (3 minutos)
**Eduardo Knopp:**
"Vamos explicar os principais padrões de projeto utilizados:

### Padrão Produtor/Consumidor:
- Implementamos uma fila thread-safe para gerenciar as imagens
- 5 threads produtoras que carregam imagens
- 5 threads consumidoras que processam as imagens
- Sincronização via mutex e semáforos

### Padrão Future:
- Cada imagem processada retorna um Future
- Permite obter resultados de forma assíncrona
- Facilita o gerenciamento de múltiplas operações
- Implementamos um monitor assíncrono para acompanhar o processamento"

## 3. Implementação Técnica (3 minutos)
**Eduardo Knopp:**
"Vamos detalhar os aspectos técnicos:

### Estruturas de Dados:
- Imagem: Armazena dados da imagem (nome, dimensões, pixels)
- FilaImagens: Gerencia a fila de processamento
- Future: Controla o resultado assíncrono

### Sincronização:
- Mutex para proteger acesso à fila
- Semáforos para controlar slots vazios/ocupados
- Variáveis de condição para Future

### Operações de Imagem:
- Conversão para escala de cinza
- Inversão de cores
- Ajuste de brilho (+20%)
- Ajuste de contraste (+30%)"

## 4. Configuração e Execução (2 minutos)
**Igor Ferreira:**
"O projeto pode ser executado de duas formas:

### Usando Docker:
```bash
# Construir e iniciar o container
docker-compose up --build

# Para execução interativa (debug)
docker-compose run --rm processador-imagens bash
```

### Instalação Nativa:
```bash
# Instalar dependências (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install build-essential curl

# Baixar bibliotecas necessárias
curl -o stb_image.h https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
curl -o stb_image_write.h https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h

# Compilar o projeto
gcc -o processador_imagens processador_imagens_paralelo.c -pthread -lm

# Executar
./processador_imagens
```

### Estrutura de Diretórios:
```
.
├── processador_imagens_paralelo.c
├── stb_image.h
├── stb_image_write.h
└── imagens/
    ├── entrada/    # Coloque suas imagens aqui
    └── saida/      # Imagens processadas serão salvas aqui
```

Formatos suportados: PNG, JPG/JPEG, BMP, TGA"

## 5. Métricas e Desempenho (1 minuto)
**Igor Ferreira:**
"O sistema coleta métricas importantes:
- Tempo total de execução
- Número de imagens por thread
- Tempo médio de processamento
- Ordem de finalização das threads

Estas métricas nos permitem analisar o desempenho do sistema e identificar possíveis gargalos no processamento."

## 6. Demonstração (2 minutos)
**Eduardo Knopp:**
"Vamos demonstrar o sistema em ação:
1. Carregamento de imagens pelos produtores
2. Processamento paralelo pelos consumidores
3. Monitoramento assíncrono do progresso
4. Visualização das métricas em tempo real"

## 7. Conclusão (1 minuto)
**Eduardo Knopp:**
"Concluindo, nosso Processador de Imagens Paralelo demonstra:
- Uso eficiente de programação concorrente
- Implementação robusta de padrões de projeto
- Processamento assíncrono com Future
- Métricas detalhadas de desempenho" 