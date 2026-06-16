all:
	g++ main.cpp RedisServer.cpp ClientHandler.cpp Store.cpp CommandParser.cpp Replicator.cpp -o redis-server -lpthread

clean:
	rm -f redis-server