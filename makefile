FTP:
	g++ -o ./bin/proxy -g ./src/WebProxy.cpp ./src/simpleSocket.cpp -lpthread
clean:
	-rm -f ./bin/*
