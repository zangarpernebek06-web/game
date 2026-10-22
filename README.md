# Star Catcher Ultimate (OpenGL)

2D аркада ойыны: жоғарыдан түскен жұлдыздарды платформамен ұстаңыз, метеориттерден сақтаныңыз.
C++ және OpenGL 3.3 (GLFW + GLEW, GLSL шейдерлер, VAO/VBO) арқылы жазылған.

2D arcade game: catch falling stars, dodge meteors. Written in C++ with OpenGL 3.3 (GLFW + GLEW, GLSL shaders, VAO/VBO).

## Басқару / Controls

| Перне / Key | Әрекет / Action |
|---|---|
| `←` `→` / `A` `D` | Платформаны жылжыту / Move |
| `B` | Фон тақырыбын ауыстыру / Change background theme |
| `R` | Қайта бастау / Restart (Game Over) |
| `ESC` | Шығу / Exit (Game Over) |

## Ойын ережелері / Gameplay

- Сары жұлдыз: ұпай + комбо, жылдамдық біртіндеп артады
- Қызыл метеорит: өмір жоғалтады (қалқан болса, бұғатталады)
- Ақ жұлдыз (тұзақ): платформа 5 секундқа кішірейеді
- Жасыл бонус: +1 өмір; көк бонус: 7 секундтық қалқан

## Іске қосу / Build & Run (Ubuntu / WSL)

```bash
sudo apt update
sudo apt install -y build-essential libglfw3-dev libglew-dev libgl1-mesa-dev
g++ -std=c++17 -O2 main.cpp -o game -lglfw -lGLEW -lGL
./game
```

Немесе бәрін бірден / Or all at once:

```bash
bash run.sh
```

WSL-де терезе ашылмаса / If the window does not open in WSL:

```bash
LIBGL_ALWAYS_SOFTWARE=1 ./game
```

## Құрылым / Structure

```
game/
├── main.cpp
├── run.sh
├── README.md
└── .gitignore
```
