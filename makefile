all:gatewayAtr historiadorAtr servApp

gatewayAtr: gatewayAtr.cpp
	g++ gatewayAtr.cpp -o gatewayAtr -lboost_system -lpthread -lrt -std=c++11

historiadorAtr: historiador.cpp
	g++ historiador.cpp -o historiadorAtr -lboost_system -lpthread -lrt -std=c++11

servApp: servApp.cpp
	g++ servApp.cpp -o servApp -lboost_system -lpthread -lrt -std=c++11
