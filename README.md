# In this fork
  I want to add the possibility to add a 7-segment display and a normal display via I²C. This is all work in progress.

  The Repo I forked added Button support. Those Buttons must be defined in config with int values. The key values can be found here: https://github.com/micmonay/keybd_event/blob/master/keybd_windows.go (make sure to convert hex values to int)

  Be sure to visit the [original repository](https://github.com/omriharel/deej)

  And the [Button Fork](https://github.com/Miodec/deej)

# Installing

## Mute Current Window

I use [this programm to mute the current window](https://github.com/tfourj/MuteActiveWindow). Thy have detailled instuctions on how to install it. Make shure to set the hotkey to F13.
Also make it [run on startup](https://stackoverflow.com/questions/41723490/how-to-build-ahk-scripts-automatically-on-startup).

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
