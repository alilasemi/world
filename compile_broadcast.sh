nvcc -std=c++17 -c src/particle_dynamics_cuda.cu -o build/obj/particle_dynamics_cuda.o
nvcc -std=c++17 -c src/particle_dynamics.cpp -o build/obj/particle_dynamics.o -Isrc
nvcc -std=c++17 src/broadcast.cpp build/obj/particle_dynamics_cuda.o build/obj/particle_dynamics.o -o build/bin/broadcast /home/ali/uWebSockets/uSockets/uSockets.a -lz -lssl -lcrypto -lpthread -I/home/ali/uWebSockets/src/ -I/home/ali/uWebSockets/uSockets/src/
