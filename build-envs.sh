docker build --platform linux/amd64 -t amd64-builder -f dockers/build_env.Dockerfile .
docker build --platform linux/arm64 -t arm-builder -f dockers/build_env.Dockerfile .
