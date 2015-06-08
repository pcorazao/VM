vm: p3 p3fmt
	
p3: ptc2e.c
	gcc -o p3 ptc2e.c -l pthread
	
p3fmt: p3fmt.c
	gcc -o p3fmt p3fmt.c
	
clean:
	rm p3 p3fmt
