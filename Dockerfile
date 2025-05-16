# Usar Ubuntu como base
FROM ubuntu:22.04

# Evitar prompts interativos durante a instalação
ENV DEBIAN_FRONTEND=noninteractive

# Instalar dependências necessárias
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    make \
    git \
    vim \
    && rm -rf /var/lib/apt/lists/*

# Criar diretório do projeto
WORKDIR /app

# Copiar arquivos do projeto
COPY . .

# Compilar o programa
RUN gcc -o processador_imagens processador_imagens_paralelo.c -pthread -lm

# Criar diretórios necessários
RUN mkdir -p imagens/entrada imagens/saida

# Comando padrão ao iniciar o container
CMD ["./processador_imagens"] 