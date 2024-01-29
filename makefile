compile: main.c timer.h common.h
	gcc -pthread main.c -o main

attacker: attacker.c
	gcc -pthread attacker.c -o attacker -lm

client: client.c
	gcc -pthread client.c -o client
clean: 
	rm main attacker client