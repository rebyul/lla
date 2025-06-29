TARGET_SRV = bin/dbserver
TARGET_CLI = bin/dbcli

SRC_SRV = $(wildcard src/srv/*.c)
OBJ_SRV = $(SRC_SRV:src/srv/%.c=obj/srv/%.o)

SRC_CLI=$(wildcard src/cli/*.c)
OBJ_CLI = $(SRC_CLI:src/cli/%.c=obj/cli/%.o)

run: clean default
	./$(TARGET_SRV) -f ./mynewdb.db -n -p 9999

default: $(TARGET_SRV) $(TARGET_CLI)

clean:
	rm -f obj/srv/*.o
	rm -f bin/*
	rm -f *.db

# -g is for debug versions of the intermediary files
# need these for cpp debugger (OpenDebugAD7) in mason to debug correctly
$(TARGET_SRV): $(OBJ_SRV)
	gcc  -o $@ $?
	# gcc -g -o $@ $?


$(OBJ_SRV): obj/srv/%.o: src/srv/%.c
	gcc -c $< -o $@ -Iinclude
	# gcc -g -c $< -o $@ -Iinclude

$(TARGET_CLI): $(OBJ_CLI)
	gcc -o $@ $?

$(OBJ_CLI): obj/cli/%.o: src/cli/%.c
	gcc -c $< -o $@ -Iinclude
	# gcc -g -c $< -o $@ -Iinclude
