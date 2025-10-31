# syntax=docker/dockerfile:1.5
FROM ubuntu:22.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential \
       cmake \
       ca-certificates \
       git \
       libssl-dev \
       libmysqlcppconn-dev \
       ccache \
    && rm -rf /var/lib/apt/lists/*

# Конфигурация ccache (ускорение инкрементальной сборки)
ENV CCACHE_DIR=/ccache
ENV CCACHE_COMPRESS=1
ENV CCACHE_MAXSIZE=5G

WORKDIR /src

COPY . .

# Генерация и сборка релизного билда с кешами BuildKit
# Требуется DOCKER_BUILDKIT=1. Важно: артефакт копируем в не-кешируемый каталог.
RUN --mount=type=cache,target=/build-site \
    --mount=type=cache,target=/ccache \
    cmake -S examples/site -B /build-site \
      -DCMAKE_BUILD_TYPE=Release \
      -DSERVER_WEB_WITH_MYSQL=ON \
      -DCMAKE_C_COMPILER_LAUNCHER=ccache \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    && cmake --build /build-site --config Release -j"$(nproc)" \
    && mkdir -p /out \
    && cp /build-site/example_site /out/example_site



################################################################################
# Этап рантайма (минимальный образ + необходимые so-библиотеки)
################################################################################
FROM ubuntu:22.04 AS runtime

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       ca-certificates \
       # для клиента OpenSSL, который нужен приложению
       libssl3 \
       libmysqlcppconn-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Бинарник
COPY --from=builder /out/example_site /app/example_site

# SSL-ключи (приложение ожидает путь "ssl/cert.pem" и "ssl/key.pem")
COPY examples/site/ssl /app/ssl

# Библиотеки OpenSSL устанавливаются как пакет рантайма (libssl3)

EXPOSE 80 443

# По умолчанию запускаем HTTP(80) и HTTPS(443) сервер
CMD ["/app/example_site"]
