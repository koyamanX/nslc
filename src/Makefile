CXX=g++
CXX_FLAGS=
CXX_LDFLAGS=

CXX_FILES=main.cpp lexer.cpp token.cpp parser.cpp ast.cpp
CXX_OBJS=$(CXX_FILES:.cpp=.o)

all: nslc

%.o: %.c
	$(CXX) -I./ -c %< %@

nslc: $(CXX_OBJS)
	$(CXX) -o $@ $^

clean:
	rm $(CXX_OBJS)
