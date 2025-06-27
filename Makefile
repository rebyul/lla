TARGET = bin/dbview
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, obj/%.o, $(SRC))

run: clean default
	./$(TARGET) -f ./mynewdb.db -n 
	./$(TARGET) -f ./mynewdb.db -a "Timmy H.,123 Sheshire Ln.,120"
	./$(TARGET) -f ./mynewdb.db -r "Timmy H."
	./$(TARGET) -f ./mynewdb.db -a "Timmy H.,123 Sheshire Ln.,120"
	./$(TARGET) -f ./mynewdb.db -a "Kenny G,44 Wallaby Way,70"
	./$(TARGET) -f ./mynewdb.db -a "Tim T,5/8 Harbour St,20"
	./$(TARGET) -f ./mynewdb.db -l
	./$(TARGET) -f ./mynewdb.db -r "Kenny G"
	./$(TARGET) -f ./mynewdb.db -l
	./$(TARGET) -f ./mynewdb.db -u "Tim T" -e 200
	./$(TARGET) -f ./mynewdb.db -l

default: $(TARGET)

clean:
	rm -f obj/*.o
	rm -f bin/*
	rm -f *.db

$(TARGET): $(OBJ)
	gcc -o $@ $?

obj/%.o : src/%.c
	gcc -c $< -o $@ -Iinclude


