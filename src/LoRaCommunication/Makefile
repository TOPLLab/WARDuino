COMMON = main.cpp circuit.cpp encryption.cpp LoRaHash.cpp TorCell.cpp logicalLayer.cpp
LIBS   = ../../lib/ecdh/ecdh.cpp ../../lib/aes/aes.cpp
CL     = clang++ -std=c++17

mock:
	$(CL) $(COMMON) radio/radio_mock.cpp $(LIBS) -o main

udp:
	$(CL) -DNODE_PORT=5001 -DRADIO_UDP $(COMMON) radio/radio_udp.cpp $(LIBS) -o node_A
	$(CL) -DNODE_PORT=5002 -DRADIO_UDP $(COMMON) radio/radio_udp.cpp $(LIBS) -o node_B
	$(CL) -DNODE_PORT=5003 -DRADIO_UDP $(COMMON) radio/radio_udp.cpp $(LIBS) -o node_C
	$(CL) -DNODE_PORT=5004 -DRADIO_UDP $(COMMON) radio/radio_udp.cpp $(LIBS) -o node_D
	$(CL) -DNODE_PORT=5005 -DRADIO_UDP $(COMMON) radio/radio_udp.cpp $(LIBS) -o node_E
	$(CL) -DNODE_PORT=5006 -DRADIO_UDP $(COMMON) radio/radio_udp.cpp $(LIBS) -o node_F
	$(CL) -DNODE_PORT=5007 -DRADIO_UDP $(COMMON) radio/radio_udp.cpp $(LIBS) -o node_G
	$(CL) -DNODE_PORT=5008 -DRADIO_UDP $(COMMON) radio/radio_udp.cpp $(LIBS) -o node_H

clean:
	rm -f main node_A node_B node_C node_D node_E node_F node_G node_H