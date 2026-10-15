# One image for the whole app: FastAPI serves /api/analyze and the built frontend.
# Build from ast-studio/:  docker build -t ast-studio .

FROM debian:bookworm-slim AS engine
RUN apt-get update && apt-get install -y --no-install-recommends g++ cmake make \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY engine engine
COPY samples samples
RUN rm -rf engine/build* \
    && cmake -S engine -B engine/build \
    && cmake --build engine/build -j \
    && cd engine/build && ctest --output-on-failure

FROM node:22-slim AS frontend
WORKDIR /src/frontend
COPY frontend/package.json frontend/package-lock.json ./
RUN npm ci
COPY frontend ./
RUN rm -rf dist && npm run build

FROM python:3.12-slim
RUN apt-get update && apt-get install -y --no-install-recommends clang clang-format \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY backend/requirements.txt backend/
RUN pip install --no-cache-dir -r backend/requirements.txt
COPY backend/app.py backend/
COPY --from=engine /src/engine/build/astc engine/build/astc
COPY --from=frontend /src/frontend/dist frontend/dist
RUN useradd --create-home app
USER app
ENV PORT=8000
EXPOSE 8000
WORKDIR /app/backend
CMD ["sh", "-c", "uvicorn app:app --host 0.0.0.0 --port ${PORT}"]
