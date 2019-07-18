all:
	gcc -o submitter submitter.c
	gcc -pthread -o instagrapd instagrapd.c
	gcc -o worker worker.c
clean:
	rm submitter instagrapd worker
