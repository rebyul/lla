TARGET_SRV = bin/dbserver
TARGET_CLI = bin/dbcli

SRC_SRV = $(wildcard src/srv/*.c)
OBJ_SRV = $(SRC_SRV:src/srv/%.c=obj/srv/%.o)

SRC_CLI=$(wildcard src/cli/*.c)
OBJ_CLI = $(SRC_CLI:src/cli/%.c=obj/cli/%.o)

run: clean default
	# ./$(TARGET_CLI) -f ./mynewdb.db -n 
	# ./$(TARGET_CLI) -f ./mynewdb.db -a "Timmy H.,123 Sheshire Ln.,120"
	# ./$(TARGET_CLI) -f ./mynewdb.db -r "Timmy H."
	# ./$(TARGET_CLI) -f ./mynewdb.db -a "Timmy H.,123 Sheshire Ln.,120"
	# ./$(TARGET_CLI) -f ./mynewdb.db -a "Kenny G,44 Wallaby Way,70"
	# ./$(TARGET_CLI) -f ./mynewdb.db -a "Tim T,5/8 Harbour St,20"
	# ./$(TARGET_CLI) -f ./mynewdb.db -l
	# ./$(TARGET_CLI) -f ./mynewdb.db -r "Kenny G"
	# ./$(TARGET_CLI) -f ./mynewdb.db -l
	# ./$(TARGET_CLI) -f ./mynewdb.db -u "Tim T" -e 200
	# ./$(TARGET_CLI) -f ./mynewdb.db -l
	./$(TARGET_SRV) -f ./mynewdb.db -n -p 9999

default: $(TARGET_SRV) $(TARGET_CLI)

clean:
	rm -f obj/srv/*.o
	rm -f bin/*
	rm -f *.db

$(TARGET_SRV): $(OBJ_SRV)
	gcc -o $@ $?

$(OBJ_SRV): obj/srv/%.o: src/srv/%.c
	gcc -c $< -o $@ -Iinclude

$(TARGET_CLI): $(OBJ_CLI)
	gcc -o $@ $?

$(OBJ_CLI): obj/cli/%.o: src/cli/%.c
	gcc -c $< -o $@ -Iinclude
