mkdir build-arm ; mkdir build-amd64 || true

docker run -it -v ./:/app/ arm-builder /bin/bash -c "cd /app/build-arm && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j8 && make all_debs"
docker run -it -v ./:/app/ amd64-builder /bin/bash -c "cd /app/build-amd64 && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j8 && make all_debs"