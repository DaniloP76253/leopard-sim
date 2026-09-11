CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2 -Iinclude
LDFLAGS = -lm

RAYLIB_ROOT = third_party/raylib/install
RAYLIB_BUILD_DIR = third_party/raylib/build
RAYLIB_SRC_DIR = third_party/raylib/src
RAYLIB_LIB = $(RAYLIB_ROOT)/lib64/libraylib.a
RAYLIB_LINK_LIBS = -lGL -lpthread -ldl -lrt -lX11 -lXrandr -lXi -lXcursor -lXinerama -lXext

SRC = src/ammo.c src/armor.c src/tank.c src/projectile.c src/combat.c src/terrain.c src/ai.c src/match.c src/main.c
OBJ = $(SRC:.c=.o)
BIN = leopard_sim
RAYLIB_BIN = leopard_sim_3d
RAYLIB_SRC = src/ammo.c src/armor.c src/tank.c src/projectile.c src/combat.c src/terrain.c src/ai.c src/match.c src/main_raylib_fixed.c

.PHONY: all clean run raylib raylib-run

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN)

raylib:
	@set -e; \
	cmake -S third_party/raylib -B $(RAYLIB_BUILD_DIR) -DCMAKE_INSTALL_PREFIX="$(CURDIR)/$(RAYLIB_ROOT)" -DBUILD_EXAMPLES=OFF -DBUILD_SHARED_LIBS=OFF -DPLATFORM=Desktop -DGLFW_BUILD_WAYLAND=ON >/dev/null; \
	cmake --build $(RAYLIB_BUILD_DIR) --target install -j$$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2) >/dev/null; \
	$(CC) $(CFLAGS) -I$(RAYLIB_SRC_DIR) -I$(RAYLIB_ROOT)/include -o $(RAYLIB_BIN) $(RAYLIB_SRC) $(RAYLIB_LIB) $(RAYLIB_LINK_LIBS) $(LDFLAGS)

raylib-run: raylib
	./$(RAYLIB_BIN)

clean:
	rm -f $(OBJ) $(BIN) $(RAYLIB_BIN)
