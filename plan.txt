WorM - A tiling window manager written by me

The screen is the area below the bar. This is split into four quadrants, split at (a moveable) central point
Top left = 1, Top right = 2, Bottom left = 3, Bottom right = 4
There are 10 workspaces

Basic operations

New windows (programs a, b, c, d)
    If current workspace has {x} windows on, launch the new window...
        0 -> fullscreen - covering quadrants 1,2,3,4
            ------------------------------
            |                            |
            |                            |
            |                            |
            |              a             |
            |                            |
            |                            |
            |                            |
            ------------------------------
        1 -> vertical split screen. resize the window covering 1,2,3,4 to just 1,3. The new window covers 2,4
            ------------------------------
            |              |             |
            |              |             |
            |              |             |
            |       a      x      b      |
            |              |             |
            |              |             |
            |              |             |
            ------------------------------
        2 -> bottom right. resize the window covering 2,4 to just 2. The new window covers 4
            ------------------------------
            |              |             |
            |              |      b      |
            |              |             |
            |       a      x-------------|
            |              |             |
            |              |      c      |
            |              |             |
            ------------------------------
        3 -> bottom left. Resize the window covering 1,3 to just 1. The new window covers 3
            ------------------------------
            |              |             |
            |      a       |      b      |
            |              |             |
            |--------------x-------------|
            |              |             |
            |      d       |      c      |
            |              |             |
            ------------------------------  
        4 -> In the next workspace over, applying the same rules

Closing windows
    Closing a window does the obvious thing of resizing the others. There are a few interesting cases though
    When there is 1 window, either move to the workspace to the left (if there are windows there) or just go to desktop
    When there are 3 windows (a,b,c as in diagram above)
        if you close a ->, b should move to the left space, c should expand to fill the right
        if you close b -> c should expand to fill the right
    When there are 4 windows
        if you close a -> d should fill the left space
        if you close b -> d should move to fill the top right, a should fill the left space
        if you close c -> d should move to fill the bottom left, a should fill the left space

Resizing quadrants
    For each workspace, the splitting point can be moved around
    Each time it is moved, update the 4 window dimensions eg
        ------------------------------
        |                     |      |
        |          1          |  2   |
        |                     |      |
        |                     |      |
        |---------------------x------|
        |          3          |  4   |
        |                     |      |
        ------------------------------  

Swapping windows
    Windows can be swapped, both within a workspace and across workspaces
    Ideally a key combo would bring up a flashing highlight box that can be moved with arrow keys (and can be moved between workspaces) and releasing the key combo performs the swap

Fullscreening
    You can fullscreen a window ... which moves it to its own workspace?
    Or moves the other windows to their own workspace?
    not sure

Focus
    You can cycle focus, which just goes through the windows in order
    Clicking a window also focuses it
    There is a thin border around each window, which changes to a highlight colour when focused. The same as the swap-window highlight box
