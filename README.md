# 3D Room with FPS Camera

This is a C++ project that uses OpenGL and GLUT to render a 3D scene of a room. It features a first-person shooter (FPS) style camera for navigation, dynamic lighting, animations, and texture mapping using the SOIL2 library.

## Features

*   **3D Room Environment:** A fully enclosed room with textured walls and a floor.
*   **FPS-Style Camera:** Navigate the scene with standard FPS controls (WASD for movement, mouse or arrow keys for looking around).
*   **Dynamic Lighting:** The scene is illuminated by a point light source attached to a swaying lamp, creating dynamic shadows and highlights.
*   **Animation:**
    *   The lamp hanging from the ceiling sways back and forth.
    *   A textured model of the Earth rotates on the table.
*   **Texture Mapping:** The SOIL2 library is used to apply textures to various objects in the scene, including:
    *   Floor
    *   Walls
    *   A wooden table
    *   A painting on the wall
    *   A rotating Earth model
*   **Geometric Primitives:** The scene is built using various geometric primitives, including cubes, spheres, and tori.
*   **User Controls:**
    *   **W/S/A/D:** Move forward, backward, strafe left, and strafe right.
    *   **Q/E:** Move up and down.
    *   **Arrow Keys:** Look up, down, left, and right.
    *   **Shift:** Move faster.
    *   **P:** Toggle between perspective and orthographic projection.
    *   **Z/X:** Zoom in and out.
    *   **M:** Toggle animation on and off.
    *   **T:** Toggle the visibility of the coordinate axes.
    *   **R:** Reset the camera to its initial position.
    *   **ESC:** Quit the application.

## Dependencies

*   **GLUT:** The OpenGL Utility Toolkit for creating windows and handling events.
*   **SOIL2:** A tiny C library used for loading textures.

## How to Compile and Run

To compile and run this project, you will need to have GLUT and SOIL2 libraries set up in your development environment.

1.  **Compile the code:**
    ```bash
    g++ main.cpp -o room -lglut -lGL -lGLU -lSOIL2
    ```
2.  **Run the executable:**
    ```bash
    ./room
    ```

## Project Structure

*   `main.cpp`: The main source code file containing all the logic for rendering the scene, handling user input, and managing animations.
*   `opengl/`: Directory containing the GLUT header files.
*   `SOIL2/`: Directory containing the SOIL2 library files.
*   `textures/`: Directory containing the texture images used in the project.
