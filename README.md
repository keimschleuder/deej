# In this fork
  I want to add the possibility to add a 7-segment display and a normal display via I²C. This is all work in progress.

  The Repo I forked added Button support. Those Buttons must be defined in config with int values. The key values can be found here: https://github.com/micmonay/keybd_event/blob/master/keybd_windows.go (make sure to convert hex values to int)

  Be sure to visit the [original repository](https://github.com/omriharel/deej)

  And the [Button Fork](https://github.com/Miodec/deej)

# How to

 - Upload the Arduino code to your board (be sure to change the pin definitions).
 - Download the `deej-release.exe` file and `config.yaml` from the [release section](https://github.com/Miodec/deej/releases/tag/compile) (`deej-dev` will show a debug console window when launched)
 - If you run into any issues, feel free to ask in the [Deej Discord Server](https://discord.gg/nf88NJu)

# Case files from Miodec

Case files available in the [/models](https://github.com/Miodec/deej/tree/master/models) directory

# Build from Miodec

Mini build log: https://imgur.com/a/baIDppz

![Finished build](https://i.imgur.com/neM2xle.jpg)
![Finished build2](https://i.imgur.com/moRmNFJ.jpg)

# Builds from omriharel and his community

  One possibility to build. With some example function assigned. It is possible to assign multiple apps to one slider within the `config.yaml`. Some other possibilities are the `master`, `mic`, `deej.current` and `deej.unmapped`. 
  
  Where `deej.current` controls only the current window and `deej.unmapped` controls everything, which is not mapped to a slider.

![Annotated Build](/assets/build-3d-annotated.png)

  The Scematic is also available here. But for detailed instructions visit [omriharels repository](https://github.com/omriharel/deej?tab=readme-ov-file#slider-mapping-configuration)

![Scematic](/assets/schematic.png)

  Builds from omriharels community are located in [this folder](/assets/community-builds/)

  Here you can find some of my personal favorite ones:

![bgrier](/assets/community-builds/bgrier.jpg)
![dimitar](/assets/community-builds/dimitar.jpg)
![ginjah](/assets/community-builds/ginjah.jpg)
