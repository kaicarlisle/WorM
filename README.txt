worm - a simple 4-window-per-workspace tiler for linux. Because who wants to look at more than 4 windows at a time?

Started from the `dminiwm` tiling window manager on 6th March 2022

TODO:
    Add a panel. There is currently a toggle-able gap for a panel at the top of the screen.
    Enable screen rotation

Basic operations

Window layouts (for 1-4 windows. The 5th window is opened on the next workspace with room)
        ------------------------------
        |                            |
        |                            |
        |                            |
        |              a             |
        |                            |
        |                            |
        |                            |
        ------------------------------
        ------------------------------
        |              |             |
        |              |             |
        |              |             |
        |       a      x      b      |
        |              |             |
        |              |             |
        |              |             |
        ------------------------------
        ------------------------------
        |              |             |
        |              |      b      |
        |              |             |
        |       a      x-------------|
        |              |             |
        |              |      c      |
        |              |             |
        ------------------------------
        ------------------------------
        |              |             |
        |      a       |      b      |
        |              |             |
        |--------------x-------------|
        |              |             |
        |      d       |      c      |
        |              |             |
        ------------------------------ 

Resizing quadrants
    For each workspace, the splitting point can be moved around
        ------------------------------
        |                     |      |
        |          1          |  2   |
        |                     |      |
        |                     |      |
        |---------------------x------|
        |          3          |  4   |
        |                     |      |
        ------------------------------ 

Focus
    You can click on a window to focus it, or move focus with binds. Focus can be moved across workspaces.
    You can also swap the active window with another, using hotkeys. Also you can send a window to another workspace.


Installation
    clone the repo to a directory of you choice.
    `cd worm`
    modify `config.h.def` then save as `config.h`
    `make`
    `sudo make install`
    `make clean`
