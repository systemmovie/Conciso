all: clean conciso run

conciso:
	clang++ -Wall -O0 -g -Werror -std=c++11 conciso.cpp -L/usr/local/Cellar/discount/2.2.0/lib -lmarkdown -I/usr/local/Cellar/discount/2.2.0/include -o conciso
run: conciso
	clear && ./conciso
clean:
	rm -rf conciso